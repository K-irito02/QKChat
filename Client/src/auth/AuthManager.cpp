#include "AuthManager.h"
#include "../utils/Logger.h"
#include "../utils/Validator.h"
#include "../utils/Crypto.h"
#include <QJsonDocument>
#include <QMutexLocker>
#include <QThread>
#include <QCoreApplication>

// 静态成员初始化
AuthManager* AuthManager::s_instance = nullptr;
QMutex AuthManager::s_mutex;

AuthManager::AuthManager(QObject *parent)
    : QObject(parent)
    , _networkClient(nullptr)
    , _sessionManager(nullptr)
    , _serverPort(0)
    , _useTLS(false)  // 暂时禁用SSL
    , _authState(Idle)
{
    // 创建网络客户端
    _networkClient = new NetworkClient(this);
    
    // 获取会话管理器实例
    _sessionManager = SessionManager::instance();
    
    // 连接网络客户端信号（使用队列连接确保线程安全）
    connect(_networkClient, &NetworkClient::connectionStateChanged,
            this, &AuthManager::onNetworkConnectionStateChanged, Qt::QueuedConnection);
    connect(_networkClient, &NetworkClient::loginResponse,
            this, &AuthManager::onLoginResponse, Qt::QueuedConnection);
    connect(_networkClient, &NetworkClient::registerResponse,
            this, &AuthManager::onRegisterResponse, Qt::QueuedConnection);
    connect(_networkClient, &NetworkClient::verificationCodeResponse,
            this, &AuthManager::onVerificationCodeResponse, Qt::QueuedConnection);
    connect(_networkClient, &NetworkClient::networkError,
            this, &AuthManager::onNetworkError, Qt::QueuedConnection);
    
    // 连接会话管理器信号
    connect(_sessionManager, &SessionManager::loginStateChanged,
            this, &AuthManager::loginStateChanged);
    connect(_sessionManager, &SessionManager::currentUserChanged,
            this, &AuthManager::currentUserChanged);
    connect(_sessionManager, &SessionManager::sessionExpired,
            this, &AuthManager::onSessionExpired);
    connect(_sessionManager, &SessionManager::autoLoginRequested,
            this, &AuthManager::onAutoLoginRequested);
}

AuthManager::~AuthManager()
{
    disconnectFromServer();
}

AuthManager* AuthManager::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new AuthManager();
            // 确保在主线程中创建
            if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
                LOG_WARNING("AuthManager instance created in non-main thread");
            }
        }
    }
    return s_instance;
}

bool AuthManager::initialize(const QString &serverHost, quint16 serverPort, bool useTLS)
{
    _serverHost = serverHost;
    _serverPort = serverPort;
    _useTLS = false;  // 强制禁用SSL
    
    LOG_INFO(QString("AuthManager initialized with server %1:%2 (TLS: disabled)")
             .arg(serverHost).arg(serverPort));
    
    return true;
}

bool AuthManager::connectToServer()
{
    LOG_INFO(QString("AuthManager::connectToServer() called - Host: %1, Port: %2, TLS: disabled")
             .arg(_serverHost).arg(_serverPort));

    if (_serverHost.isEmpty() || _serverPort == 0) {
        LOG_ERROR("Server configuration not set");
        return false;
    }

    if (_networkClient->isConnected()) {
        LOG_WARNING("Already connected to server");
        return true;
    }

    LOG_INFO("Setting auth state to Connecting and calling NetworkClient::connectToServer");
    setAuthState(Connecting);
    bool result = _networkClient->connectToServer(_serverHost, _serverPort, false);  // 强制禁用SSL
    LOG_INFO(QString("NetworkClient::connectToServer returned: %1").arg(result ? "true" : "false"));
    return result;
}

void AuthManager::disconnectFromServer()
{
    _networkClient->disconnectFromServer();
    setAuthState(Idle);
}

bool AuthManager::login(const QString &username, const QString &password, bool rememberMe)
{
    // 验证输入数据
    auto validation = validateLoginData(username, password);
    if (!validation.first) {
        emit loginFailed(validation.second);
        return false;
    }
    
    if (!_networkClient->isConnected()) {
        emit loginFailed("未连接到服务器");
        return false;
    }
    
    if (_authState != Idle) {
        emit loginFailed("正在处理其他请求");
        return false;
    }
    
    setAuthState(LoggingIn);
    
    // 发送登录请求 - 发送原始密码，让服务器使用存储的salt计算哈希
    QString requestId = _networkClient->sendLoginRequest(username, password, rememberMe);
    if (requestId.isEmpty()) {
        setAuthState(Idle);
        emit loginFailed("发送登录请求失败");
        return false;
    }
    
    _pendingRequests[requestId] = "login";
    
    // 保存登录信息（如果启用记住登录）
    // 注意：为了安全，我们保存密码哈希而不是原始密码
    if (rememberMe) {
        QString salt = Crypto::generateSalt();
        QString passwordHash = Crypto::hashPassword(password, salt);
        _sessionManager->saveLoginInfo(username, passwordHash);
    }
    
    LOG_INFO(QString("Login request sent for user: %1").arg(username));
    return true;
}

bool AuthManager::registerUser(const QString &username, const QString &email, 
                              const QString &password, const QString &verificationCode)
{
    // 验证输入数据
    auto validation = validateRegistrationData(username, email, password, password, verificationCode);
    if (!validation.first) {
        emit registerFailed(validation.second);
        return false;
    }
    
    if (!_networkClient->isConnected()) {
        emit registerFailed("未连接到服务器");
        return false;
    }
    
    if (_authState != Idle) {
        emit registerFailed("正在处理其他请求");
        return false;
    }
    
    setAuthState(Registering);
    
    // 发送注册请求 - 发送原始密码，让服务器生成salt和哈希
    QString requestId = _networkClient->sendRegisterRequest(username, email, password, verificationCode);
    
    if (requestId.isEmpty()) {
        setAuthState(Idle);
        emit registerFailed("发送注册请求失败");
        return false;
    }
    
    _pendingRequests[requestId] = "register";
    
    LOG_INFO(QString("Register request sent for user: %1, email: %2").arg(username).arg(email));
    return true;
}

bool AuthManager::sendVerificationCode(const QString &email)
{
    // 验证邮箱格式
    if (!Validator::isValidEmail(email)) {
        emit verificationCodeFailed("请输入有效的邮箱地址");
        return false;
    }
    
    if (!_networkClient->isConnected()) {
        emit verificationCodeFailed("未连接到服务器");
        return false;
    }
    
    if (_authState != Idle) {
        emit verificationCodeFailed("正在处理其他请求");
        return false;
    }
    
    setAuthState(SendingVerificationCode);
    
    // 发送验证码请求
    QString requestId = _networkClient->sendVerificationCodeRequest(email);
    
    if (requestId.isEmpty()) {
        setAuthState(Idle);
        emit verificationCodeFailed("发送验证码请求失败");
        return false;
    }
    
    _pendingRequests[requestId] = "verification_code";
    
    LOG_INFO(QString("Verification code request sent for email: %1").arg(email));
    return true;
}

void AuthManager::logout()
{
    if (!_sessionManager->isLoggedIn()) {
        return;
    }
    
    setAuthState(LoggingOut);
    
    LOG_INFO("User logging out");
    
    // 销毁会话
    _sessionManager->destroySession();
    
    setAuthState(Idle);
}

bool AuthManager::tryAutoLogin()
{
    return _sessionManager->tryAutoLogin();
}

bool AuthManager::isConnected() const
{
    return _networkClient->isConnected();
}

bool AuthManager::isLoggedIn() const
{
    return _sessionManager->isLoggedIn();
}

User* AuthManager::currentUser() const
{
    return _sessionManager->currentUser();
}

QPair<bool, QString> AuthManager::validateRegistrationData(const QString &username, const QString &email,
                                                           const QString &password, const QString &confirmPassword,
                                                           const QString &verificationCode)
{
    // 验证用户名
    QString usernameError = Validator::getUsernameValidationError(username);
    if (!usernameError.isEmpty()) {
        return qMakePair(false, usernameError);
    }
    
    // 验证邮箱
    QString emailError = Validator::getEmailValidationError(email);
    if (!emailError.isEmpty()) {
        return qMakePair(false, emailError);
    }
    
    // 验证密码
    QString passwordError = Validator::getPasswordValidationError(password);
    if (!passwordError.isEmpty()) {
        return qMakePair(false, passwordError);
    }
    
    // 验证确认密码
    if (password != confirmPassword) {
        return qMakePair(false, "两次输入的密码不一致");
    }
    
    // 验证验证码（如果提供）
    if (!verificationCode.isEmpty() && !Validator::isValidVerificationCode(verificationCode)) {
        return qMakePair(false, "请输入有效的验证码");
    }
    
    return qMakePair(true, "");
}

QPair<bool, QString> AuthManager::validateLoginData(const QString &username, const QString &password)
{
    if (username.trimmed().isEmpty()) {
        return qMakePair(false, "请输入用户名或邮箱");
    }

    if (password.isEmpty()) {
        return qMakePair(false, "请输入密码");
    }

    return qMakePair(true, "");
}

void AuthManager::onNetworkConnectionStateChanged(NetworkClient::ConnectionState state)
{
    bool connected = (state == NetworkClient::Connected);

    if (connected) {
        LOG_INFO("Connected to server");
        setAuthState(Idle);
    } else {
        LOG_INFO("Disconnected from server");
        if (_authState == Connecting) {
            setAuthState(Idle);
        }
    }

    emit connectionStateChanged(connected);
}

void AuthManager::onLoginResponse(const QString &requestId, const QJsonObject &response)
{
    if (!_pendingRequests.contains(requestId)) {
        return;
    }

    _pendingRequests.remove(requestId);
    setAuthState(Idle);

    AuthResponse* authResponse = processAuthResponse(response);
    
    LOG_INFO(QString("Processing login response: success=%1, message='%2', hasUser=%3, hasSessionToken=%4")
             .arg(authResponse->success())
             .arg(authResponse->message())
             .arg(authResponse->user() != nullptr)
             .arg(!authResponse->sessionToken().isEmpty()));

    if (authResponse->success()) {
        User* user = authResponse->user();
        QString sessionToken = authResponse->sessionToken();

        if (user && !sessionToken.isEmpty()) {
            // 创建会话
            bool rememberMe = _sessionManager->isRememberMeEnabled();
            if (_sessionManager->createSession(user, sessionToken, rememberMe)) {
                LOG_INFO(QString("Login successful for user: %1").arg(user->username()));
                
                emit loginSucceeded(user);
            } else {
                LOG_ERROR("Failed to create session after successful login");
                emit loginFailed("创建会话失败");
            }
        } else {
            LOG_ERROR("Invalid login response: missing user data or session token");

            emit loginFailed("服务器响应数据无效");
        }
    } else {
        LOG_WARNING(QString("Login failed: %1").arg(authResponse->message()));

        emit loginFailed(authResponse->message());
    }

    authResponse->deleteLater();
}

void AuthManager::onRegisterResponse(const QString &requestId, const QJsonObject &response)
{
    if (!_pendingRequests.contains(requestId)) {
        return;
    }

    _pendingRequests.remove(requestId);
    setAuthState(Idle);

    AuthResponse* authResponse = processAuthResponse(response);

    if (authResponse->success()) {
        User* user = authResponse->user();

        if (user) {
            LOG_INFO(QString("Registration successful for user: %1").arg(user->username()));
            emit registerSucceeded(user);
        } else {
            LOG_ERROR("Invalid register response: missing user data");
            emit registerFailed("服务器响应数据无效");
        }
    } else {
        LOG_WARNING(QString("Registration failed: %1").arg(authResponse->message()));
        emit registerFailed(authResponse->message());
    }

    authResponse->deleteLater();
}

void AuthManager::onVerificationCodeResponse(const QString &requestId, const QJsonObject &response)
{
    if (!_pendingRequests.contains(requestId)) {
        return;
    }

    _pendingRequests.remove(requestId);
    setAuthState(Idle);

    AuthResponse* authResponse = processAuthResponse(response);

    if (authResponse->success()) {
        LOG_INFO("Verification code sent successfully");
        emit verificationCodeSent();
    } else {
        LOG_WARNING(QString("Verification code failed: %1").arg(authResponse->message()));
        emit verificationCodeFailed(authResponse->message());
    }

    authResponse->deleteLater();
}

void AuthManager::onNetworkError(const QString &error)
{
    LOG_ERROR(QString("Network error: %1").arg(error));

    setAuthState(Idle);
    _pendingRequests.clear();

    emit networkError(error);
}

void AuthManager::onSessionExpired()
{
    LOG_WARNING("Session expired, user logged out");
    setAuthState(Idle);
}

void AuthManager::onAutoLoginRequested(const QString &username, const QString &passwordHash)
{
    if (!_networkClient->isConnected()) {
        // 如果未连接，先连接到服务器
        if (!connectToServer()) {
            LOG_ERROR("Failed to connect to server for auto login");
            return;
        }

        // 等待连接成功后再尝试登录
        QTimer::singleShot(2000, [this, username, passwordHash]() {
            if (_networkClient->isConnected()) {
                performAutoLogin(username, passwordHash);
            }
        });
    } else {
        performAutoLogin(username, passwordHash);
    }
}

void AuthManager::performAutoLogin(const QString &username, const QString &passwordHash)
{
    if (_authState != Idle) {
        return;
    }

    setAuthState(LoggingIn);

    QString requestId = _networkClient->sendLoginRequest(username, passwordHash, true);
    if (requestId.isEmpty()) {
        setAuthState(Idle);
        LOG_ERROR("Failed to send auto login request");
        return;
    }

    _pendingRequests[requestId] = "login";
    LOG_INFO("Auto login request sent");
}

void AuthManager::setAuthState(AuthState state)
{
    if (_authState != state) {
        _authState = state;
        emit loadingStateChanged(state != Idle);
    }
}

AuthResponse* AuthManager::processAuthResponse(const QJsonObject &response)
{
    return new AuthResponse(response, this);
}
