#include "AuthManager.h"
#include "../utils/Logger.h"
#include "../utils/Validator.h"
#include "../utils/Crypto.h"
#include "../models/User.h"
#include "../chat/ChatNetworkClient.h"
#include "../models/RecentContactsManager.h"
#include "../models/ChatMessageManager.h"
#include <QJsonDocument>
#include <QMutexLocker>
#include <QThread>
#include <QCoreApplication>
#include <QTimer>

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
    // 使用单例网络客户端实例
    _networkClient = NetworkClient::instance();
    
    // 获取会话管理器实例
    _sessionManager = SessionManager::instance();
    
    // 初始化认证状态
    _authState = Idle;
    
    // 连接网络客户端信号
    connect(_networkClient, &NetworkClient::connectionStateChanged, 
            this, &AuthManager::onNetworkConnectionStateChanged);
    connect(_networkClient, &NetworkClient::loginResponse, 
            this, &AuthManager::onLoginResponse);
    connect(_networkClient, &NetworkClient::registerResponse, 
            this, &AuthManager::onRegisterResponse);
    connect(_networkClient, &NetworkClient::verificationCodeResponse, 
            this, &AuthManager::onVerificationCodeResponse);
    connect(_networkClient, &NetworkClient::usernameAvailabilityResponse, 
            this, &AuthManager::onUsernameAvailabilityResponse);
    connect(_networkClient, &NetworkClient::emailAvailabilityResponse, 
            this, &AuthManager::onEmailAvailabilityResponse);
    connect(_networkClient, &NetworkClient::networkError, 
            this, &AuthManager::onNetworkError);
    
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
    try {
        if (_networkClient) {
            disconnectFromServer();
            // 不再删除_networkClient，因为它是单例
            _networkClient = nullptr;
        }
    } catch (...) {
        // 忽略析构错误
    }
}

AuthManager* AuthManager::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new AuthManager();

        }
    }
    return s_instance;
}

void AuthManager::cleanup()
{
    QMutexLocker locker(&s_mutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool AuthManager::initialize(const QString &serverHost, quint16 serverPort, bool useTLS)
{
    _serverHost = serverHost;
    _serverPort = serverPort;
    _useTLS = false;  // 强制禁用SSL
    

    
    return true;
}

bool AuthManager::connectToServer()
{


    if (_serverHost.isEmpty() || _serverPort == 0) {
        LOG_ERROR("Server configuration not set");
        return false;
    }

    if (_networkClient->isConnected()) {
        LOG_WARNING("Already connected to server");
        return true;
    }

    setAuthState(Connecting);
    bool result = _networkClient->connectToServer(_serverHost, _serverPort, false);  // 强制禁用SSL
    return result;
}

void AuthManager::disconnectFromServer()
{
    if (_networkClient) {
        _networkClient->disconnectFromServer();
    }
    
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
    
    // 发送登录请求
    QString requestId = _networkClient->sendLoginRequest(username, password, rememberMe);
    
    if (requestId.isEmpty()) {
        setAuthState(Idle);
        emit loginFailed("发送登录请求失败");
        return false;
    }
    

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
    

    return true;
}

bool AuthManager::checkUsernameAvailability(const QString &username)
{
    // 验证用户名格式
    if (!Validator::isValidUsername(username)) {
        // 格式无效时不发出信号，让UI自己处理
        return false;
    }
    
    if (!_networkClient->isConnected()) {
        // 网络未连接时不发出信号，让UI自己处理
        return false;
    }
    
    // 发送检查用户名可用性请求
    QString requestId = _networkClient->sendCheckUsernameRequest(username);
    
    if (requestId.isEmpty()) {
        // 请求发送失败时不发出信号，让UI自己处理
        return false;
    }
    

    return true;
}

bool AuthManager::checkEmailAvailability(const QString &email)
{
    // 验证邮箱格式
    if (!Validator::isValidEmail(email)) {
        // 格式无效时不发出信号，让UI自己处理
        return false;
    }
    
    if (!_networkClient->isConnected()) {
        // 网络未连接时不发出信号，让UI自己处理
        return false;
    }
    
    // 发送检查邮箱可用性请求
    QString requestId = _networkClient->sendCheckEmailRequest(email);
    
    if (requestId.isEmpty()) {
        // 请求发送失败时不发出信号，让UI自己处理
        return false;
    }
    

    return true;
}

void AuthManager::logout()
{
    if (!_sessionManager->isLoggedIn()) {
        return;
    }
    
    setAuthState(LoggingOut);
    

    
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
    
        setAuthState(Idle);
    } else {
    
        if (_authState == Connecting) {
            setAuthState(Idle);
        }
    }

    emit connectionStateChanged(connected);
}

void AuthManager::onLoginResponse(const QString &requestId, const QJsonObject &response)
{
    AuthResponse* authResponse = processAuthResponse(response);

    if (authResponse->success()) {
        // 登录成功
        QJsonObject userData = response["user"].toObject();
        QString sessionToken = response["session_token"].toString();
        qint64 expiresIn = response["expires_in"].toVariant().toLongLong();
        QString clientId = response["client_id"].toString();  // 获取服务器分配的client_id
        
        // 创建User对象
        User* user = new User(userData, this);
        
        // 创建会话
        _sessionManager->createSession(user, sessionToken, true);
        
        // 更新NetworkClient的认证状态
        if (_networkClient) {
            // 传递用户ID给NetworkClient
            _networkClient->setAuthenticated(true, sessionToken, user->id());
            
            // 保存服务器分配的client_id
            if (!clientId.isEmpty()) {
                _networkClient->setClientId(clientId);
            
            } else {
                LOG_WARNING("Server did not provide client_id in login response");
            }
        } else {
            LOG_ERROR("NetworkClient is null, cannot update authentication state");
        }
        
        // 更新认证状态
        setAuthState(Idle);
        
        // 发送登录成功信号
        emit loginSucceeded(user);

        // 登录成功后只清理聊天消息，保留最近联系人数据
        QTimer::singleShot(500, [this]() {
            auto chatManager = ChatMessageManager::instance();
            if (chatManager) {
                chatManager->clearMessages();
            }
        });

        // 登录成功后加载最近联系人数据
        QTimer::singleShot(200, [this]() {
            auto recentManager = RecentContactsManager::instance();
            if (recentManager) {
                recentManager->loadDataAfterLogin();
            }
        });

        // 登录成功后立即初始化ChatNetworkClient并发送好友列表请求
        ChatNetworkClient* chatClient = ChatNetworkClient::instance();
        if (chatClient && chatClient->initialize()) {
            // 延迟一小段时间确保网络连接稳定
            QTimer::singleShot(100, [chatClient]() {
                chatClient->getFriendList();
                chatClient->getFriendGroups();
                // 获取离线消息
                chatClient->getOfflineMessages();
            });
        }
        
    } else {
        // 登录失败
        setAuthState(Idle);
        emit loginFailed(authResponse->message());
        
        LOG_WARNING(QString("User login failed: %1").arg(authResponse->message()));
    }

    authResponse->deleteLater();
}

void AuthManager::onRegisterResponse(const QString &requestId, const QJsonObject &response)
{


    AuthResponse* authResponse = processAuthResponse(response);

    if (authResponse->success()) {
        // 注册成功
        QJsonObject userData = response["user"].toObject();
        
        // 创建User对象
        User* user = new User(userData, this);
        
        // 更新认证状态
        setAuthState(Idle);
        
        // 发送注册成功信号
        emit registerSucceeded(user);
        
    
    } else {
        // 注册失败
        setAuthState(Idle);
        
        // 获取用户友好的错误消息
        QString errorMessage = getRegisterErrorMessage(authResponse->errorCode(), authResponse->message());
        emit registerFailed(errorMessage);
        
        LOG_WARNING(QString("User registration failed: %1").arg(errorMessage));
    }

    authResponse->deleteLater();
}

void AuthManager::onVerificationCodeResponse(const QString &requestId, const QJsonObject &response)
{

    
    setAuthState(Idle);
    
    AuthResponse* authResponse = processAuthResponse(response);
    
    if (authResponse->success()) {
        emit verificationCodeSent();
    } else {
        LOG_WARNING(QString("Verification code failed: %1").arg(authResponse->message()));
        
        // 根据错误类型提供更友好的提示
        QString userFriendlyMessage = getVerificationCodeErrorMessage(authResponse->errorCode(), authResponse->message());
        emit verificationCodeFailed(userFriendlyMessage);
    }

    authResponse->deleteLater();
}

void AuthManager::onUsernameAvailabilityResponse(const QString &requestId, const QJsonObject &response)
{

    
    AuthResponse* authResponse = processAuthResponse(response);
    
    if (authResponse->success()) {
        // 从data字段中读取结果
        QJsonObject data = response["data"].toObject();
        bool available = data["available"].toBool();
        QString username = data["username"].toString();
        emit usernameAvailabilityResult(username, available);
    } else {
        LOG_WARNING(QString("Username availability check failed: %1").arg(authResponse->message()));
        // 失败时从data字段中读取username，如果没有则从response中读取
        QJsonObject data = response["data"].toObject();
        QString username = data["username"].toString();
        if (username.isEmpty()) {
            username = response["username"].toString();
        }
        emit usernameAvailabilityResult(username, false);
    }
    
    authResponse->deleteLater();
}

void AuthManager::onEmailAvailabilityResponse(const QString &requestId, const QJsonObject &response)
{

    
    AuthResponse* authResponse = processAuthResponse(response);
    
    if (authResponse->success()) {
        // 从data字段中读取结果
        QJsonObject data = response["data"].toObject();
        bool available = data["available"].toBool();
        QString email = data["email"].toString();
        emit emailAvailabilityResult(email, available);
    } else {
        LOG_WARNING(QString("Email availability check failed: %1").arg(authResponse->message()));
        // 失败时从data字段中读取email，如果没有则从response中读取
        QJsonObject data = response["data"].toObject();
        QString email = data["email"].toString();
        if (email.isEmpty()) {
            email = response["email"].toString();
        }
        emit emailAvailabilityResult(email, false);
    }
    
    authResponse->deleteLater();
}

void AuthManager::onNetworkError(const QString &error)
{
    LOG_ERROR(QString("Network error: %1").arg(error));

    setAuthState(Idle);

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

QString AuthManager::getVerificationCodeErrorMessage(const QString &errorCode, const QString &originalMessage)
{
    if (errorCode == "RATE_LIMITED") {
        return originalMessage; // 服务器已经提供了具体的等待时间
    } else if (errorCode == "IP_RATE_LIMITED") {
        return originalMessage; // 服务器已经提供了具体的等待时间
    } else if (errorCode == "VALIDATION_ERROR") {
        if (originalMessage.contains("邮箱格式无效")) {
            return "邮箱地址格式不正确，请检查后重试";
        }
        return "输入信息有误，请检查后重试";
    } else if (errorCode == "SERVICE_ERROR") {
        return "验证码服务暂时不可用，请稍后重试";
    } else if (errorCode == "CODE_GENERATION_FAILED") {
        return "验证码生成失败，请稍后重试";
    } else if (errorCode == "SEND_FAILED") {
        return "验证码发送失败，请重试";
    } else if (errorCode == "DUPLICATE_REQUEST") {
        return "请求正在处理中，请勿重复提交";
    } else if (errorCode == "REGISTER_FAILED") {
        // 注册失败的错误处理
        if (originalMessage.contains("用户名已存在")) {
            return "用户名已被占用，请尝试其他用户名";
        } else if (originalMessage.contains("邮箱已存在")) {
            return "邮箱已被注册，请尝试其他邮箱";
        } else if (originalMessage.contains("验证码无效")) {
            return "验证码错误或已过期，请重新获取";
        }
        return originalMessage;
    } else {
        // 如果没有特定的错误代码，使用服务器返回的消息
        return originalMessage;
    }
}

QString AuthManager::getRegisterErrorMessage(const QString &errorCode, const QString &originalMessage)
{
    if (errorCode == "RATE_LIMITED") {
        return originalMessage; // 服务器已经提供了具体的等待时间
    } else if (errorCode == "IP_RATE_LIMITED") {
        return originalMessage; // 服务器已经提供了具体的等待时间
    } else if (errorCode == "VALIDATION_ERROR") {
        if (originalMessage.contains("用户名已存在")) {
            return "用户名已被占用，请尝试其他用户名";
        } else if (originalMessage.contains("邮箱已存在")) {
            return "邮箱已被注册，请尝试其他邮箱";
        } else if (originalMessage.contains("用户名格式无效")) {
            return "用户名格式不正确，请检查后重试";
        } else if (originalMessage.contains("邮箱格式无效")) {
            return "邮箱地址格式不正确，请检查后重试";
        }
        return "输入信息有误，请检查后重试";
    } else if (errorCode == "SERVICE_ERROR") {
        return "注册服务暂时不可用，请稍后重试";
    } else if (errorCode == "DUPLICATE_REQUEST") {
        return "请求正在处理中，请勿重复提交";
    } else {
        // 如果没有特定的错误代码，使用服务器返回的消息
        return originalMessage;
    }
}
