#ifndef ONLINESTATUSSERVICE_H
#define ONLINESTATUSSERVICE_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMutex>
#include <QMap>
#include <QDateTime>
#include "../utils/Logger.h"

/**
 * @brief 在线状态管理服务类
 * 负责管理用户在线状态、状态广播、心跳检测等功能
 */
class OnlineStatusService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 用户在线状态枚举
     */
    enum OnlineStatus {
        Online,
        Offline,
        Away,
        Busy,
        Invisible
    };
    Q_ENUM(OnlineStatus)

    /**
     * @brief 用户状态信息结构
     */
    struct UserStatusInfo {
        qint64 userId;
        OnlineStatus status;
        QDateTime lastSeen;
        QString clientId;
        QString deviceInfo;
        QString ipAddress;
        
        UserStatusInfo() : userId(-1), status(Offline) {}
        UserStatusInfo(qint64 id, OnlineStatus s, const QDateTime& lastSeen = QDateTime::currentDateTime())
            : userId(id), status(s), lastSeen(lastSeen) {}
    };

    explicit OnlineStatusService(QObject *parent = nullptr);
    ~OnlineStatusService();

    /**
     * @brief 获取单例实例
     */
    static OnlineStatusService* instance();

    /**
     * @brief 初始化服务
     */
    bool initialize();

    /**
     * @brief 用户上线
     * @param userId 用户ID
     * @param clientId 客户端ID
     * @param deviceInfo 设备信息
     * @param ipAddress IP地址
     * @return 是否成功
     */
    bool userOnline(qint64 userId, const QString& clientId, const QString& deviceInfo = QString(), const QString& ipAddress = QString());

    /**
     * @brief 用户下线
     * @param userId 用户ID
     * @param clientId 客户端ID（可选，用于多设备登录）
     * @return 是否成功
     */
    bool userOffline(qint64 userId, const QString& clientId = QString());

    /**
     * @brief 更新用户状态
     * @param userId 用户ID
     * @param status 新状态
     * @param clientId 客户端ID
     * @return 是否成功
     */
    bool updateUserStatus(qint64 userId, OnlineStatus status, const QString& clientId = QString());

    /**
     * @brief 更新用户心跳
     * @param userId 用户ID
     * @param clientId 客户端ID
     * @return 是否成功
     */
    bool updateHeartbeat(qint64 userId, const QString& clientId);

    /**
     * @brief 获取用户在线状态
     * @param userId 用户ID
     * @return 用户状态信息
     */
    UserStatusInfo getUserStatus(qint64 userId);

    /**
     * @brief 获取多个用户的在线状态
     * @param userIds 用户ID列表
     * @return 用户状态信息映射
     */
    QMap<qint64, UserStatusInfo> getUsersStatus(const QList<qint64>& userIds);

    /**
     * @brief 获取用户好友的在线状态
     * @param userId 用户ID
     * @return 好友状态列表JSON数组
     */
    QJsonArray getFriendsOnlineStatus(qint64 userId);

    /**
     * @brief 获取在线用户数量
     * @return 在线用户数量
     */
    int getOnlineUserCount();

    /**
     * @brief 获取所有在线用户列表
     * @return 在线用户列表
     */
    QList<qint64> getOnlineUsers();

    /**
     * @brief 检查用户是否在线
     * @param userId 用户ID
     * @return 是否在线
     */
    bool isUserOnline(qint64 userId);

    /**
     * @brief 广播状态变化给好友
     * @param userId 用户ID
     * @param status 新状态
     */
    void broadcastStatusToFriends(qint64 userId, OnlineStatus status);

    /**
     * @brief 清理过期的在线状态
     */
    void cleanupExpiredStatus();

    /**
     * @brief 处理用户上线后的离线消息推送
     * @param userId 用户ID
     */
    void processOfflineMessages(qint64 userId);

    /**
     * @brief 状态枚举转字符串
     */
    static QString statusToString(OnlineStatus status);

    /**
     * @brief 字符串转状态枚举
     */
    static OnlineStatus stringToStatus(const QString& statusStr);

signals:
    /**
     * @brief 用户状态变化信号
     * @param userId 用户ID
     * @param oldStatus 旧状态
     * @param newStatus 新状态
     */
    void userStatusChanged(qint64 userId, OnlineStatus oldStatus, OnlineStatus newStatus);

    /**
     * @brief 用户上线信号
     * @param userId 用户ID
     * @param clientId 客户端ID
     */
    void userWentOnline(qint64 userId, const QString& clientId);

    /**
     * @brief 用户下线信号
     * @param userId 用户ID
     * @param clientId 客户端ID
     */
    void userWentOffline(qint64 userId, const QString& clientId);

private slots:
    /**
     * @brief 定时清理过期状态
     */
    void onCleanupTimer();

private:
    /**
     * @brief 获取数据库连接
     */
    QSqlDatabase getDatabase();

    /**
     * @brief 更新数据库中的用户状态
     */
    bool updateStatusInDatabase(qint64 userId, OnlineStatus status, const QString& clientId, 
                               const QString& deviceInfo = QString(), const QString& ipAddress = QString());

    /**
     * @brief 从数据库加载用户状态
     */
    UserStatusInfo loadStatusFromDatabase(qint64 userId);

    /**
     * @brief 获取用户好友列表
     */
    QList<qint64> getUserFriends(qint64 userId);

    static OnlineStatusService* s_instance;
    static QMutex s_instanceMutex;
    
    bool _initialized;
    mutable QMutex _mutex;
    
    // 内存缓存的用户状态信息
    QMap<qint64, UserStatusInfo> _userStatusCache;
    
    // 清理定时器
    QTimer* _cleanupTimer;
    
    // 心跳超时时间（秒）
    static const int HEARTBEAT_TIMEOUT = 30; // 30秒（临时用于测试）
    
    // 清理间隔（毫秒）
    static const int CLEANUP_INTERVAL = 30000; // 30秒
};

#endif // ONLINESTATUSSERVICE_H
