#include "UserRegistrationService.h"
#include "UserIdGenerator.h"
#include "VerificationCodeManager.h"
#include "../database/DatabaseManager.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include "../utils/Validator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDateTime>

UserRegistrationService::UserRegistrationService(QObject* parent)
    : QObject(parent)
    , _databaseManager(DatabaseManager::instance())
    , _userIdGenerator(UserIdGenerator::instance())
    , _verificationCodeManager(VerificationCodeManager::instance())
    , _totalRegistrations(0)
    , _successfulRegistrations(0)
    , _failedRegistrations(0)
{
}

UserRegistrationService::~UserRegistrationService()
{
}

UserRegistrationService* UserRegistrationService::instance()
{
    return ThreadSafeSingleton<UserRegistrationService>::instance();
}

bool UserRegistrationService::RegistrationRequest::isValid() const
{
    return !username.isEmpty() && 
           !email.isEmpty() && 
           !password.isEmpty() && 
           !verificationCode.isEmpty();
}

QJsonObject UserRegistrationService::RegistrationResponse::toJson() const
{
    QJsonObject json;
    json["result"] = static_cast<int>(result);
    json["success"] = (result == Success);
    json["message"] = message;
    
    if (result == Success) {
        json["user_id"] = userId;
        json["user_data"] = userData;
    }
    
    return json;
}

UserRegistrationService::RegistrationResponse UserRegistrationService::registerUser(const RegistrationRequest& request)
{
    QMutexLocker locker(&_mutex);
    
    RegistrationResponse response;
    _totalRegistrations.fetchAndAddAcquire(1);
    
    LOG_INFO(QString("Starting user registration for email: %1, username: %2").arg(request.email).arg(request.username));
    
    // 1. 验证注册请求
    response.result = validateRegistrationRequest(request);
    if (response.result != Success) {
        response.message = getResultDescription(response.result);
        _failedRegistrations.fetchAndAddAcquire(1);
        emit registrationFailed(response.result, request.email);
        LOG_WARNING(QString("Registration validation failed for %1: %2").arg(request.email).arg(response.message));
        return response;
    }
    
    // 2. 检查用户名和邮箱唯一性
    response.result = checkUniqueness(request.username, request.email);
    if (response.result != Success) {
        response.message = getResultDescription(response.result);
        _failedRegistrations.fetchAndAddAcquire(1);
        emit registrationFailed(response.result, request.email);
        LOG_WARNING(QString("Registration uniqueness check failed for %1: %2").arg(request.email).arg(response.message));
        return response;
    }
    
    // 3. 验证邮箱验证码
    response.result = verifyEmailCode(request.email, request.verificationCode);
    if (response.result != Success) {
        response.message = getResultDescription(response.result);
        _failedRegistrations.fetchAndAddAcquire(1);
        emit registrationFailed(response.result, request.email);
        LOG_WARNING(QString("Email verification failed for %1: %2").arg(request.email).arg(response.message));
        return response;
    }
    
    // 4. 生成用户ID
    QString userId;
    UserIdGenerator::GenerateResult idResult = _userIdGenerator->generateNextUserId(userId);
    if (idResult != UserIdGenerator::Success) {
        response.result = UserIdGenerationFailed;
        response.message = QString("用户ID生成失败: %1").arg(UserIdGenerator::getResultDescription(idResult));
        _failedRegistrations.fetchAndAddAcquire(1);
        emit registrationFailed(response.result, request.email);
        LOG_ERROR(QString("User ID generation failed for %1: %2").arg(request.email).arg(response.message));
        return response;
    }
    
    response.userId = userId;
    
    // 5. 创建用户记录
    response.result = createUserRecord(request, userId);
    if (response.result != Success) {
        response.message = getResultDescription(response.result);
        _failedRegistrations.fetchAndAddAcquire(1);
        emit registrationFailed(response.result, request.email);
        LOG_ERROR(QString("User record creation failed for %1: %2").arg(request.email).arg(response.message));
        return response;
    }
    
    // 6. 构建成功响应
    response.result = Success;
    response.message = "用户注册成功";
    response.userData = buildUserDataJson(userId, request);
    
    _successfulRegistrations.fetchAndAddAcquire(1);
    emit userRegistered(userId, request.username, request.email);
    
    LOG_INFO(QString("User registration successful - ID: %1, Username: %2, Email: %3").arg(userId).arg(request.username).arg(request.email));
    
    return response;
}

bool UserRegistrationService::isUsernameAvailable(const QString& username)
{
    if (!isValidUsername(username)) {
        return false;
    }
    
    QString sql = "SELECT COUNT(*) FROM users WHERE username = ?";
    QSqlQuery query = _databaseManager->executeQuery(sql, {username});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error checking username availability: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (query.next()) {
        return query.value(0).toInt() == 0;
    }
    
    return false;
}

bool UserRegistrationService::isEmailAvailable(const QString& email)
{
    if (!isValidEmail(email)) {
        return false;
    }
    
    QString sql = "SELECT COUNT(*) FROM users WHERE email = ?";
    QSqlQuery query = _databaseManager->executeQuery(sql, {email});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error checking email availability: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (query.next()) {
        return query.value(0).toInt() == 0;
    }
    
    return false;
}

bool UserRegistrationService::isValidUsername(const QString& username)
{
    return Validator::isValidUsername(username);
}

bool UserRegistrationService::isValidEmail(const QString& email)
{
    return Validator::isValidEmail(email);
}

bool UserRegistrationService::isValidPassword(const QString& password)
{
    return Validator::isValidPassword(password);
}

QString UserRegistrationService::getResultDescription(RegistrationResult result)
{
    switch (result) {
        case Success:
            return "注册成功";
        case InvalidInput:
            return "输入参数无效";
        case UsernameExists:
            return "用户名已存在";
        case EmailExists:
            return "邮箱已被注册";
        case InvalidVerificationCode:
            return "验证码无效或已过期";
        case DatabaseError:
            return "数据库操作错误";
        case UserIdGenerationFailed:
            return "用户ID生成失败";
        case PasswordTooWeak:
            return "密码强度不足";
        case EmailFormatInvalid:
            return "邮箱格式无效";
        case UsernameFormatInvalid:
            return "用户名格式无效";
        default:
            return "未知错误";
    }
}

QJsonObject UserRegistrationService::getRegistrationStatistics()
{
    QJsonObject stats;
    stats["total_registrations"] = _totalRegistrations.loadAcquire();
    stats["successful_registrations"] = _successfulRegistrations.loadAcquire();
    stats["failed_registrations"] = _failedRegistrations.loadAcquire();
    
    int total = _totalRegistrations.loadAcquire();
    if (total > 0) {
        stats["success_rate"] = static_cast<double>(_successfulRegistrations.loadAcquire()) / total * 100.0;
    } else {
        stats["success_rate"] = 0.0;
    }
    
    // 获取用户ID序列状态
    int currentId, maxId, remainingCount;
    if (_userIdGenerator->getSequenceStatus(currentId, maxId, remainingCount)) {
        stats["user_id_sequence"] = QJsonObject{
            {"current_id", currentId},
            {"max_id", maxId},
            {"remaining_count", remainingCount},
            {"usage_percentage", static_cast<double>(currentId) / maxId * 100.0}
        };
    }
    
    return stats;
}

UserRegistrationService::RegistrationResult UserRegistrationService::validateRegistrationRequest(const RegistrationRequest& request)
{
    // 检查基本有效性
    if (!request.isValid()) {
        return InvalidInput;
    }
    
    // 验证用户名格式
    if (!isValidUsername(request.username)) {
        return UsernameFormatInvalid;
    }
    
    // 验证邮箱格式
    if (!isValidEmail(request.email)) {
        return EmailFormatInvalid;
    }
    
    // 验证密码强度
    if (!isValidPassword(request.password)) {
        return PasswordTooWeak;
    }
    
    return Success;
}

UserRegistrationService::RegistrationResult UserRegistrationService::checkUniqueness(const QString& username, const QString& email)
{
    // 检查用户名是否已存在
    if (!isUsernameAvailable(username)) {
        return UsernameExists;
    }
    
    // 检查邮箱是否已存在
    if (!isEmailAvailable(email)) {
        return EmailExists;
    }
    
    return Success;
}

UserRegistrationService::RegistrationResult UserRegistrationService::verifyEmailCode(const QString& email, const QString& code)
{
    VerificationCodeManager::VerificationResult result = 
        _verificationCodeManager->verifyCode(email, code, VerificationCodeManager::Registration);
    
    if (result == VerificationCodeManager::Success) {
        return Success;
    }
    
    return InvalidVerificationCode;
}

UserRegistrationService::RegistrationResult UserRegistrationService::createUserRecord(const RegistrationRequest& request, const QString& userId)
{
    // 生成密码哈希和盐值
    QString salt;
    QString passwordHash = generatePasswordHash(request.password, salt);
    
    // 准备SQL语句
    QString sql = R"(
        INSERT INTO users (user_id, username, email, password_hash, salt, display_name, status, email_verified, created_at)
        VALUES (?, ?, ?, ?, ?, ?, 'active', 1, NOW())
    )";
    
    QVariantList params = {
        userId,
        request.username,
        request.email,
        passwordHash,
        salt,
        request.displayName.isEmpty() ? request.username : request.displayName
    };
    
    int result = _databaseManager->executeUpdate(sql, params);
    
    if (result > 0) {
    
        return Success;
    }
    
    LOG_ERROR(QString("Failed to create user record for %1").arg(request.email));
    return DatabaseError;
}

QString UserRegistrationService::generatePasswordHash(const QString& password, QString& salt)
{
    salt = Crypto::generateSalt();
    return Crypto::hashPassword(password, salt);
}

QJsonObject UserRegistrationService::buildUserDataJson(const QString& userId, const RegistrationRequest& request)
{
    QJsonObject userData;
    userData["user_id"] = userId;
    userData["username"] = request.username;
    userData["email"] = request.email;
    userData["display_name"] = request.displayName.isEmpty() ? request.username : request.displayName;
    userData["status"] = "active";
    userData["email_verified"] = true;
    userData["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return userData;
}
