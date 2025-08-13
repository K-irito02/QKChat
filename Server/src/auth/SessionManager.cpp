#include "SessionManager.h"
#include "../config/ConfigManager.h"
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>

// 静态成员初始化
SessionManager* SessionManager::s_instance = nullptr;
QMutex SessionManager::s_mutex;

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , _redisClient(nullptr)
    , _cleanupTimer(nullptr)
    , _totalSessionsCreated(0)
    , _totalSessionsExpired(0)
    , _totalSessionsDestroyed(0)
    , _totalCacheHits(0)
    , _totalCacheMisses(0)
    , _defaultTimeout(604800)        // 7天
    , _rememberMeTimeout(2592000)    // 30天
    , _activityUpdateInterval(1800)  // 30分钟
    , _maxSessionsPerUser(5)
    , _cleanupInterval(3600)         // 1小时
    , _slidingWindow(true)
    , _multiDeviceSupport(true)
{
    // 获取Redis客户端实例
    _redisClient = RedisClient::instance();
    
    // 创建清理定时器
    _cleanupTimer = new QTimer(this);
    _cleanupTimer->setSingleShot(false);
    connect(_cleanupTimer, &QTimer::timeout, this, &SessionManager::onCleanupTimer);
    
    // 加载配置
    loadConfiguration();
    
    // 启动清理定时器
    _cleanupTimer->start(_cleanupInterval * 1000);
    
    // LOG_INFO removed
}

SessionManager::~SessionManager()
{
    if (_cleanupTimer) {
        _cleanupTimer->stop();
    }
}

SessionManager* SessionManager::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new SessionManager();
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

void SessionManager::loadConfiguration()
{
    ConfigManager* config = ConfigManager::instance();
    if (!config) {
        LOG_WARNING("ConfigManager not available, using default session configuration");
        return;
    }
    
    QJsonObject sessionConfig = config->getValue("security.session").toJsonObject();
    
    if (!sessionConfig.isEmpty()) {
        _defaultTimeout = sessionConfig["default_timeout"].toInt(_defaultTimeout);
        _rememberMeTimeout = sessionConfig["remember_me_timeout"].toInt(_rememberMeTimeout);
        _activityUpdateInterval = sessionConfig["activity_update_interval"].toInt(_activityUpdateInterval);
        _maxSessionsPerUser = sessionConfig["max_sessions_per_user"].toInt(_maxSessionsPerUser);
        _cleanupInterval = sessionConfig["cleanup_interval"].toInt(_cleanupInterval);
        _slidingWindow = sessionConfig["sliding_window"].toBool(_slidingWindow);
        _multiDeviceSupport = sessionConfig["multi_device_support"].toBool(_multiDeviceSupport);
    }
    
    LOG_INFO(QString("Session configuration loaded - Default: %1s, RememberMe: %2s, MaxSessions: %3")
             .arg(_defaultTimeout).arg(_rememberMeTimeout).arg(_maxSessionsPerUser));
}

bool SessionManager::createSession(qint64 userId, const QString& sessionToken, const QString& deviceId,
                                 const QString& clientId, const QString& ipAddress, bool rememberMe)
{
    if (userId <= 0 || sessionToken.isEmpty()) {
        LOG_ERROR("Invalid parameters for session creation");
        return false;
    }
    
    // 检查用户会话数量限制
    if (!_multiDeviceSupport && isUserSessionLimitExceeded(userId)) {
        LOG_WARNING(QString("User %1 session limit exceeded").arg(userId));
        return false;
    }
    
    // 创建会话信息
    SessionInfo sessionInfo(userId, deviceId, clientId, ipAddress);
    sessionInfo.expiresAt = calculateExpiryTime(rememberMe);
    
    // 序列化会话数据
    QString sessionData = serializeSessionData(sessionInfo);
    
    // 存储到Redis
    QString sessionKey = QString("session:%1").arg(sessionToken);
    int expireSeconds = rememberMe ? _rememberMeTimeout : _defaultTimeout;
    
    RedisClient::Result result = _redisClient->set(sessionKey, sessionData, expireSeconds);
    
    if (result == RedisClient::Success) {
        QMutexLocker locker(&_statsMutex);
        _totalSessionsCreated++;
        
        LOG_INFO(QString("Session created for user %1, device %2, expires at %3")
                 .arg(userId).arg(deviceId).arg(sessionInfo.expiresAt.toString()));
        
        emit sessionCreated(sessionToken, userId);
        return true;
    } else {
        LOG_ERROR(QString("Failed to create session for user %1: Redis error %2")
                  .arg(userId).arg(static_cast<int>(result)));
        return false;
    }
}

SessionManager::SessionInfo SessionManager::validateSession(const QString& sessionToken)
{
    if (sessionToken.isEmpty()) {
        QMutexLocker locker(&_statsMutex);
        _totalCacheMisses++;
        return SessionInfo();
    }
    
    QString sessionKey = QString("session:%1").arg(sessionToken);
    QString sessionData;
    
    RedisClient::Result result = _redisClient->get(sessionKey, sessionData);
    
    if (result != RedisClient::Success || sessionData.isEmpty()) {
        QMutexLocker locker(&_statsMutex);
        _totalCacheMisses++;
        return SessionInfo();
    }
    
    // 解析会话数据
    SessionInfo sessionInfo = parseSessionData(sessionData);
    
    if (!sessionInfo.isValid) {
        QMutexLocker locker(&_statsMutex);
        _totalCacheMisses++;
        return SessionInfo();
    }
    
    // 检查是否过期
    if (sessionInfo.isExpired()) {
        // 删除过期会话
        _redisClient->del(sessionKey);
        
        QMutexLocker locker(&_statsMutex);
        _totalSessionsExpired++;
        _totalCacheMisses++;
        
        LOG_INFO(QString("Session expired for user %1").arg(sessionInfo.userId));
        emit sessionExpired(sessionToken, sessionInfo.userId);
        
        return SessionInfo();
    }
    
    QMutexLocker locker(&_statsMutex);
    _totalCacheHits++;
    
    return sessionInfo;
}

bool SessionManager::updateSessionActivity(const QString& sessionToken)
{
    if (sessionToken.isEmpty() || !_slidingWindow) {
        return false;
    }
    
    QString sessionKey = QString("session:%1").arg(sessionToken);
    QString sessionData;
    
    RedisClient::Result result = _redisClient->get(sessionKey, sessionData);
    if (result != RedisClient::Success || sessionData.isEmpty()) {
        return false;
    }
    
    // 解析会话数据
    SessionInfo sessionInfo = parseSessionData(sessionData);
    if (!sessionInfo.isValid || sessionInfo.isExpired()) {
        return false;
    }
    
    // 更新活动时间
    sessionInfo.updateActivity();
    
    // 重新计算过期时间（滑动窗口）
    sessionInfo.expiresAt = calculateExpiryTime(false); // 使用默认超时时间
    
    // 重新存储会话数据
    QString newSessionData = serializeSessionData(sessionInfo);
    int expireSeconds = _defaultTimeout;
    
    result = _redisClient->set(sessionKey, newSessionData, expireSeconds);
    
    if (result == RedisClient::Success) {
        LOG_DEBUG(QString("Session activity updated for user %1").arg(sessionInfo.userId));
        return true;
    } else {
        LOG_ERROR(QString("Failed to update session activity for user %1").arg(sessionInfo.userId));
        return false;
    }
}

bool SessionManager::destroySession(const QString& sessionToken)
{
    if (sessionToken.isEmpty()) {
        return false;
    }
    
    QString sessionKey = QString("session:%1").arg(sessionToken);
    
    // 获取会话信息用于日志记录
    QString sessionData;
    SessionInfo sessionInfo;
    if (_redisClient->get(sessionKey, sessionData) == RedisClient::Success) {
        sessionInfo = parseSessionData(sessionData);
    }
    
    // 删除会话
    RedisClient::Result result = _redisClient->del(sessionKey);
    
    if (result == RedisClient::Success) {
        QMutexLocker locker(&_statsMutex);
        _totalSessionsDestroyed++;
        
        if (sessionInfo.isValid) {
            LOG_INFO(QString("Session destroyed for user %1").arg(sessionInfo.userId));
            emit sessionDestroyed(sessionToken, sessionInfo.userId);
        }
        
        return true;
    } else {
        LOG_ERROR(QString("Failed to destroy session: Redis error %1").arg(static_cast<int>(result)));
        return false;
    }
}

int SessionManager::destroyUserSessions(qint64 userId)
{
    if (userId <= 0) {
        return 0;
    }
    
    QStringList userSessions = getUserActiveSessions(userId);
    int destroyedCount = 0;
    
    for (const QString& sessionToken : userSessions) {
        if (destroySession(sessionToken)) {
            destroyedCount++;
        }
    }
    
    LOG_INFO(QString("Destroyed %1 sessions for user %2").arg(destroyedCount).arg(userId));
    return destroyedCount;
}

QStringList SessionManager::getUserActiveSessions(qint64 userId)
{
    QStringList activeSessions;
    QString pattern = "session:*";
    
    // 注意：这里需要Redis的KEYS命令，在生产环境中应该谨慎使用
    // 可以考虑使用SCAN命令或者维护一个用户会话索引
    QStringList keys = _redisClient->keys(pattern);
    
    for (const QString& key : keys) {
        QString sessionData;
        if (_redisClient->get(key, sessionData) == RedisClient::Success) {
            SessionInfo sessionInfo = parseSessionData(sessionData);
            if (sessionInfo.isValid && sessionInfo.userId == userId && !sessionInfo.isExpired()) {
                QString sessionToken = extractSessionTokenFromKey(key);
                activeSessions.append(sessionToken);
            }
        }
    }
    
    return activeSessions;
}

bool SessionManager::isUserSessionLimitExceeded(qint64 userId)
{
    if (_maxSessionsPerUser <= 0) {
        return false; // 无限制
    }
    
    QStringList activeSessions = getUserActiveSessions(userId);
    return activeSessions.size() >= _maxSessionsPerUser;
}

int SessionManager::cleanupExpiredSessions()
{
    int cleanedCount = 0;
    QString pattern = "session:*";
    
    QStringList keys = _redisClient->keys(pattern);
    
    for (const QString& key : keys) {
        QString sessionData;
        if (_redisClient->get(key, sessionData) == RedisClient::Success) {
            SessionInfo sessionInfo = parseSessionData(sessionData);
            if (sessionInfo.isValid && sessionInfo.isExpired()) {
                QString sessionToken = extractSessionTokenFromKey(key);
                if (destroySession(sessionToken)) {
                    cleanedCount++;
                }
            }
        }
    }
    
    if (cleanedCount > 0) {
        LOG_INFO(QString("Cleaned up %1 expired sessions").arg(cleanedCount));
    }
    
    return cleanedCount;
}

QString SessionManager::getSessionStats() const
{
    QMutexLocker locker(&_statsMutex);
    
    return QString("Sessions - Created: %1, Expired: %2, Destroyed: %3, Cache Hits: %4, Cache Misses: %5")
           .arg(_totalSessionsCreated)
           .arg(_totalSessionsExpired)
           .arg(_totalSessionsDestroyed)
           .arg(_totalCacheHits)
           .arg(_totalCacheMisses);
}

void SessionManager::onCleanupTimer()
{
    // 在定时器回调中执行清理，避免阻塞
    QTimer::singleShot(0, this, [this]() {
        cleanupExpiredSessions();
    });
}

QString SessionManager::extractSessionTokenFromKey(const QString& redisKey)
{
    // 从 "session:token" 格式中提取 token
    if (redisKey.startsWith("session:")) {
        return redisKey.mid(8); // 跳过 "session:" 前缀
    }
    return redisKey;
}

SessionManager::SessionInfo SessionManager::parseSessionData(const QString& sessionData)
{
    SessionInfo sessionInfo;
    
    try {
        QStringList parts = sessionData.split(':');
        if (parts.size() >= 6) {
            sessionInfo.userId = parts[0].toLongLong();
            sessionInfo.deviceId = parts[1];
            sessionInfo.createdAt = QDateTime::fromSecsSinceEpoch(parts[2].toLongLong());
            sessionInfo.lastActivity = QDateTime::fromSecsSinceEpoch(parts[3].toLongLong());
            sessionInfo.expiresAt = QDateTime::fromSecsSinceEpoch(parts[4].toLongLong());
            sessionInfo.clientId = parts[5];
            sessionInfo.ipAddress = parts.size() > 6 ? parts[6] : QString();
            sessionInfo.isValid = true;
        }
    } catch (...) {
        LOG_ERROR("Failed to parse session data");
        sessionInfo.isValid = false;
    }
    
    return sessionInfo;
}

QString SessionManager::serializeSessionData(const SessionInfo& sessionInfo)
{
    return QString("%1:%2:%3:%4:%5:%6:%7")
           .arg(sessionInfo.userId)
           .arg(sessionInfo.deviceId)
           .arg(sessionInfo.createdAt.toSecsSinceEpoch())
           .arg(sessionInfo.lastActivity.toSecsSinceEpoch())
           .arg(sessionInfo.expiresAt.toSecsSinceEpoch())
           .arg(sessionInfo.clientId)
           .arg(sessionInfo.ipAddress);
}

QDateTime SessionManager::calculateExpiryTime(bool rememberMe)
{
    int timeoutSeconds = rememberMe ? _rememberMeTimeout : _defaultTimeout;
    return QDateTime::currentDateTime().addSecs(timeoutSeconds);
}
