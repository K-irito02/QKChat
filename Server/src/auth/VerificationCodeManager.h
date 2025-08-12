#ifndef VERIFICATIONCODEMANAGER_H
#define VERIFICATIONCODEMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include "../database/DatabaseManager.h"
#include "../database/RedisClient.h"

/**
 * @brief 验证码管理类
 * 
 * 统一管理验证码的生成、存储、验证和清理
 * 提供清晰的接口和错误处理
 */
class VerificationCodeManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 验证码类型枚举
     */
    enum CodeType {
        Registration,
        PasswordReset,
        EmailChange
    };
    Q_ENUM(CodeType)

    /**
     * @brief 验证结果枚举
     */
    enum VerificationResult {
        Success,
        InvalidCode,
        ExpiredCode,
        AlreadyUsed,
        DatabaseError,
        RedisError
    };
    Q_ENUM(VerificationResult)

    explicit VerificationCodeManager(QObject *parent = nullptr);
    ~VerificationCodeManager();
    
    /**
     * @brief 获取单例实例
     * @return 单例实例指针
     */
    static VerificationCodeManager* instance();

    /**
     * @brief 生成并保存验证码
     * @param email 邮箱地址
     * @param codeType 验证码类型
     * @param expireMinutes 过期时间（分钟）
     * @return 生成的验证码，失败返回空字符串
     */
    QString generateAndSaveCode(const QString &email, CodeType codeType, int expireMinutes = 5);
    
    /**
     * @brief 生成并保存验证码（带IP地址限制）
     * @param email 邮箱地址
     * @param ipAddress IP地址
     * @param codeType 验证码类型
     * @param expireMinutes 过期时间（分钟）
     * @return 生成的验证码，失败返回空字符串
     */
    QString generateAndSaveCode(const QString &email, const QString &ipAddress, CodeType codeType, int expireMinutes = 5);
    
    /**
     * @brief 生成并保存验证码（内部使用，不包含IP限制）
     * @param email 邮箱地址
     * @param codeType 验证码类型
     * @param expireMinutes 过期时间（分钟）
     * @return 生成的验证码，失败返回空字符串
     */
    QString generateAndSaveCodeInternal(const QString &email, CodeType codeType, int expireMinutes = 5);

    /**
     * @brief 验证验证码
     * @param email 邮箱地址
     * @param code 验证码
     * @param codeType 验证码类型
     * @return 验证结果
     */
    VerificationResult verifyCode(const QString &email, const QString &code, CodeType codeType);

    /**
     * @brief 清理过期的验证码
     * @return 清理的数量
     */
    int cleanupExpiredCodes();

    /**
     * @brief 获取验证结果描述
     * @param result 验证结果
     * @return 描述文本
     */
    static QString getVerificationResultDescription(VerificationResult result);

    /**
     * @brief 检查邮箱是否在频率限制内
     * @param email 邮箱地址
     * @param minIntervalSeconds 最小间隔（秒）
     * @return 是否允许发送
     */
    bool isAllowedToSend(const QString &email, int minIntervalSeconds = 60);
    
    /**
     * @brief 获取剩余等待时间（秒）
     * @param email 邮箱地址
     * @param minIntervalSeconds 最小间隔时间（秒）
     * @return 剩余等待时间，如果为0表示可以立即发送
     */
    int getRemainingWaitTime(const QString &email, int minIntervalSeconds = 60);
    
    /**
     * @brief 检查IP地址是否在频率限制内
     * @param ipAddress IP地址
     * @param minIntervalSeconds 最小间隔（秒）
     * @return 是否允许发送
     */
    bool isIPAllowedToSend(const QString &ipAddress, int minIntervalSeconds = 60);
    
    /**
     * @brief 获取IP地址剩余等待时间（秒）
     * @param ipAddress IP地址
     * @param minIntervalSeconds 最小间隔时间（秒）
     * @return 剩余等待时间，如果为0表示可以立即发送
     */
    int getIPRemainingWaitTime(const QString &ipAddress, int minIntervalSeconds = 60);

private:
    /**
     * @brief 生成6位数字验证码
     * @return 验证码字符串
     */
    QString generateCode();

    /**
     * @brief 保存验证码到数据库
     * @param email 邮箱地址
     * @param code 验证码
     * @param codeType 验证码类型
     * @param expireMinutes 过期时间（分钟）
     * @return 是否成功
     */
    bool saveToDatabase(const QString &email, const QString &code, CodeType codeType, int expireMinutes);

    /**
     * @brief 缓存验证码到Redis
     * @param email 邮箱地址
     * @param code 验证码
     * @param expireMinutes 过期时间（分钟）
     * @return 是否成功
     */
    bool cacheToRedis(const QString &email, const QString &code, int expireMinutes);

    /**
     * @brief 从Redis验证验证码
     * @param email 邮箱地址
     * @param code 验证码
     * @return 验证结果
     */
    VerificationResult verifyFromRedis(const QString &email, const QString &code);

    /**
     * @brief 从数据库验证验证码
     * @param email 邮箱地址
     * @param code 验证码
     * @param codeType 验证码类型
     * @return 验证结果
     */
    VerificationResult verifyFromDatabase(const QString &email, const QString &code, CodeType codeType);

    /**
     * @brief 标记验证码为已使用
     * @param email 邮箱地址
     * @param code 验证码
     * @param codeType 验证码类型
     * @return 是否成功
     */
    bool markCodeAsUsed(const QString &email, const QString &code, CodeType codeType);

    /**
     * @brief 验证码类型转字符串
     * @param codeType 验证码类型
     * @return 类型字符串
     */
    QString codeTypeToString(CodeType codeType);

    /**
     * @brief 记录发送历史
     * @param email 邮箱地址
     */
    void recordSendHistory(const QString &email);
    
    /**
     * @brief 记录IP地址发送历史
     * @param ipAddress IP地址
     */
    void recordIPSendHistory(const QString &ipAddress);
    
    /**
     * @brief 使旧验证码失效，避免覆盖问题
     * @param email 邮箱地址
     * @param codeType 验证码类型
     */
    void invalidateOldCodes(const QString &email, CodeType codeType);

private:
    DatabaseManager* _databaseManager;
    RedisClient* _redisClient;
    QMutex _mutex;
    
    // 发送历史记录（内存缓存）
    QMap<QString, QDateTime> _lastSendTime;
    
    // IP地址发送历史记录（内存缓存）
    QMap<QString, QDateTime> _lastIPSendTime;
};

#endif // VERIFICATIONCODEMANAGER_H 