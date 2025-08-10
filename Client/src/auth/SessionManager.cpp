#include "SessionManager.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QThread>
#include <QMetaObject>

// 静态成员初始化
SessionManager* SessionManager::s_instance = nullptr;
QMutex SessionManager::s_mutex;

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , _isLoggedIn(false)
    , _currentUser(nullptr)
    , _rememberMe(false)
    , _sessionTimeout(3600) // 默认1小时
{
    // 创建定时器
    _sessionTimer = new QTimer(this);
    _sessionTimer->setSingleShot(true);
    
    _expiringWarningTimer = new QTimer(this);
    _expiringWarningTimer->setSingleShot(true);
    
    // 连接信号（使用队列连接确保线程安全）
    connect(_sessionTimer, &QTimer::timeout, this, &SessionManager::onSessionTimeout, Qt::QueuedConnection);
    connect(_expiringWarningTimer, &QTimer::timeout, this, &SessionManager::onSessionExpiringWarning, Qt::QueuedConnection);
    
    // 创建设置对象
    _settings = new QSettings("QKChat", "Client", this);
    
    // 加载设置
    loadSettings();
}

SessionManager::~SessionManager()
{
    stopSessionTimer();
    saveSettings();
    destroySession();
}

SessionManager* SessionManager::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new SessionManager();
            // 确保在主线程中创建
            if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
                LOG_WARNING("SessionManager instance created in non-main thread");
            }
        }
    }
    return s_instance;
}

void SessionManager::cleanup()
{
    QMutexLocker locker(&s_mutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool SessionManager::createSession(User* user, const QString &sessionToken, bool rememberMe)
{
    if (!user || sessionToken.isEmpty()) {
        LOG_ERROR("Invalid parameters for creating session");
        return false;
    }
    
    // 确保在主线程中执行
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        LOG_ERROR("createSession called from non-main thread");
        // 使用QueuedConnection避免死锁，但这意味着无法返回结果
        // 在非主线程中调用时，应该重新设计调用方式
        QMetaObject::invokeMethod(this, [this, user, sessionToken, rememberMe]() {
            createSession(user, sessionToken, rememberMe);
        }, Qt::QueuedConnection);

        // 返回false表示需要异步处理
        LOG_WARNING("Session creation queued for main thread execution");
        return false;
    }
    
    // 销毁现有会话
    destroySession();
    
    // 创建新用户对象的副本
    _currentUser = new User(this);
    _currentUser->copyFrom(*user);
    
    _sessionToken = sessionToken;
    _loginTime = QDateTime::currentDateTime();
    _rememberMe = rememberMe;
    
    setLoggedIn(true);
    
    // 启动会话定时器
    startSessionTimer();
    
    // 保存登录信息（如果启用记住登录）
    if (_rememberMe) {
        // 注意：这里应该保存用户名，密码哈希应该在登录时保存
        _settings->setValue("login/username", _currentUser->username());
        _settings->setValue("login/remember_me", true);
    } else {
        clearSavedLoginInfo();
    }
    
    // 保存用户主题设置
    _settings->setValue("user/theme", _currentUser->theme());
    
    LOG_INFO(QString("Session created for user: %1").arg(_currentUser->username()));
    
    emit currentUserChanged();
    emit sessionTokenChanged();
    
    return true;
}

void SessionManager::destroySession()
{
    if (!_isLoggedIn) {
        return;
    }
    
    LOG_INFO("Destroying session");
    
    // 停止定时器
    stopSessionTimer();
    
    // 清理用户数据
    if (_currentUser) {
        _currentUser->deleteLater();
        _currentUser = nullptr;
    }
    
    _sessionToken.clear();
    _loginTime = QDateTime();
    
    setLoggedIn(false);
    
    emit currentUserChanged();
    emit sessionTokenChanged();
}

bool SessionManager::isSessionValid() const
{
    if (!_isLoggedIn || _sessionToken.isEmpty()) {
        return false;
    }
    
    // 检查会话是否过期
    if (_loginTime.isValid()) {
        qint64 elapsedSeconds = _loginTime.secsTo(QDateTime::currentDateTime());
        return elapsedSeconds < _sessionTimeout;
    }
    
    return true;
}

void SessionManager::refreshSessionToken(const QString &newToken)
{
    if (newToken.isEmpty()) {
        LOG_WARNING("Attempted to refresh with empty token");
        return;
    }
    
    _sessionToken = newToken;
    _loginTime = QDateTime::currentDateTime(); // 重置登录时间
    
    // 重启会话定时器
    startSessionTimer();
    
    LOG_INFO("Session token refreshed");
    emit sessionTokenChanged();
}

void SessionManager::updateUserInfo(User* user)
{
    if (!user || !_currentUser) {
        return;
    }
    
    _currentUser->copyFrom(*user);
    
    // 更新保存的主题设置
    _settings->setValue("user/theme", _currentUser->theme());
    
    LOG_INFO("User information updated");
    emit currentUserChanged();
}

void SessionManager::setSessionTimeout(int timeout)
{
    _sessionTimeout = timeout;
    
    if (_isLoggedIn) {
        startSessionTimer(); // 重启定时器
    }
}

QPair<QString, QString> SessionManager::getSavedLoginInfo() const
{
    QString username = _settings->value("login/username", "").toString();
    QString passwordHash = _settings->value("login/password_hash", "").toString();
    bool rememberMe = _settings->value("login/remember_me", false).toBool();
    
    if (rememberMe && !username.isEmpty() && !passwordHash.isEmpty()) {
        return qMakePair(username, passwordHash);
    }
    
    return qMakePair(QString(), QString());
}

void SessionManager::saveLoginInfo(const QString &username, const QString &passwordHash)
{
    if (_rememberMe) {
        _settings->setValue("login/username", username);
        _settings->setValue("login/password_hash", passwordHash);
        _settings->setValue("login/remember_me", true);
        LOG_INFO("Login information saved");
    }
}

void SessionManager::clearSavedLoginInfo()
{
    _settings->remove("login/username");
    _settings->remove("login/password_hash");
    _settings->setValue("login/remember_me", false);
    LOG_INFO("Saved login information cleared");
}

bool SessionManager::tryAutoLogin()
{
    auto loginInfo = getSavedLoginInfo();
    if (!loginInfo.first.isEmpty() && !loginInfo.second.isEmpty()) {
        LOG_INFO("Auto login requested for user: " + loginInfo.first);
        
        // 使用QTimer::singleShot延迟发送信号，避免在构造过程中的循环调用
        QTimer::singleShot(100, this, [this, loginInfo]() {
            emit autoLoginRequested(loginInfo.first, loginInfo.second);
        });
        
        return true;
    }
    
    return false;
}

void SessionManager::onSessionTimeout()
{
    LOG_WARNING("Session expired");
    emit sessionExpired();
    destroySession();
}

void SessionManager::onSessionExpiringWarning()
{
    int remainingSeconds = _sessionTimeout - _loginTime.secsTo(QDateTime::currentDateTime());
    LOG_INFO(QString("Session expiring in %1 seconds").arg(remainingSeconds));
    emit sessionExpiring(remainingSeconds);
}

void SessionManager::setLoggedIn(bool loggedIn)
{
    if (_isLoggedIn != loggedIn) {
        _isLoggedIn = loggedIn;
        emit loginStateChanged(loggedIn);
    }
}

void SessionManager::startSessionTimer()
{
    stopSessionTimer();
    
    if (_sessionTimeout > 0) {
        // 启动会话超时定时器
        _sessionTimer->start(_sessionTimeout * 1000);
        
        // 启动过期警告定时器（提前5分钟警告）
        int warningTime = qMax(0, (_sessionTimeout - 300) * 1000);
        if (warningTime > 0) {
            _expiringWarningTimer->start(warningTime);
        }
    }
}

void SessionManager::stopSessionTimer()
{
    _sessionTimer->stop();
    _expiringWarningTimer->stop();
}

void SessionManager::loadSettings()
{
    _sessionTimeout = _settings->value("session/timeout", 3600).toInt();
}

void SessionManager::saveSettings()
{
    _settings->setValue("session/timeout", _sessionTimeout);
    _settings->sync();
}
