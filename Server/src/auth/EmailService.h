#ifndef EMAILSERVICE_H
#define EMAILSERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "../database/DatabaseManager.h"
#include "../database/RedisClient.h"

// 前向声明
class SmtpClient;

/**
 * @brief 邮件服务类
 * 
 * 提供SMTP邮件发送功能，主要用于发送验证码邮件。
 * 支持多种邮件服务商，包含发送频率限制和防滥用机制。
 */
class EmailService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 邮件发送结果枚举
     */
    enum SendResult {
        Success,
        InvalidEmail,
        RateLimited,
        SmtpError,
        NetworkError,
        ConfigError,
        DatabaseError
    };
    Q_ENUM(SendResult)

    /**
     * @brief 验证码类型枚举
     */
    enum CodeType {
        Registration,
        PasswordReset,
        EmailChange
    };
    Q_ENUM(CodeType)

    explicit EmailService(QObject *parent = nullptr);
    ~EmailService();
    
    /**
     * @brief 初始化邮件服务
     * @param smtpServer SMTP服务器地址
     * @param smtpPort SMTP端口
     * @param username 邮箱用户名
     * @param password 邮箱密码或授权码
     * @param useTLS 是否使用TLS加密
     * @return 初始化是否成功
     */
    bool initialize(const QString &smtpServer, int smtpPort, 
                   const QString &username, const QString &password, bool useTLS = true);
    
    /**
     * @brief 发送验证码邮件
     * @param email 目标邮箱
     * @param codeType 验证码类型
     * @return 发送结果
     */
    SendResult sendVerificationCode(const QString &email, CodeType codeType = Registration);
    
    /**
     * @brief 发送验证码邮件（使用指定验证码）
     * @param email 目标邮箱
     * @param code 验证码
     * @param codeType 验证码类型
     * @return 发送结果
     */
    SendResult sendVerificationCode(const QString &email, const QString &code, CodeType codeType = Registration);
    

    
    /**
     * @brief 发送自定义邮件
     * @param email 目标邮箱
     * @param subject 邮件主题
     * @param content 邮件内容
     * @param isHtml 是否为HTML格式
     * @return 发送结果
     */
    SendResult sendCustomEmail(const QString &email, const QString &subject, 
                              const QString &content, bool isHtml = true);
    

    
    /**
     * @brief 获取发送结果描述
     * @param result 发送结果
     * @return 描述文本
     */
    static QString getSendResultDescription(SendResult result);
    

    
    /**
     * @brief 设置验证码有效期
     * @param minutes 有效期（分钟）
     */
    void setCodeExpiration(int minutes = 5);
    
    /**
     * @brief 清理过期的验证码和发送记录
     */
    void cleanup();

signals:
    /**
     * @brief 邮件发送完成信号
     * @param email 目标邮箱
     * @param result 发送结果
     */
    void emailSent(const QString &email, SendResult result);
    
    /**
     * @brief 邮件发送错误信号
     * @param email 目标邮箱
     * @param error 错误信息
     */
    void emailError(const QString &email, const QString &error);

private slots:
    void onCleanupTimer();

private:

    
    /**
     * @brief 获取邮件模板
     * @param codeType 验证码类型
     * @param code 验证码
     * @return 邮件内容
     */
    QString getEmailTemplate(CodeType codeType, const QString &code);
    
    /**
     * @brief 获取邮件主题
     * @param codeType 验证码类型
     * @return 邮件主题
     */
    QString getEmailSubject(CodeType codeType);
    

    

    
    /**
     * @brief 实际发送邮件
     * @param email 目标邮箱
     * @param subject 邮件主题
     * @param content 邮件内容
     * @param isHtml 是否为HTML格式
     * @return 发送是否成功
     */
    bool sendEmailInternal(const QString &email, const QString &subject, 
                          const QString &content, bool isHtml);
    
    /**
     * @brief 发送验证码邮件（防重复发送）
     * @param email 目标邮箱
     * @param subject 邮件主题
     * @param content 邮件内容
     * @param isHtml 是否为HTML格式
     * @return 发送是否成功
     */
    bool sendVerificationCodeEmail(const QString &email, const QString &subject, 
                                  const QString &content, bool isHtml);
    


private slots:
    void onEmailSent(const QString &messageId);
    void onEmailFailed(const QString &messageId, const QString &error);

private:
    DatabaseManager* _databaseManager;
    RedisClient* _redisClient;
    QTimer* _cleanupTimer;
    SmtpClient* _smtpClient;
    
    // SMTP配置
    QString _smtpServer;
    int _smtpPort;
    QString _username;
    QString _password;
    bool _useTLS;
    bool _initialized;
    
    // 验证码有效期（分钟）
    int _codeExpiration;
};

#endif // EMAILSERVICE_H
