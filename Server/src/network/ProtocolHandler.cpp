#include "ProtocolHandler.h"
#include "../chat/ChatProtocolHandler.h"
#include "../auth/SessionManager.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include "../utils/Validator.h"
#include "../auth/UserRegistrationService.h"
#include <QDateTime>
#include <QSqlQuery>
#include <QMutexLocker>

ProtocolHandler::ProtocolHandler(EmailService *emailService, QObject *parent)
    : QObject(parent)
    , _userService(new UserService(this))
    , _emailService(emailService)
    , _redisClient(RedisClient::instance())
    , _chatHandler(ChatProtocolHandler::instance())
    , _sessionManager(SessionManager::instance())
    , _cleanupTimer(new QTimer(this))
{
    // 如果没有提供EmailService，创建一个新的实例（向后兼容）
    if (!_emailService) {
        _emailService = new EmailService(this);
    }
    
    // 初始化请求去重机制
    // 每30分钟清理一次过期的请求ID，防止内存泄漏
    // 增加清理间隔，避免过早清理导致重复处理
    _cleanupTimer->setInterval(30 * 60 * 1000); // 30分钟
    connect(_cleanupTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker locker(&_requestMutex);
        int beforeCount = _processedRequests.size();
        // 只清理超过1小时的请求记录，保留最近的请求记录
        QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-3600);
        auto it = _processedRequestsTime.begin();
        while (it != _processedRequestsTime.end()) {
            if (it.value() < oneHourAgo) {
                _processedRequests.remove(it.key());
                it = _processedRequestsTime.erase(it);
            } else {
                ++it;
            }
        }
        int afterCount = _processedRequests.size();
        LOG_INFO(QString("Cleaned up processed requests cache: %1 -> %2 requests").arg(beforeCount).arg(afterCount));
    });
    _cleanupTimer->start();
}

ProtocolHandler::~ProtocolHandler()
{
}

QJsonObject ProtocolHandler::handleMessage(const QJsonObject &message, const QString &clientId, const QString &clientIP)
{
    QString action = message["action"].toString();
    QString requestId = message["request_id"].toString();
    
    // Processing message
    LOG_INFO(QString("Action: %1, RequestID: %2, ClientID: %3, ClientIP: %4")
             .arg(action).arg(requestId).arg(clientId).arg(clientIP));
    
    if (requestId.isEmpty()) {
        LOG_ERROR("Missing request_id in message");
        return createErrorResponse("", action, "INVALID_REQUEST", "Missing request_id");
    }
    
    MessageType messageType = getMessageType(action);
    LOG_INFO(QString("Message type determined: %1").arg(static_cast<int>(messageType)));

    switch (messageType) {
        case Login:
            // LOG_INFO removed
            return handleLoginRequest(message, clientId, clientIP);
        case Register:
            // LOG_INFO removed
            return handleRegisterRequest(message, clientId, clientIP);
        case SendVerificationCode:
            // LOG_INFO removed
            return handleVerificationCodeRequest(message, clientId, clientIP);
        case CheckUsername:
            // LOG_INFO removed
            return handleCheckUsernameRequest(message, clientId, clientIP);
        case CheckEmail:
            // LOG_INFO removed
            return handleCheckEmailRequest(message, clientId, clientIP);
        case Heartbeat:
            // LOG_INFO removed
            return handleHeartbeatRequest(message, clientId);
        case Chat:
            // LOG_INFO removed
            return handleChatMessage(message, clientId, clientIP);
        default:
            LOG_ERROR(QString("Unknown action: %1").arg(action));
            return createErrorResponse(requestId, action, "UNKNOWN_ACTION", "Unknown action: " + action);
    }
}

QJsonObject ProtocolHandler::handleLoginRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证请求格式
    auto validation = validateRequest(request, {"username", "password"});
    if (!validation.first) {
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", validation.second);
    }
    
    QString username = request["username"].toString();
    QString password = request["password"].toString();
    bool rememberMe = request["remember_me"].toBool();
    
    // 进行用户认证
    auto authResult = _userService->authenticateUser(username, password);
    
    if (authResult.first == UserService::Success) {
        User* user = authResult.second;
        
        // 生成会话令牌
        QString sessionToken = generateSessionToken(user->id());
        
        // 生成设备ID
        QString deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        
        // 使用新的会话管理器创建会话
        if (_sessionManager->createSession(user->id(), sessionToken, deviceId, clientId, clientIP, rememberMe)) {
            // 记录登录日志
            logLoginAttempt(user->id(), user->username(), user->email(), true, clientIP);
            
            // 发送登录成功信号
            emit userLoggedIn(user->id(), clientId, sessionToken);
            
            LOG_INFO(QString("User login successful: %1 from %2").arg(username).arg(clientIP));
            
            // 创建响应数据
            QJsonObject successResponse;
            successResponse["request_id"] = requestId;
            successResponse["action"] = action + "_response";
            successResponse["success"] = true;
            successResponse["message"] = "登录成功";
            successResponse["timestamp"] = QDateTime::currentSecsSinceEpoch();
            successResponse["user"] = user->toClientJson();
            successResponse["session_token"] = sessionToken;
            successResponse["device_id"] = deviceId;
            successResponse["client_id"] = clientId;  // 添加客户端ID
            successResponse["expires_in"] = rememberMe ? 2592000 : 604800; // 30天或7天
            
            return successResponse;
        } else {
            LOG_ERROR(QString("Failed to create session for user %1").arg(user->id()));
            return createErrorResponse(requestId, action, "SESSION_CREATION_FAILED", "Failed to create session");
        }
    } else {
        // 记录登录失败日志
        QString errorMessage = UserService::getAuthResultDescription(authResult.first);
        logLoginAttempt(-1, username, "", false, clientIP, errorMessage);
        
        LOG_WARNING(QString("User login failed: %1 from %2 - %3").arg(username).arg(clientIP).arg(errorMessage));
        
        return createErrorResponse(requestId, action, "AUTH_FAILED", errorMessage);
    }
}

QJsonObject ProtocolHandler::handleRegisterRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 请求去重检查
    {
        QMutexLocker locker(&_requestMutex);
        if (_processedRequests.contains(requestId)) {
            LOG_WARNING(QString("Duplicate registration request detected: %1 from %2").arg(requestId).arg(clientIP));
            return createErrorResponse(requestId, action, "DUPLICATE_REQUEST", "请求正在处理中，请勿重复提交");
        }
        _processedRequests.insert(requestId);
        _processedRequestsTime[requestId] = QDateTime::currentDateTime();
        LOG_INFO(QString("Registration request marked as processing: %1 from %2").arg(requestId).arg(clientIP));
    }
    
    // 验证请求格式
    auto validation = validateRequest(request, {"username", "email", "password", "verification_code"});
    if (!validation.first) {
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", validation.second);
    }
    
    QString username = request["username"].toString();
    QString email = request["email"].toString();
    QString password = request["password"].toString();
    QString verificationCode = request["verification_code"].toString();
    QString displayName = request["display_name"].toString(); // 可选字段
    
    // 使用UserRegistrationService处理注册
    LOG_INFO(QString("Processing registration request for user: %1, email: %2, verification code: %3, request_id: %4").arg(username).arg(email).arg(verificationCode).arg(requestId));
    
    UserRegistrationService* registrationService = UserRegistrationService::instance();
    
    // 构建注册请求
    UserRegistrationService::RegistrationRequest regRequest;
    regRequest.username = username;
    regRequest.email = email;
    regRequest.password = password;
    regRequest.verificationCode = verificationCode;
    regRequest.displayName = displayName.isEmpty() ? username : displayName;
    
    // 执行注册
    UserRegistrationService::RegistrationResponse response = registrationService->registerUser(regRequest);
    
    LOG_INFO(QString("Registration result for user %1: %2, request_id: %3").arg(username).arg((int)response.result).arg(requestId));
    
    // 注册请求完成后，延迟移除请求ID以防止重复提交

    
    if (response.result == UserRegistrationService::Success) {
        // 发送注册成功信号
        QJsonObject userData = response.userData;
        qint64 userId = userData["user_id"].toString().toLongLong();
        emit userRegistered(userId, userData["username"].toString(), userData["email"].toString());
        
        LOG_INFO(QString("User registration successful: %1 (%2) with user_id: %3 from %4").arg(username).arg(email).arg(response.userId).arg(clientIP));
        
        // 创建响应数据
        QJsonObject successResponse;
        successResponse["request_id"] = requestId;
        successResponse["action"] = action + "_response";
        successResponse["success"] = true;
        successResponse["timestamp"] = QDateTime::currentSecsSinceEpoch();
        successResponse["message"] = response.message;
        successResponse["user_id"] = response.userId;
        successResponse["user"] = response.userData;
        
        return successResponse;
    } else {
        LOG_WARNING(QString("User registration failed: %1 (%2) from %3 - %4 (result: %5)").arg(username).arg(email).arg(clientIP).arg(response.message).arg((int)response.result));
        
        return createErrorResponse(requestId, action, "REGISTER_FAILED", response.message);
    }
}

QJsonObject ProtocolHandler::handleVerificationCodeRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证请求格式
    auto validation = validateRequest(request, {"email"});
    if (!validation.first) {
        LOG_WARNING(QString("Invalid verification code request format from %1: %2").arg(clientIP).arg(validation.second));
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", validation.second);
    }
    
    QString email = request["email"].toString();
    
    // 请求去重检查
    {
        QMutexLocker locker(&_requestMutex);
        if (_processedRequests.contains(requestId)) {
            LOG_WARNING(QString("Duplicate verification code request detected: %1 from %2 for email: %3").arg(requestId).arg(clientIP).arg(email));
            return createErrorResponse(requestId, action, "DUPLICATE_REQUEST", "请求正在处理中，请勿重复提交");
        }
        _processedRequests.insert(requestId);
        _processedRequestsTime[requestId] = QDateTime::currentDateTime();
        LOG_INFO(QString("Verification code request marked as processing: %1 for email: %2 from %3").arg(requestId).arg(email).arg(clientIP));
    }
    
    // 验证邮箱格式
    if (!Validator::isValidEmail(email)) {
        LOG_WARNING(QString("Invalid email format in verification code request: %1 from %2").arg(email).arg(clientIP));
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", "邮箱格式无效");
    }

    // 发送验证码
    VerificationCodeManager* codeManager = VerificationCodeManager::instance();
    if (!codeManager) {
        LOG_ERROR("VerificationCodeManager not available");
        return createErrorResponse(requestId, action, "SERVICE_ERROR", "验证码服务不可用");
    }
    
    // 检查频率限制
    if (!codeManager->isAllowedToSend(email, 30)) { // 从60秒改为30秒
        int remainingTime = codeManager->getRemainingWaitTime(email, 30);
        QString errorMessage = QString("验证码发送频繁，请%1秒后重试").arg(remainingTime);
        LOG_WARNING(QString("Rate limited for email: %1, remaining time: %2 seconds").arg(email).arg(remainingTime));
        return createErrorResponse(requestId, action, "RATE_LIMITED", errorMessage);
    }
    
    // 检查IP地址频率限制
    if (!codeManager->isIPAllowedToSend(clientIP, 30)) { // IP地址限制30秒间隔
        int remainingTime = codeManager->getIPRemainingWaitTime(clientIP, 30);
        QString errorMessage = QString("发送过于频繁，请%1秒后重试").arg(remainingTime);
        LOG_WARNING(QString("IP rate limited: %1, remaining time: %2 seconds").arg(clientIP).arg(remainingTime));
        return createErrorResponse(requestId, action, "IP_RATE_LIMITED", errorMessage);
    }
    
    QString code = codeManager->generateAndSaveCode(email, clientIP, VerificationCodeManager::Registration);
    if (code.isEmpty()) {
        LOG_ERROR(QString("Failed to generate verification code for email: %1").arg(email));
        return createErrorResponse(requestId, action, "CODE_GENERATION_FAILED", "验证码生成失败，请稍后重试");
    }
    
    // 发送邮件
    EmailService::SendResult result = _emailService->sendVerificationCode(email, code, EmailService::Registration);

    // 注意：不要立即从去重集合中移除请求ID，保持一段时间防止重复处理
    // 请求ID会在定时器清理时被移除
    
    if (result == EmailService::Success) {
        QJsonObject responseData;
        responseData["message"] = "验证码已发送到您的邮箱";
        responseData["expires_in"] = 300; // 5分钟
        
    
        
        return createSuccessResponse(requestId, action, responseData);
    } else {
        QString errorMessage = EmailService::getSendResultDescription(result);
        LOG_WARNING(QString("Verification code send failed: %1 from %2 - %3").arg(email).arg(clientIP).arg(errorMessage));
        
        return createErrorResponse(requestId, action, "SEND_FAILED", errorMessage);
    }
}

QJsonObject ProtocolHandler::handleHeartbeatRequest(const QJsonObject &request, const QString &clientId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 从请求中获取用户ID
    qint64 userId = request["user_id"].toVariant().toLongLong();
    
    if (userId <= 0) {
        LOG_ERROR(QString("Invalid user_id in heartbeat request: %1").arg(userId));
        return createErrorResponse(requestId, action, "INVALID_USER_ID", "Invalid user_id in heartbeat request");
    }
    
    // 检查聊天处理器是否可用
    if (!_chatHandler) {
        LOG_ERROR("ChatProtocolHandler instance is null");
        return createErrorResponse(requestId, action, "SERVICE_UNAVAILABLE", "Chat service is not available");
    }
    
    // 初始化聊天处理器（如果尚未初始化）
    if (!_chatHandler->initialize()) {
        LOG_ERROR("Chat service initialization failed");
        return createErrorResponse(requestId, action, "SERVICE_UNAVAILABLE", "Chat service initialization failed");
    }
    
    // 委托给聊天协议处理器处理心跳
    return _chatHandler->handleChatRequest(request, "", userId);
}

QJsonObject ProtocolHandler::handleLogoutRequest(const QJsonObject &request, const QString &clientId, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    QString sessionToken = request["session_token"].toString();
    
    // 使用新的会话管理器销毁会话
    if (!sessionToken.isEmpty()) {
        if (_sessionManager->destroySession(sessionToken)) {
            LOG_INFO(QString("Session destroyed for user %1").arg(userId));
        } else {
            LOG_WARNING(QString("Failed to destroy session for user %1").arg(userId));
        }
    } else {
        // 如果没有提供会话令牌，销毁用户的所有会话
        int destroyedCount = _sessionManager->destroyUserSessions(userId);
        LOG_INFO(QString("Destroyed %1 sessions for user %2").arg(destroyedCount).arg(userId));
    }
    
    // 发送登出信号
    emit userLoggedOut(userId, clientId);
    
    QJsonObject responseData;
    responseData["message"] = "登出成功";
    
    LOG_INFO(QString("User logout: ID=%1, Client=%2").arg(userId).arg(clientId));
    
    return createSuccessResponse(requestId, action, responseData);
}

QPair<bool, QString> ProtocolHandler::validateRequest(const QJsonObject &request, const QStringList &requiredFields)
{
    for (const QString &field : requiredFields) {
        if (!request.contains(field) || request[field].toString().trimmed().isEmpty()) {
            return qMakePair(false, QString("Missing or empty field: %1").arg(field));
        }
    }
    
    return qMakePair(true, "");
}

ProtocolHandler::MessageType ProtocolHandler::getMessageType(const QString &action)
{
    if (action == "login") return Login;
    if (action == "register") return Register;
    if (action == "send_verification_code") return SendVerificationCode;
    if (action == "check_username") return CheckUsername;
    if (action == "check_email") return CheckEmail;
    if (action == "heartbeat") return Heartbeat;

    // 聊天相关的动作
    if (action.startsWith("friend_") || action.startsWith("message_") ||
        action.startsWith("status_") || action == "send_message" ||
        action == "get_chat_history" || action == "get_chat_sessions") {
        return Chat;
    }

    return Unknown;
}

QJsonObject ProtocolHandler::createSuccessResponse(const QString &requestId, const QString &action, const QJsonObject &data)
{
    QJsonObject response;
    response["request_id"] = requestId;
    response["action"] = action + "_response";
    response["success"] = true;
    response["timestamp"] = QDateTime::currentSecsSinceEpoch();
    
    if (!data.isEmpty()) {
        response["data"] = data;
    }
    
    return response;
}

QJsonObject ProtocolHandler::createErrorResponse(const QString &requestId, const QString &action, const QString &errorCode, const QString &errorMessage)
{
    QJsonObject response;
    response["request_id"] = requestId;
    response["action"] = action + "_response";
    response["success"] = false;
    response["error_code"] = errorCode;
    response["error_message"] = errorMessage;
    response["message"] = errorMessage;  // 为了兼容性，也设置message字段
    response["timestamp"] = QDateTime::currentSecsSinceEpoch();
    
    LOG_WARNING(QString("Error response created: requestId=%1, action=%2, errorCode=%3, message=%4")
                .arg(requestId).arg(action).arg(errorCode).arg(errorMessage));
    
    return response;
}

QString ProtocolHandler::generateSessionToken(qint64 userId)
{
    return Crypto::generateSessionToken(userId);
}

void ProtocolHandler::logLoginAttempt(qint64 userId, const QString &username, const QString &email, 
                                     bool success, const QString &clientIP, const QString &errorMessage)
{
    // 记录到数据库
    QString sql = R"(
        INSERT INTO login_logs (user_id, username, email, success, ip_address, error_message, created_at)
        VALUES (?, ?, ?, ?, ?, ?, NOW())
    )";
    
    QVariantList params = {
        userId > 0 ? userId : QVariant(),
        username,
        email,
        success,
        clientIP,
        errorMessage
    };
    
    DatabaseManager::instance()->executeUpdate(sql, params);
    
    // 记录到日志文件
    Logger::logAuth(success ? "LOGIN_SUCCESS" : "LOGIN_FAILED", username, success, clientIP, errorMessage);
}

QJsonObject ProtocolHandler::handleChatMessage(const QJsonObject &request, const QString &clientId, const QString &clientIP)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // Processing chat message
    LOG_INFO(QString("Action: %1, RequestID: %2, ClientID: %3, ClientIP: %4")
             .arg(action).arg(requestId).arg(clientId).arg(clientIP));

    // 验证用户是否已认证
    QString sessionToken = request["session_token"].toString();
    if (sessionToken.isEmpty()) {
        LOG_ERROR("Session token is missing");
        return createErrorResponse(requestId, action, "AUTHENTICATION_REQUIRED", "Session token is required for chat operations");
    }

    LOG_INFO(QString("Session token found: %1").arg(sessionToken.left(10) + "..."));

    // 使用新的会话管理器验证会话
    SessionManager::SessionInfo sessionInfo = _sessionManager->validateSession(sessionToken);
    
    if (!sessionInfo.isValid) {
        LOG_ERROR(QString("Invalid or expired session token: %1").arg(sessionToken.left(10) + "..."));
        return createErrorResponse(requestId, action, "INVALID_SESSION", "Invalid or expired session token");
    }

    LOG_INFO(QString("User ID resolved: %1, Device: %2").arg(sessionInfo.userId).arg(sessionInfo.deviceId));

    // 更新会话活动时间（滑动窗口）
    _sessionManager->updateSessionActivity(sessionToken);

    // 检查聊天处理器是否可用
    if (!_chatHandler) {
        LOG_ERROR("ChatProtocolHandler instance is null");
        return createErrorResponse(requestId, action, "SERVICE_UNAVAILABLE", "Chat service is not available");
    }

    // 初始化聊天处理器（如果尚未初始化）
    if (!_chatHandler->initialize()) {
        LOG_ERROR("Chat service initialization failed");
        return createErrorResponse(requestId, action, "SERVICE_UNAVAILABLE", "Chat service initialization failed");
    }

    // 委托给聊天协议处理器
    return _chatHandler->handleChatRequest(request, clientIP, sessionInfo.userId);
}

QJsonObject ProtocolHandler::handleCheckUsernameRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证请求格式
    auto validation = validateRequest(request, {"username"});
    if (!validation.first) {
        LOG_WARNING(QString("Invalid check username request format from %1: %2").arg(clientIP).arg(validation.second));
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", validation.second);
    }
    
    QString username = request["username"].toString();
    
    // 验证用户名格式
    if (!Validator::isValidUsername(username)) {
        LOG_WARNING(QString("Invalid username format in check request: %1 from %2").arg(username).arg(clientIP));
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", "用户名格式无效");
    }
    
    // 检查用户名可用性
    UserRegistrationService* registrationService = UserRegistrationService::instance();
    if (!registrationService) {
        LOG_ERROR("UserRegistrationService not available");
        return createErrorResponse(requestId, action, "SERVICE_ERROR", "服务不可用");
    }
    
    bool available = registrationService->isUsernameAvailable(username);
    
    QJsonObject responseData;
    responseData["username"] = username;
    responseData["available"] = available;
    responseData["message"] = available ? "用户名可用" : "用户名已被使用";
    
    LOG_INFO(QString("Username availability check: %1 -> %2 from %3").arg(username).arg(available ? "available" : "unavailable").arg(clientIP));
    
    return createSuccessResponse(requestId, action, responseData);
}

QJsonObject ProtocolHandler::handleCheckEmailRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证请求格式
    auto validation = validateRequest(request, {"email"});
    if (!validation.first) {
        LOG_WARNING(QString("Invalid check email request format from %1: %2").arg(clientIP).arg(validation.second));
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", validation.second);
    }
    
    QString email = request["email"].toString();
    
    // 验证邮箱格式
    if (!Validator::isValidEmail(email)) {
        LOG_WARNING(QString("Invalid email format in check request: %1 from %2").arg(email).arg(clientIP));
        return createErrorResponse(requestId, action, "VALIDATION_ERROR", "邮箱格式无效");
    }
    
    // 检查邮箱可用性
    UserRegistrationService* registrationService = UserRegistrationService::instance();
    if (!registrationService) {
        LOG_ERROR("UserRegistrationService not available");
        return createErrorResponse(requestId, action, "SERVICE_ERROR", "服务不可用");
    }
    
    bool available = registrationService->isEmailAvailable(email);
    
    QJsonObject responseData;
    responseData["email"] = email;
    responseData["available"] = available;
    responseData["message"] = available ? "邮箱可用" : "邮箱已被注册";
    
    LOG_INFO(QString("Email availability check: %1 -> %2 from %3").arg(email).arg(available ? "available" : "unavailable").arg(clientIP));
    
    return createSuccessResponse(requestId, action, responseData);
}
