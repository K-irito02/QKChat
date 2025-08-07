#ifndef USERSERVICE_H
#define USERSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include "../models/User.h"
#include "../database/DatabaseManager.h"

/**
 * @brief 用户服务类
 * 
 * 提供用户相关的业务逻辑处理，包括用户注册、登录验证、信息更新等功能。
 * 负责与数据库交互，处理用户数据的增删改查操作。
 */
class UserService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 认证结果枚举
     */
    enum AuthResult {
        Success,
        InvalidCredentials,
        UserNotFound,
        UserNotVerified,
        UserDisabled,
        DatabaseError,
        ValidationError
    };
    Q_ENUM(AuthResult)

    explicit UserService(QObject *parent = nullptr);
    ~UserService();
    
    /**
     * @brief 用户登录验证
     * @param username 用户名或邮箱
     * @param passwordHash 密码哈希
     * @return 认证结果和用户信息
     */
    QPair<AuthResult, User*> authenticateUser(const QString &username, const QString &passwordHash);
    
    /**
     * @brief 用户注册
     * @param username 用户名
     * @param email 邮箱
     * @param passwordHash 密码哈希
     * @param verificationCode 验证码
     * @return 注册结果和用户信息
     */
    QPair<AuthResult, User*> registerUser(const QString &username, const QString &email, 
                                         const QString &passwordHash, const QString &verificationCode);
    
    /**
     * @brief 根据ID获取用户
     * @param userId 用户ID
     * @return 用户对象
     */
    User* getUserById(qint64 userId);
    
    /**
     * @brief 根据用户名获取用户
     * @param username 用户名
     * @return 用户对象
     */
    User* getUserByUsername(const QString &username);
    
    /**
     * @brief 根据邮箱获取用户
     * @param email 邮箱
     * @return 用户对象
     */
    User* getUserByEmail(const QString &email);
    
    /**
     * @brief 更新用户信息
     * @param user 用户对象
     * @return 更新是否成功
     */
    bool updateUser(User* user);
    
    /**
     * @brief 更新用户最后登录时间
     * @param userId 用户ID
     * @return 更新是否成功
     */
    bool updateLastLogin(qint64 userId);
    
    /**
     * @brief 检查用户名是否存在
     * @param username 用户名
     * @return 是否存在
     */
    bool isUsernameExists(const QString &username);
    
    /**
     * @brief 检查邮箱是否存在
     * @param email 邮箱
     * @return 是否存在
     */
    bool isEmailExists(const QString &email);
    
    /**
     * @brief 验证用户邮箱
     * @param email 邮箱
     * @param verificationCode 验证码
     * @return 验证是否成功
     */
    bool verifyEmail(const QString &email, const QString &verificationCode);
    
    /**
     * @brief 设置用户状态
     * @param userId 用户ID
     * @param status 状态
     * @return 设置是否成功
     */
    bool setUserStatus(qint64 userId, const QString &status);
    
    /**
     * @brief 获取用户统计信息
     * @return 统计信息JSON对象
     */
    QJsonObject getUserStatistics();
    
    /**
     * @brief 验证用户数据
     * @param username 用户名
     * @param email 邮箱
     * @param password 密码
     * @return 验证结果和错误信息
     */
    QPair<bool, QString> validateUserData(const QString &username, const QString &email, const QString &password);
    
    /**
     * @brief 获取认证结果描述
     * @param result 认证结果
     * @return 描述文本
     */
    static QString getAuthResultDescription(AuthResult result);

private:
    /**
     * @brief 从查询结果创建用户对象
     * @param query 数据库查询对象
     * @return 用户对象
     */
    User* createUserFromQuery(QSqlQuery &query);
    
    /**
     * @brief 生成用户盐值
     * @return 盐值
     */
    QString generateUserSalt();
    
    /**
     * @brief 哈希密码
     * @param password 原始密码
     * @param salt 盐值
     * @return 哈希后的密码
     */
    QString hashPassword(const QString &password, const QString &salt);

private:
    DatabaseManager* _databaseManager;
};

#endif // USERSERVICE_H
