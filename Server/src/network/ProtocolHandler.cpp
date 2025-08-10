#include "ProtocolHandler.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include "../utils/Validator.h"
#include <QDateTime>
#include <QSqlQuery>
#include <QMutexLocker>

ProtocolHandler::ProtocolHandler(EmailService *emailService, QObject *parent)
    : QObject(parent)
    , _userService(new UserService(this))
    , _emailService(emailService)
    , _redisClient(RedisClient::instance())
    , _cleanupTimer(new QTimer(this))
{
    // 如果没有提供EmailService，创建一个新的实例（向后兼容）
    if (!_emailService) {
        _emailService = new EmailService(this);
    }
    
    // 初始化请求去重机制
    // 每5分钟清理一次过期的请求ID，防止内存泄漏
    _cleanupTimer->setInterval(5 * 60 * 1000); // 5分钟
    connect(_cleanupTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker locker(&_requestMutex);
        int beforeCount = _processedRequests.size();
        _processedRequests.clear();
        LOG_INFO(QString("Cleaned up processed requests cache: %1 requests").arg(beforeCount));
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
    
    if (requestId.isEmpty()) {
        return createErrorResponse("", action, "INVALID_REQUEST", "Missing request_id");
    }
    
    MessageType messageType = getMessageType(action);
    
    switch (messageType) {
        case Login:
            return handleLoginRequest(message, clientId, clientIP);
        case Register:
            return handleRegisterRequest(message, clientId, clientIP);
        case SendVerificationCode:
            return handleVerificationCodeRequest(message, clientId, clientIP);
        case Heartbeat:
            return handleHeartbeatRequest(message, clientId);
        default:
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
        
        // 保存会话令牌到Redis
        _redisClient->setSessionToken(user->id(), sessionToken, rememberMe ? 24 * 7 : 24); // 7天或1天
        
        // 记录登录日志
        logLoginAttempt(user->id(), user->username(), user->email(), true, clientIP);
        
        // 发送登录成功信号
        emit userLoggedIn(user->id(), clientId, sessionToken);
        
        LOG_INFO(QString("User login successful: %1 from %2").arg(username).arg(clientIP));
        
        // 创建响应数据 - 直接放在根级别，不使用 data 包装
        QJsonObject successResponse;
        successResponse["request_id"] = requestId;
        successResponse["action"] = action + "_response";
        successResponse["success"] = true;
        successResponse["message"] = "登录成功";
        successResponse["timestamp"] = QDateTime::currentSecsSinceEpoch();
        successResponse["user"] = user->toClientJson();
        successResponse["session_token"] = sessionToken;
        successResponse["expires_in"] = rememberMe ? 604800 : 86400; // 秒数
        
        LOG_INFO(QString("Login response created for user %1: success=true, message='登录成功'").arg(username));
        
        return successResponse;
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
    
    // 进行用户注册
    LOG_INFO(QString("Processing registration request for user: %1, email: %2, verification code: %3, request_id: %4").arg(username).arg(email).arg(verificationCode).arg(requestId));
    
    auto registerResult = _userService->registerUser(username, email, password, verificationCode);
    LOG_INFO(QString("Registration result for user %1: %2, request_id: %3").arg(username).arg((int)registerResult.first).arg(requestId));
    
    // 注册请求完成后，延迟移除请求ID以防止重复提交
    LOG_INFO(QString("Registration request processing completed: %1 from %2").arg(requestId).arg(clientIP));
    
    if (registerResult.first == UserService::Success) {
        User* user = registerResult.second;
        
        // 发送注册成功信号
        emit userRegistered(user->id(), user->username(), user->email());
        
        LOG_INFO(QString("User registration successful: %1 (%2) from %3").arg(username).arg(email).arg(clientIP));
        
        // 创建响应数据
        QJsonObject successResponse;
        successResponse["request_id"] = requestId;
        successResponse["action"] = action + "_response";
        successResponse["success"] = true;
        successResponse["timestamp"] = QDateTime::currentSecsSinceEpoch();
        successResponse["user"] = user->toClientJson();
        successResponse["message"] = "注册成功，请使用新账号登录";
        
        return successResponse;
    } else {
        QString errorMessage = UserService::getAuthResultDescription(registerResult.first);
        LOG_WARNING(QString("User registration failed: %1 (%2) from %3 - %4 (result: %5)").arg(username).arg(email).arg(clientIP).arg(errorMessage).arg((int)registerResult.first));
        
        return createErrorResponse(requestId, action, "REGISTER_FAILED", errorMessage);
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
    
    QString code = codeManager->generateAndSaveCode(email, VerificationCodeManager::Registration);
    if (code.isEmpty()) {
        LOG_ERROR(QString("Failed to generate verification code for email: %1").arg(email));
        return createErrorResponse(requestId, action, "CODE_GENERATION_FAILED", "验证码生成失败，请稍后重试");
    }
    
    // 发送邮件
    EmailService::SendResult result = _emailService->sendVerificationCode(email, code, EmailService::Registration);
    
    // 处理完成后从去重集合中移除请求ID
    {
        QMutexLocker locker(&_requestMutex);
        _processedRequests.remove(requestId);
        LOG_INFO(QString("Verification code request completed and removed from processing: %1 for email: %2").arg(requestId).arg(email));
    }
    
    if (result == EmailService::Success) {
        QJsonObject responseData;
        responseData["message"] = "验证码已发送到您的邮箱";
        responseData["expires_in"] = 300; // 5分钟
        
        LOG_INFO(QString("Verification code sent successfully to: %1 from %2").arg(email).arg(clientIP));
        
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
    
    QJsonObject responseData;
    responseData["timestamp"] = QDateTime::currentSecsSinceEpoch();
    responseData["server_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return createSuccessResponse(requestId, action, responseData);
}

QJsonObject ProtocolHandler::handleLogoutRequest(const QJsonObject &request, const QString &clientId, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 删除会话令牌
    _redisClient->deleteSessionToken(userId);
    
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
    if (action == "heartbeat") return Heartbeat;
    if (action == "logout") return Logout;
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
    
    LOG_INFO(QString("Error response created: code=%1, message='%2'").arg(errorCode).arg(errorMessage));
    
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
