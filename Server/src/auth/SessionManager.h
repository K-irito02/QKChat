#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QMap>
#include <QSet>
#include "../database/RedisClient.h"
#include "../utils/Logger.h"

/**
 * @brief 服务器端会话管理器类
 * 
 * 负责管理用户会话令牌的生命周期，包括创建、验证、更新和清理。
 * 支持多设备登录、滑动窗口过期策略和会话活动监控。
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 会话信息结构
     */
    struct SessionInfo {
        qint64 userId;
        QString deviceId;
        QDateTime createdAt;
        QDateTime lastActivity;
        QDateTime expiresAt;
        QString clientId;
        QString ipAddress;
        bool isValid;
        
        SessionInfo() : userId(0), isValid(false) {}
        
        SessionInfo(qint64 uid, const QString& device, const QString& client, const QString& ip)
            : userId(uid), deviceId(device), clientId(client), ipAddress(ip), isValid(true) {
            createdAt = QDateTime::currentDateTime();
            lastActivity = createdAt;
        }
        
        bool isExpired() const {
            return QDateTime::currentDateTime() > expiresAt;
        }
        
        void updateActivity() {
            lastActivity = QDateTime::currentDateTime();
        }
    };

    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager();
    
    /**
     * @brief 获取单例实例
     * @return 会话管理器实例
     */
    static SessionManager* instance();
    
    /**
     * @brief 清理单例实例
     */
    static void cleanup();

    /**
     * @brief 创建新会话
     * @param userId 用户ID
     * @param sessionToken 会话令牌
     * @param deviceId 设备ID
     * @param clientId 客户端ID
     * @param ipAddress IP地址
     * @param rememberMe 是否记住登录
     * @return 创建是否成功
     */
    bool createSession(qint64 userId, const QString& sessionToken, const QString& deviceId,
                      const QString& clientId, const QString& ipAddress, bool rememberMe = false);

    /**
     * @brief 验证会话令牌
     * @param sessionToken 会话令牌
     * @return 会话信息，如果无效则返回空结构
     */
    SessionInfo validateSession(const QString& sessionToken);

    /**
     * @brief 更新会话活动时间
     * @param sessionToken 会话令牌
     * @return 更新是否成功
     */
    bool updateSessionActivity(const QString& sessionToken);

    /**
     * @brief 销毁会话
     * @param sessionToken 会话令牌
     * @return 销毁是否成功
     */
    bool destroySession(const QString& sessionToken);

    /**
     * @brief 销毁用户的所有会话
     * @param userId 用户ID
     * @return 销毁的会话数量
     */
    int destroyUserSessions(qint64 userId);

    /**
     * @brief 获取用户的活动会话列表
     * @param userId 用户ID
     * @return 会话令牌列表
     */
    QStringList getUserActiveSessions(qint64 userId);

    /**
     * @brief 检查用户会话数量是否超过限制
     * @param userId 用户ID
     * @return 是否超过限制
     */
    bool isUserSessionLimitExceeded(qint64 userId);

    /**
     * @brief 清理过期会话
     * @return 清理的会话数量
     */
    int cleanupExpiredSessions();

    /**
     * @brief 获取会话统计信息
     * @return 统计信息字符串
     */
    QString getSessionStats() const;

signals:
    /**
     * @brief 会话创建信号
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     */
    void sessionCreated(const QString& sessionToken, qint64 userId);
    
    /**
     * @brief 会话过期信号
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     */
    void sessionExpired(const QString& sessionToken, qint64 userId);
    
    /**
     * @brief 会话销毁信号
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     */
    void sessionDestroyed(const QString& sessionToken, qint64 userId);

private slots:
    /**
     * @brief 定期清理过期会话
     */
    void onCleanupTimer();

private:
    /**
     * @brief 从Redis键中提取会话令牌
     * @param redisKey Redis键
     * @return 会话令牌
     */
    QString extractSessionTokenFromKey(const QString& redisKey);
    
    /**
     * @brief 解析会话数据
     * @param sessionData 会话数据字符串
     * @return 会话信息结构
     */
    SessionInfo parseSessionData(const QString& sessionData);
    
    /**
     * @brief 序列化会话数据
     * @param sessionInfo 会话信息
     * @return 序列化后的字符串
     */
    QString serializeSessionData(const SessionInfo& sessionInfo);
    
    /**
     * @brief 计算会话过期时间
     * @param rememberMe 是否记住登录
     * @return 过期时间
     */
    QDateTime calculateExpiryTime(bool rememberMe);
    
    /**
     * @brief 加载配置
     */
    void loadConfiguration();

private:
    static SessionManager* s_instance;
    static QMutex s_mutex;
    
    RedisClient* _redisClient;
    QTimer* _cleanupTimer;
    
    mutable QMutex _statsMutex;
    qint64 _totalSessionsCreated;
    qint64 _totalSessionsExpired;
    qint64 _totalSessionsDestroyed;
    qint64 _totalCacheHits;
    qint64 _totalCacheMisses;
    
    // 配置参数
    int _defaultTimeout;
    int _rememberMeTimeout;
    int _activityUpdateInterval;
    int _maxSessionsPerUser;
    int _cleanupInterval;
    bool _slidingWindow;
    bool _multiDeviceSupport;
};

#endif // SESSIONMANAGER_H
