#include "UserService.h"
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

QPair<UserService::AuthResult, User*> UserService::authenticateUser(const QString &username, const QString &passwordHash)
{
    if (username.isEmpty() || passwordHash.isEmpty()) {
        return qMakePair(ValidationError, nullptr);
    }
    
    // 查找用户（支持用户名或邮箱登录）
    QString sql = R"(
        SELECT id, username, email, display_name, password_hash, salt, avatar_url, 
               status, theme, is_verified, is_active, created_at, updated_at, last_login
        FROM users 
        WHERE (username = ? OR email = ?) AND is_active = 1
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
    if (!user->isVerified()) {
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
    if (!Crypto::verifyPassword(passwordHash, user->salt(), user->passwordHash())) {
        LOG_WARNING(QString("Authentication failed: invalid password - %1").arg(username));
        delete user;
        return qMakePair(InvalidCredentials, nullptr);
    }
    
    // 更新最后登录时间
    updateLastLogin(user->id());
    user->updateLastLogin();
    
    LOG_INFO(QString("Authentication successful for user: %1").arg(user->username()));
    return qMakePair(Success, user);
}

QPair<UserService::AuthResult, User*> UserService::registerUser(const QString &username, const QString &email, 
                                                               const QString &passwordHash, const QString &verificationCode)
{
    // 验证输入数据
    auto validation = validateUserData(username, email, passwordHash);
    if (!validation.first) {
        LOG_WARNING(QString("Registration validation failed: %1").arg(validation.second));
        return qMakePair(ValidationError, nullptr);
    }
    
    // 检查用户名是否已存在
    if (isUsernameExists(username)) {
        LOG_WARNING(QString("Registration failed: username already exists - %1").arg(username));
        return qMakePair(ValidationError, nullptr);
    }
    
    // 检查邮箱是否已存在
    if (isEmailExists(email)) {
        LOG_WARNING(QString("Registration failed: email already exists - %1").arg(email));
        return qMakePair(ValidationError, nullptr);
    }
    
    // 验证邮箱验证码
    if (!verifyEmail(email, verificationCode)) {
        LOG_WARNING(QString("Registration failed: invalid verification code - %1").arg(email));
        return qMakePair(ValidationError, nullptr);
    }
    
    // 开始事务
    if (!_databaseManager->beginTransaction()) {
        LOG_ERROR("Failed to begin transaction for user registration");
        return qMakePair(DatabaseError, nullptr);
    }
    
    // 生成盐值和密码哈希
    QString salt = Crypto::generateSalt();
    QString finalPasswordHash = Crypto::hashPassword(passwordHash, salt);
    
    // 插入用户记录
    QString sql = R"(
        INSERT INTO users (username, email, password_hash, salt, is_verified, created_at, updated_at)
        VALUES (?, ?, ?, ?, 1, NOW(), NOW())
    )";
    
    int result = _databaseManager->executeUpdate(sql, {username, email, finalPasswordHash, salt});
    
    if (result <= 0) {
        _databaseManager->rollbackTransaction();
        LOG_ERROR(QString("Failed to insert user record: %1").arg(_databaseManager->lastError()));
        return qMakePair(DatabaseError, nullptr);
    }
    
    qint64 userId = _databaseManager->lastInsertId();
    
    // 标记验证码为已使用
    QString updateCodeSql = "UPDATE verification_codes SET used_at = NOW() WHERE email = ? AND code = ? AND used_at IS NULL";
    _databaseManager->executeUpdate(updateCodeSql, {email, verificationCode});
    
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

User* UserService::getUserById(qint64 userId)
{
    QString sql = R"(
        SELECT id, username, email, display_name, password_hash, salt, avatar_url, 
               status, theme, is_verified, is_active, created_at, updated_at, last_login
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
               status, theme, is_verified, is_active, created_at, updated_at, last_login
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
               status, theme, is_verified, is_active, created_at, updated_at, last_login
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
            display_name = ?, avatar_url = ?, status = ?, theme = ?, 
            is_verified = ?, is_active = ?, updated_at = NOW()
        WHERE id = ?
    )";
    
    QVariantList params = {
        user->displayName(), user->avatarUrl(), user->status(), user->theme(),
        user->isVerified(), user->isActive(), user->id()
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
    QString sql = "UPDATE users SET last_login = NOW() WHERE id = ?";
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

bool UserService::verifyEmail(const QString &email, const QString &verificationCode)
{
    QString sql = R"(
        SELECT COUNT(*) FROM verification_codes
        WHERE email = ? AND code = ? AND expires_at > NOW() AND used_at IS NULL
    )";

    QSqlQuery query = _databaseManager->executeQuery(sql, {email, verificationCode});

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
    QString activeSql = "SELECT COUNT(*) FROM users WHERE is_active = 1";
    QSqlQuery activeQuery = _databaseManager->executeQuery(activeSql);
    if (activeQuery.next()) {
        stats["active_users"] = activeQuery.value(0).toInt();
    }

    // 已验证用户数
    QString verifiedSql = "SELECT COUNT(*) FROM users WHERE is_verified = 1";
    QSqlQuery verifiedQuery = _databaseManager->executeQuery(verifiedSql);
    if (verifiedQuery.next()) {
        stats["verified_users"] = verifiedQuery.value(0).toInt();
    }

    // 在线用户数
    QString onlineSql = "SELECT COUNT(*) FROM users WHERE status = 'online'";
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

    // 验证密码
    if (password.isEmpty()) {
        return qMakePair(false, "密码不能为空");
    }

    if (password.length() < 6) {
        return qMakePair(false, "密码长度至少6个字符");
    }

    if (password.length() > 100) {
        return qMakePair(false, "密码长度不能超过100个字符");
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
    user->setTheme(query.value("theme").toString());
    user->setIsVerified(query.value("is_verified").toBool());
    user->setIsActive(query.value("is_active").toBool());
    user->setCreatedAt(query.value("created_at").toDateTime());
    user->setUpdatedAt(query.value("updated_at").toDateTime());
    user->setLastLogin(query.value("last_login").toDateTime());

    return user;
}
