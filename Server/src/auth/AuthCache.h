#ifndef AUTHCACHE_H
#define AUTHCACHE_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QReadWriteLock>

/**
 * @brief 身份验证缓存类
 * 
 * 提供高性能的用户认证信息缓存，减少数据库查询频率，
 * 支持会话管理、用户信息缓存和自动过期清理。
 */
class AuthCache : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 用户会话信息结构
     */
    struct SessionInfo {
        qint64 userId;
        QString username;
        QString clientId;
        QDateTime loginTime;
        QDateTime lastActivity;
        QDateTime expiryTime;
        QString ipAddress;
        bool isValid;
        
        SessionInfo() : userId(-1), isValid(false) {}
        
        bool isExpired() const {
            return QDateTime::currentDateTime() > expiryTime;
        }
        
        void updateActivity() {
            lastActivity = QDateTime::currentDateTime();
        }
    };

    /**
     * @brief 用户信息缓存结构
     */
    struct UserInfo {
        qint64 userId;
        QString username;
        QString email;
        QString passwordHash;
        bool isActive;
        QDateTime cacheTime;
        
        UserInfo() : userId(-1), isActive(false) {}
        
        bool isExpired(int cacheTimeoutSeconds = 300) const {
            return cacheTime.secsTo(QDateTime::currentDateTime()) > cacheTimeoutSeconds;
        }
    };

    explicit AuthCache(QObject *parent = nullptr);
    ~AuthCache();
    
    /**
     * @brief 获取单例实例
     * @return AuthCache实例
     */
    static AuthCache* instance();
    
    /**
     * @brief 初始化认证缓存
     * @param sessionTimeoutMinutes 会话超时时间（分钟）
     * @param userCacheTimeoutMinutes 用户信息缓存超时时间（分钟）
     * @param cleanupIntervalMinutes 清理间隔（分钟）
     * @return 初始化是否成功
     */
    bool initialize(int sessionTimeoutMinutes = 30, 
                   int userCacheTimeoutMinutes = 5,
                   int cleanupIntervalMinutes = 1);
    
    // 会话管理
    /**
     * @brief 创建用户会话
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     * @param username 用户名
     * @param clientId 客户端ID
     * @param ipAddress IP地址
     * @param timeoutMinutes 超时时间（分钟）
     * @return 创建是否成功
     */
    bool createSession(const QString& sessionToken, qint64 userId, 
                      const QString& username, const QString& clientId,
                      const QString& ipAddress, int timeoutMinutes = 30);
    
    /**
     * @brief 验证会话
     * @param sessionToken 会话令牌
     * @return 会话信息，无效时返回空SessionInfo
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
    
    // 用户信息缓存
    /**
     * @brief 缓存用户信息
     * @param userId 用户ID
     * @param username 用户名
     * @param email 邮箱
     * @param passwordHash 密码哈希
     * @param isActive 是否激活
     */
    void cacheUserInfo(qint64 userId, const QString& username, 
                      const QString& email, const QString& passwordHash, 
                      bool isActive = true);
    
    /**
     * @brief 获取缓存的用户信息
     * @param userId 用户ID
     * @return 用户信息，未找到时返回空UserInfo
     */
    UserInfo getCachedUserInfo(qint64 userId);
    
    /**
     * @brief 根据用户名获取缓存的用户信息
     * @param username 用户名
     * @return 用户信息，未找到时返回空UserInfo
     */
    UserInfo getCachedUserInfoByUsername(const QString& username);
    
    /**
     * @brief 移除用户信息缓存
     * @param userId 用户ID
     */
    void removeUserInfoCache(qint64 userId);
    
    // 统计信息
    /**
     * @brief 获取活跃会话数量
     * @return 活跃会话数量
     */
    int getActiveSessionCount() const;
    
    /**
     * @brief 获取缓存的用户数量
     * @return 缓存的用户数量
     */
    int getCachedUserCount() const;
    
    /**
     * @brief 获取缓存统计信息
     * @return 统计信息JSON对象
     */
    QJsonObject getCacheStatistics() const;
    
    /**
     * @brief 清理过期数据
     */
    void cleanup();

signals:
    /**
     * @brief 会话创建信号
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     */
    void sessionCreated(const QString& sessionToken, qint64 userId);
    
    /**
     * @brief 会话销毁信号
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     */
    void sessionDestroyed(const QString& sessionToken, qint64 userId);
    
    /**
     * @brief 会话过期信号
     * @param sessionToken 会话令牌
     * @param userId 用户ID
     */
    void sessionExpired(const QString& sessionToken, qint64 userId);

private slots:
    /**
     * @brief 定时清理过期数据
     */
    void performCleanup();

private:
    static AuthCache* s_instance;
    static QMutex s_instanceMutex;
    
    // 会话存储
    QHash<QString, SessionInfo> _sessions;
    mutable QReadWriteLock _sessionsLock;
    
    // 用户信息缓存
    QHash<qint64, UserInfo> _userInfoCache;
    QHash<QString, qint64> _usernameToIdMap;
    mutable QReadWriteLock _userCacheLock;
    
    // 配置参数
    int _sessionTimeoutMinutes;
    int _userCacheTimeoutMinutes;
    int _cleanupIntervalMinutes;
    
    // 清理定时器
    QTimer* _cleanupTimer;
    
    // 统计信息
    mutable QMutex _statsMutex;
    qint64 _totalSessionsCreated;
    qint64 _totalSessionsDestroyed;
    qint64 _totalSessionsExpired;
    qint64 _totalCacheHits;
    qint64 _totalCacheMisses;
};

#endif // AUTHCACHE_H