#ifndef USERREGISTRATIONSERVICE_H
#define USERREGISTRATIONSERVICE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QMutex>
#include <QAtomicInt>
#include "../utils/ThreadSafeSingleton.h"

class DatabaseManager;
class UserIdGenerator;
class VerificationCodeManager;

/**
 * @brief 用户注册服务
 * 
 * 提供完整的用户注册功能，包括：
 * - 自动生成9位数字用户ID
 * - 用户信息验证和存储
 * - 邮箱验证码处理
 * - 线程安全的注册流程
 */
class UserRegistrationService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 注册结果枚举
     */
    enum RegistrationResult {
        Success = 0,            // 注册成功
        InvalidInput,           // 输入参数无效
        UsernameExists,         // 用户名已存在
        EmailExists,            // 邮箱已存在
        InvalidVerificationCode, // 验证码无效
        DatabaseError,          // 数据库错误
        UserIdGenerationFailed, // 用户ID生成失败
        PasswordTooWeak,        // 密码强度不足
        EmailFormatInvalid,     // 邮箱格式无效
        UsernameFormatInvalid   // 用户名格式无效
    };

    /**
     * @brief 注册请求结构
     */
    struct RegistrationRequest {
        QString username;           // 用户名
        QString email;             // 邮箱
        QString password;          // 密码
        QString displayName;       // 显示名称（可选）
        QString verificationCode;  // 邮箱验证码
        
        // 验证请求数据是否有效
        bool isValid() const;
    };

    /**
     * @brief 注册响应结构
     */
    struct RegistrationResponse {
        RegistrationResult result;  // 注册结果
        QString userId;            // 生成的用户ID
        QString message;           // 结果消息
        QJsonObject userData;      // 用户数据（成功时）
        
        // 转换为JSON对象
        QJsonObject toJson() const;
    };

    /**
     * @brief 获取单例实例
     */
    static UserRegistrationService* instance();

    /**
     * @brief 注册新用户
     * @param request 注册请求
     * @return 注册响应
     */
    RegistrationResponse registerUser(const RegistrationRequest& request);

    /**
     * @brief 检查用户名是否可用
     * @param username 用户名
     * @return true表示可用，false表示已被占用
     */
    bool isUsernameAvailable(const QString& username);

    /**
     * @brief 检查邮箱是否可用
     * @param email 邮箱地址
     * @return true表示可用，false表示已被占用
     */
    bool isEmailAvailable(const QString& email);

    /**
     * @brief 验证用户名格式
     * @param username 用户名
     * @return true表示格式正确
     */
    static bool isValidUsername(const QString& username);

    /**
     * @brief 验证邮箱格式
     * @param email 邮箱地址
     * @return true表示格式正确
     */
    static bool isValidEmail(const QString& email);

    /**
     * @brief 验证密码强度
     * @param password 密码
     * @return true表示密码强度足够
     */
    static bool isValidPassword(const QString& password);

    /**
     * @brief 获取注册结果的描述信息
     * @param result 注册结果
     * @return 描述信息
     */
    static QString getResultDescription(RegistrationResult result);

    /**
     * @brief 获取注册统计信息
     * @return 包含统计信息的JSON对象
     */
    QJsonObject getRegistrationStatistics();

signals:
    /**
     * @brief 用户注册成功信号
     * @param userId 用户ID
     * @param username 用户名
     * @param email 邮箱
     */
    void userRegistered(const QString& userId, const QString& username, const QString& email);

    /**
     * @brief 注册失败信号
     * @param result 失败原因
     * @param email 邮箱
     */
    void registrationFailed(RegistrationResult result, const QString& email);

private:
    explicit UserRegistrationService(QObject* parent = nullptr);
    ~UserRegistrationService();

    /**
     * @brief 验证注册请求
     * @param request 注册请求
     * @return 验证结果
     */
    RegistrationResult validateRegistrationRequest(const RegistrationRequest& request);

    /**
     * @brief 检查用户名和邮箱唯一性
     * @param username 用户名
     * @param email 邮箱
     * @return 检查结果
     */
    RegistrationResult checkUniqueness(const QString& username, const QString& email);

    /**
     * @brief 验证邮箱验证码
     * @param email 邮箱
     * @param code 验证码
     * @return 验证结果
     */
    RegistrationResult verifyEmailCode(const QString& email, const QString& code);

    /**
     * @brief 创建用户记录
     * @param request 注册请求
     * @param userId 生成的用户ID
     * @return 创建结果
     */
    RegistrationResult createUserRecord(const RegistrationRequest& request, const QString& userId);

    /**
     * @brief 生成密码哈希和盐值
     * @param password 原始密码
     * @param salt 输出参数，生成的盐值
     * @return 密码哈希值
     */
    QString generatePasswordHash(const QString& password, QString& salt);

    /**
     * @brief 构建用户数据JSON对象
     * @param userId 用户ID
     * @param request 注册请求
     * @return 用户数据JSON对象
     */
    QJsonObject buildUserDataJson(const QString& userId, const RegistrationRequest& request);

private:
    DatabaseManager* _databaseManager;
    UserIdGenerator* _userIdGenerator;
    VerificationCodeManager* _verificationCodeManager;
    
    mutable QMutex _mutex;
    
    // 统计信息
    QAtomicInt _totalRegistrations;
    QAtomicInt _successfulRegistrations;
    QAtomicInt _failedRegistrations;
    
    friend class ThreadSafeSingleton<UserRegistrationService>;
};

#endif // USERREGISTRATIONSERVICE_H