#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSet>
#include <QTimer>
#include <QMutex>
#include "../auth/UserService.h"
#include "../auth/EmailService.h"
#include "../auth/VerificationCodeManager.h"
#include "../database/RedisClient.h"

/**
 * @brief 协议处理器类
 * 
 * 负责处理客户端发送的各种协议消息，包括登录、注册、验证码等请求。
 * 提供统一的消息处理接口和响应格式。
 */
class ProtocolHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 消息类型枚举
     */
    enum MessageType {
        Unknown,
        Login,
        Register,
        SendVerificationCode,
        Heartbeat,
        Logout
    };
    Q_ENUM(MessageType)

    explicit ProtocolHandler(EmailService *emailService = nullptr, QObject *parent = nullptr);
    ~ProtocolHandler();
    
    /**
     * @brief 处理客户端消息
     * @param message JSON消息
     * @param clientId 客户端ID
     * @param clientIP 客户端IP地址
     * @return 响应消息
     */
    QJsonObject handleMessage(const QJsonObject &message, const QString &clientId, const QString &clientIP);
    
    /**
     * @brief 处理登录请求
     * @param request 登录请求
     * @param clientId 客户端ID
     * @param clientIP 客户端IP地址
     * @return 登录响应
     */
    QJsonObject handleLoginRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP);
    
    /**
     * @brief 处理注册请求
     * @param request 注册请求
     * @param clientId 客户端ID
     * @param clientIP 客户端IP地址
     * @return 注册响应
     */
    QJsonObject handleRegisterRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP);
    
    /**
     * @brief 处理验证码发送请求
     * @param request 验证码请求
     * @param clientId 客户端ID
     * @param clientIP 客户端IP地址
     * @return 验证码响应
     */
    QJsonObject handleVerificationCodeRequest(const QJsonObject &request, const QString &clientId, const QString &clientIP);
    
    /**
     * @brief 处理心跳请求
     * @param request 心跳请求
     * @param clientId 客户端ID
     * @return 心跳响应
     */
    QJsonObject handleHeartbeatRequest(const QJsonObject &request, const QString &clientId);
    
    /**
     * @brief 处理登出请求
     * @param request 登出请求
     * @param clientId 客户端ID
     * @param userId 用户ID
     * @return 登出响应
     */
    QJsonObject handleLogoutRequest(const QJsonObject &request, const QString &clientId, qint64 userId);
    
    /**
     * @brief 验证请求格式
     * @param request 请求消息
     * @param requiredFields 必需字段列表
     * @return 验证结果和错误信息
     */
    QPair<bool, QString> validateRequest(const QJsonObject &request, const QStringList &requiredFields);
    
    /**
     * @brief 获取消息类型
     * @param action 动作字符串
     * @return 消息类型
     */
    static MessageType getMessageType(const QString &action);
    
    /**
     * @brief 创建成功响应
     * @param requestId 请求ID
     * @param action 动作
     * @param data 响应数据
     * @return 响应JSON对象
     */
    static QJsonObject createSuccessResponse(const QString &requestId, const QString &action, const QJsonObject &data = QJsonObject());
    
    /**
     * @brief 创建错误响应
     * @param requestId 请求ID
     * @param action 动作
     * @param errorCode 错误代码
     * @param errorMessage 错误消息
     * @return 响应JSON对象
     */
    static QJsonObject createErrorResponse(const QString &requestId, const QString &action, const QString &errorCode, const QString &errorMessage);

signals:
    /**
     * @brief 用户登录成功信号
     * @param userId 用户ID
     * @param clientId 客户端ID
     * @param sessionToken 会话令牌
     */
    void userLoggedIn(qint64 userId, const QString &clientId, const QString &sessionToken);
    
    /**
     * @brief 用户注册成功信号
     * @param userId 用户ID
     * @param username 用户名
     * @param email 邮箱
     */
    void userRegistered(qint64 userId, const QString &username, const QString &email);
    
    /**
     * @brief 用户登出信号
     * @param userId 用户ID
     * @param clientId 客户端ID
     */
    void userLoggedOut(qint64 userId, const QString &clientId);

private:
    /**
     * @brief 生成会话令牌
     * @param userId 用户ID
     * @return 会话令牌
     */
    QString generateSessionToken(qint64 userId);
    
    /**
     * @brief 记录登录日志
     * @param userId 用户ID
     * @param username 用户名
     * @param email 邮箱
     * @param success 是否成功
     * @param clientIP 客户端IP
     * @param errorMessage 错误信息
     */
    void logLoginAttempt(qint64 userId, const QString &username, const QString &email, 
                        bool success, const QString &clientIP, const QString &errorMessage = "");
    
    /**
     * @brief 验证会话令牌
     * @param userId 用户ID
     * @param token 会话令牌
     * @return 验证是否成功
     */
    bool validateSessionToken(qint64 userId, const QString &token);

public:
    /**
     * @brief 获取用户服务实例
     * @return 用户服务实例
     */
    UserService* userService() const { return _userService; }

private:
    UserService* _userService;
    EmailService* _emailService;
    RedisClient* _redisClient;
    
    // 请求去重机制
    QSet<QString> _processedRequests;  // 已处理的请求ID集合
    QMutex _requestMutex;              // 请求处理互斥锁
    QTimer* _cleanupTimer;             // 定期清理过期请求ID的定时器
};

#endif // PROTOCOLHANDLER_H
