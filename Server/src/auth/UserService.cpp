#include "UserService.h"
#include "VerificationCodeManager.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include "../utils/Validator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QJsonDocument>

UserService::UserService(QObject *parent)
    : QObject(parent)
    , _databaseManager(DatabaseManager::instance())
{
}

UserService::~UserService()
{
}

QPair<UserService::AuthResult, User*> UserService::authenticateUser(const QString &username, const QString &password)
{
    if (username.isEmpty() || password.isEmpty()) {
        return qMakePair(ValidationError, nullptr);
    }
    
    // 查找用户（支持用户名或邮箱登录）
    QString sql = R"(
        SELECT id, username, email, display_name, password_hash, salt, avatar_url,
               status, email_verified, bio, verification_code, verification_expires,
               created_at, updated_at, last_online
        FROM users
        WHERE (username = ? OR email = ?) AND status = 'active'
    )";
    
    QSqlQuery query = _databaseManager->executeQuery(sql, {username, username});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error during authentication: %1").arg(query.lastError().text()));
        return qMakePair(DatabaseError, nullptr);
    }
    
    if (!query.next()) {
        LOG_WARNING(QString("Authentication failed: user not found - %1").arg(username));
        return qMakePair(UserNotFound, nullptr);
    }
    
    // 创建用户对象
    User* user = createUserFromQuery(query);
    if (!user) {
        return qMakePair(DatabaseError, nullptr);
    }
    
        // 检查用户状态
    
    if (!user->isEmailVerified()) {
        LOG_WARNING(QString("Authentication failed: user not verified - %1").arg(username));
        delete user;
        return qMakePair(UserNotVerified, nullptr);
    }

    if (!user->isActive()) {
        LOG_WARNING(QString("Authentication failed: user disabled - %1").arg(username));
        delete user;
        return qMakePair(UserDisabled, nullptr);
    }
    
    // 验证密码
    // 客户端发送原始密码，服务器使用存储的salt重新计算哈希进行验证
    if (!user->verifyPassword(password)) {
        LOG_WARNING(QString("Authentication failed: invalid password - %1").arg(username));
        delete user;
        return qMakePair(InvalidCredentials, nullptr);
    }
    
    // 更新最后在线时间
    updateLastLogin(user->id());
    user->updateLastOnline();
    
    LOG_INFO(QString("Authentication successful for user: %1").arg(user->username()));
    return qMakePair(Success, user);
}

QPair<UserService::AuthResult, User*> UserService::registerUser(const QString &username, const QString &email, 
                                                               const QString &password, const QString &verificationCode)
{
    
    // 验证输入数据
    auto validation = validateUserData(username, email, password);
    if (!validation.first) {
        LOG_WARNING(QString("Registration validation failed: %1").arg(validation.second));
        return qMakePair(ValidationError, nullptr);
    }
    
    // 检查用户名是否已存在
    if (isUsernameExists(username)) {
        LOG_WARNING(QString("Registration failed: username already exists - %1").arg(username));
        return qMakePair(UsernameExists, nullptr);
    }
    
    // 检查邮箱是否已存在
    if (isEmailExists(email)) {
        LOG_WARNING(QString("Registration failed: email already exists - %1").arg(email));
        return qMakePair(EmailExists, nullptr);
    }
    
    // 额外检查：确保原始密码不为空且长度合理
    if (password.isEmpty() || password.length() < 6) {
        LOG_WARNING(QString("Registration failed: invalid password length - %1").arg(password.length()));
        return qMakePair(ValidationError, nullptr);
    }
    
    // 验证邮箱验证码
    LOG_INFO(QString("Starting email verification for registration: %1, code: %2").arg(email).arg(verificationCode));
    
    VerificationCodeManager* codeManager = VerificationCodeManager::instance();
    if (!codeManager) {
        LOG_ERROR("VerificationCodeManager not available");
        return qMakePair(DatabaseError, nullptr);
    }
    
    VerificationCodeManager::VerificationResult verifyResult = codeManager->verifyCode(email, verificationCode, VerificationCodeManager::Registration);
    
    if (verifyResult != VerificationCodeManager::Success) {
        QString errorMsg = VerificationCodeManager::getVerificationResultDescription(verifyResult);
        LOG_WARNING(QString("Registration failed due to verification code error: %1 - %2 (result: %3)").arg(email).arg(errorMsg).arg((int)verifyResult));
        return qMakePair(ValidationError, nullptr);
    }
    
    LOG_INFO(QString("Email verification successful for registration: %1").arg(email));
    
    // 开始事务
    if (!_databaseManager->beginTransaction()) {
        LOG_ERROR("Failed to begin transaction for user registration");
        return qMakePair(DatabaseError, nullptr);
    }
    
    // 生成salt并计算密码哈希
    // 客户端发送原始密码，服务器生成salt并计算哈希
    QString salt = generateUserSalt();
    QString serverPasswordHash = hashPassword(password, salt);

    // 插入用户记录
    QString sql = R"(
        INSERT INTO users (username, email, password_hash, salt, display_name, status, email_verified, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, 'active', 1, NOW(), NOW())
    )";

    QVariantList params = {username, email, serverPasswordHash, salt, username}; // 使用username作为display_name
    int result = _databaseManager->executeUpdate(sql, params);
    
    if (result <= 0) {
        _databaseManager->rollbackTransaction();
        LOG_ERROR(QString("Failed to insert user record: %1").arg(_databaseManager->lastError()));
        return qMakePair(DatabaseError, nullptr);
    }
    
    qint64 userId = _databaseManager->lastInsertId();
    
    // 提交事务
    if (!_databaseManager->commitTransaction()) {
        LOG_ERROR("Failed to commit transaction for user registration");
        return qMakePair(DatabaseError, nullptr);
    }
    
    // 获取创建的用户
    User* user = getUserById(userId);
    if (!user) {
        LOG_ERROR("Failed to retrieve created user");
        return qMakePair(DatabaseError, nullptr);
    }
    
    LOG_INFO(QString("User registration successful: %1 (%2)").arg(username).arg(email));
    return qMakePair(Success, user);
}

bool UserService::migrateUserStatuses()
{
    LOG_INFO("Starting user status migration...");
    
    // 首先查询需要迁移的用户
    QString selectSql = R"(
        SELECT id, username, email, status, email_verified 
        FROM users 
        WHERE status = 'inactive' OR email_verified = 0
    )";
    
    QSqlQuery query = _databaseManager->executeQuery(selectSql);
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to query users for migration: %1").arg(query.lastError().text()));
        return false;
    }
    
    while (query.next()) {
        // 用户迁移信息已记录到日志
    }
    
    // 更新所有用户的status为active，email_verified为1
    QString sql = R"(
        UPDATE users 
        SET status = 'active', email_verified = 1 
        WHERE status = 'inactive' OR email_verified = 0
    )";
    
    int result = _databaseManager->executeUpdate(sql);
    if (result < 0) {
        LOG_ERROR(QString("Failed to migrate user statuses: %1").arg(_databaseManager->lastError()));
        return false;
    }
    
    LOG_INFO(QString("User status migration completed. Updated %1 users.").arg(result));
    return true;
}

User* UserService::getUserById(qint64 userId)
{
    QString sql = R"(
        SELECT id, username, email, display_name, password_hash, salt, avatar_url,
               status, email_verified, bio, verification_code, verification_expires,
               created_at, updated_at, last_online
        FROM users WHERE id = ?
    )";
    
    QSqlQuery query = _databaseManager->executeQuery(sql, {userId});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error getting user by ID: %1").arg(query.lastError().text()));
        return nullptr;
    }
    
    if (query.next()) {
        return createUserFromQuery(query);
    }
    
    return nullptr;
}

User* UserService::getUserByUsername(const QString &username)
{
    QString sql = R"(
        SELECT id, username, email, display_name, password_hash, salt, avatar_url,
               status, email_verified, bio, verification_code, verification_expires,
               created_at, updated_at, last_online
        FROM users WHERE username = ?
    )";
    
    QSqlQuery query = _databaseManager->executeQuery(sql, {username});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error getting user by username: %1").arg(query.lastError().text()));
        return nullptr;
    }
    
    if (query.next()) {
        return createUserFromQuery(query);
    }
    
    return nullptr;
}

User* UserService::getUserByEmail(const QString &email)
{
    QString sql = R"(
        SELECT id, username, email, display_name, password_hash, salt, avatar_url,
               status, email_verified, bio, verification_code, verification_expires,
               created_at, updated_at, last_online
        FROM users WHERE email = ?
    )";
    
    QSqlQuery query = _databaseManager->executeQuery(sql, {email});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error getting user by email: %1").arg(query.lastError().text()));
        return nullptr;
    }
    
    if (query.next()) {
        return createUserFromQuery(query);
    }
    
    return nullptr;
}

bool UserService::updateUser(User* user)
{
    if (!user || !user->isValid()) {
        return false;
    }
    
    QString sql = R"(
        UPDATE users SET 
            display_name = ?, avatar_url = ?, status = ?, 
            email_verified = ?, updated_at = NOW()
        WHERE id = ?
    )";
    
    QVariantList params = {
        user->displayName(), user->avatarUrl(), user->status(),
        user->isEmailVerified(), user->id()
    };
    
    int result = _databaseManager->executeUpdate(sql, params);
    
    if (result > 0) {
        LOG_INFO(QString("User updated successfully: %1").arg(user->username()));
        return true;
    } else {
        LOG_ERROR(QString("Failed to update user: %1").arg(user->username()));
        return false;
    }
}

bool UserService::updateLastLogin(qint64 userId)
{
    QString sql = "UPDATE users SET last_online = NOW() WHERE id = ?";
    int result = _databaseManager->executeUpdate(sql, {userId});
    return result > 0;
}

bool UserService::isUsernameExists(const QString &username)
{
    QString sql = "SELECT COUNT(*) FROM users WHERE username = ?";
    QSqlQuery query = _databaseManager->executeQuery(sql, {username});

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

bool UserService::isEmailExists(const QString &email)
{
    QString sql = "SELECT COUNT(*) FROM users WHERE email = ?";
    QSqlQuery query = _databaseManager->executeQuery(sql, {email});

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}



bool UserService::setUserStatus(qint64 userId, const QString &status)
{
    QString sql = "UPDATE users SET status = ?, updated_at = NOW() WHERE id = ?";
    int result = _databaseManager->executeUpdate(sql, {status, userId});

    if (result > 0) {
        LOG_INFO(QString("User status updated: ID=%1, Status=%2").arg(userId).arg(status));
        return true;
    }

    return false;
}

QJsonObject UserService::getUserStatistics()
{
    QJsonObject stats;

    // 总用户数
    QString totalSql = "SELECT COUNT(*) FROM users";
    QSqlQuery totalQuery = _databaseManager->executeQuery(totalSql);
    if (totalQuery.next()) {
        stats["total_users"] = totalQuery.value(0).toInt();
    }

    // 活跃用户数
    QString activeSql = "SELECT COUNT(*) FROM users WHERE status = 'active'";
    QSqlQuery activeQuery = _databaseManager->executeQuery(activeSql);
    if (activeQuery.next()) {
        stats["active_users"] = activeQuery.value(0).toInt();
    }

    // 已验证用户数
    QString verifiedSql = "SELECT COUNT(*) FROM users WHERE email_verified = 1";
    QSqlQuery verifiedQuery = _databaseManager->executeQuery(verifiedSql);
    if (verifiedQuery.next()) {
        stats["verified_users"] = verifiedQuery.value(0).toInt();
    }

    // 在线用户数（基于最后在线时间）
    QString onlineSql = "SELECT COUNT(*) FROM users WHERE last_online >= DATE_SUB(NOW(), INTERVAL 5 MINUTE)";
    QSqlQuery onlineQuery = _databaseManager->executeQuery(onlineSql);
    if (onlineQuery.next()) {
        stats["online_users"] = onlineQuery.value(0).toInt();
    }

    // 今日注册用户数
    QString todaySql = "SELECT COUNT(*) FROM users WHERE DATE(created_at) = CURDATE()";
    QSqlQuery todayQuery = _databaseManager->executeQuery(todaySql);
    if (todayQuery.next()) {
        stats["today_registrations"] = todayQuery.value(0).toInt();
    }

    // 本周注册用户数
    QString weekSql = "SELECT COUNT(*) FROM users WHERE created_at >= DATE_SUB(NOW(), INTERVAL 7 DAY)";
    QSqlQuery weekQuery = _databaseManager->executeQuery(weekSql);
    if (weekQuery.next()) {
        stats["week_registrations"] = weekQuery.value(0).toInt();
    }

    return stats;
}

QPair<bool, QString> UserService::validateUserData(const QString &username, const QString &email, const QString &password)
{
    // 验证用户名
    if (username.trimmed().isEmpty()) {
        return qMakePair(false, "用户名不能为空");
    }

    if (username.length() < 3 || username.length() > 50) {
        return qMakePair(false, "用户名长度必须在3-50个字符之间");
    }

    if (!Validator::isValidUsername(username)) {
        return qMakePair(false, "用户名只能包含字母、数字和下划线");
    }

    // 验证邮箱
    if (email.trimmed().isEmpty()) {
        return qMakePair(false, "邮箱不能为空");
    }

    if (!Validator::isValidEmail(email)) {
        return qMakePair(false, "请输入有效的邮箱地址");
    }

    // 验证原始密码
    if (password.isEmpty()) {
        return qMakePair(false, "密码不能为空");
    }

    // 验证原始密码长度
    if (password.length() < 6) {
        return qMakePair(false, "密码长度不能少于6个字符");
    }

    if (password.length() > 128) {
        return qMakePair(false, "密码长度不能超过128个字符");
    }

    return qMakePair(true, "");
}

QString UserService::getAuthResultDescription(AuthResult result)
{
    switch (result) {
        case Success:
            return "认证成功";
        case InvalidCredentials:
            return "用户名或密码错误";
        case UserNotFound:
            return "用户不存在";
        case UserNotVerified:
            return "用户邮箱未验证";
        case UserDisabled:
            return "用户账号已被禁用";
        case DatabaseError:
            return "数据库错误";
        case ValidationError:
            return "数据验证失败";
        case UsernameExists:
            return "用户名已存在，请选择其他用户名";
        case EmailExists:
            return "邮箱已被注册，请使用其他邮箱或直接登录";
        default:
            return "未知错误";
    }
}

User* UserService::createUserFromQuery(QSqlQuery &query)
{
    User* user = new User(this);

    user->setId(query.value("id").toLongLong());
    user->setUsername(query.value("username").toString());
    user->setEmail(query.value("email").toString());
    user->setDisplayName(query.value("display_name").toString());
    user->setPasswordHash(query.value("password_hash").toString());
    user->setSalt(query.value("salt").toString());
    user->setAvatarUrl(query.value("avatar_url").toString());
    user->setStatus(query.value("status").toString());
    user->setBio(query.value("bio").toString());
    user->setEmailVerified(query.value("email_verified").toBool());
    user->setVerificationCode(query.value("verification_code").toString());
    user->setVerificationExpires(query.value("verification_expires").toDateTime());
    user->setLastOnline(query.value("last_online").toDateTime());
    user->setCreatedAt(query.value("created_at").toDateTime());
    user->setUpdatedAt(query.value("updated_at").toDateTime());

    return user;
}

QString UserService::generateUserSalt()
{
    return Crypto::generateSalt(32);
}

QString UserService::hashPassword(const QString &password, const QString &salt)
{
    return Crypto::hashPassword(password, salt);
}
