#ifndef FRIENDSERVICE_H
#define FRIENDSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutex>
#include <QDateTime>
#include "../utils/Logger.h"

/**
 * @brief 好友管理服务类
 * 负责处理好友请求、好友列表、好友状态等功能
 */
class FriendService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 好友请求结果枚举
     */
    enum FriendRequestResult {
        Success,
        AlreadyFriends,
        AlreadyRequested,
        SelfRequest,
        UserNotFound,
        UserBlocked,
        DatabaseError
    };
    Q_ENUM(FriendRequestResult)

    /**
     * @brief 好友状态枚举
     */
    enum FriendshipStatus {
        Pending,
        Accepted,
        Blocked,
        Deleted
    };
    Q_ENUM(FriendshipStatus)

    explicit FriendService(QObject *parent = nullptr);
    ~FriendService();

    /**
     * @brief 获取单例实例
     */
    static FriendService* instance();

    /**
     * @brief 初始化服务
     */
    bool initialize();

    /**
     * @brief 发送好友请求
     * @param fromUserId 发起用户ID
     * @param toUserIdentifier 目标用户标识（用户名、邮箱或用户ID）
     * @param message 附加消息
     * @param remark 好友备注
     * @param groupName 好友分组名称
     * @return 请求结果
     */
    FriendRequestResult sendFriendRequest(qint64 fromUserId, const QString& toUserIdentifier,
                                        const QString& message = QString(),
                                        const QString& remark = QString(),
                                        const QString& groupName = QString());

    /**
     * @brief 响应好友请求
     * @param userId 当前用户ID
     * @param friendshipId 好友关系ID
     * @param accept 是否接受
     * @return 是否成功
     */
    bool respondToFriendRequest(qint64 userId, qint64 friendshipId, bool accept);

    /**
     * @brief 获取好友列表
     * @param userId 用户ID
     * @return 好友列表JSON数组
     */
    QJsonArray getFriendList(qint64 userId);

    /**
     * @brief 获取待处理的好友请求
     * @param userId 用户ID
     * @return 好友请求列表JSON数组
     */
    QJsonArray getPendingFriendRequests(qint64 userId);

    /**
     * @brief 删除好友
     * @param userId 当前用户ID
     * @param friendId 好友用户ID
     * @return 是否成功
     */
    bool removeFriend(qint64 userId, qint64 friendId);

    /**
     * @brief 屏蔽用户
     * @param userId 当前用户ID
     * @param targetUserId 目标用户ID
     * @return 是否成功
     */
    bool blockUser(qint64 userId, qint64 targetUserId);

    /**
     * @brief 取消屏蔽用户
     * @param userId 当前用户ID
     * @param targetUserId 目标用户ID
     * @return 是否成功
     */
    bool unblockUser(qint64 userId, qint64 targetUserId);

    /**
     * @brief 检查两个用户是否为好友
     * @param userId1 用户1 ID
     * @param userId2 用户2 ID
     * @return 是否为好友
     */
    bool areFriends(qint64 userId1, qint64 userId2);

    /**
     * @brief 获取好友关系状态
     * @param userId1 用户1 ID
     * @param userId2 用户2 ID
     * @return 好友关系状态
     */
    FriendshipStatus getFriendshipStatus(qint64 userId1, qint64 userId2);

    /**
     * @brief 搜索用户
     * @param keyword 搜索关键词（用户名、邮箱或显示名）
     * @param currentUserId 当前用户ID（用于过滤）
     * @param limit 结果限制数量
     * @return 用户搜索结果JSON数组
     */
    QJsonArray searchUsers(const QString& keyword, qint64 currentUserId, int limit = 20);

    // 好友分组相关方法
    /**
     * @brief 获取用户的好友分组列表
     * @param userId 用户ID
     * @return 分组列表JSON数组
     */
    QJsonArray getFriendGroups(qint64 userId);

    /**
     * @brief 创建好友分组
     * @param userId 用户ID
     * @param groupName 分组名称
     * @return 是否成功
     */
    bool createFriendGroup(qint64 userId, const QString& groupName);

    /**
     * @brief 删除好友分组
     * @param userId 用户ID
     * @param groupId 分组ID
     * @return 是否成功
     */
    bool deleteFriendGroup(qint64 userId, qint64 groupId);

    /**
     * @brief 重命名好友分组
     * @param userId 用户ID
     * @param groupId 分组ID
     * @param newName 新名称
     * @return 是否成功
     */
    bool renameFriendGroup(qint64 userId, qint64 groupId, const QString& newName);

    /**
     * @brief 移动好友到指定分组
     * @param userId 用户ID
     * @param friendId 好友ID
     * @param groupId 分组ID
     * @return 是否成功
     */
    bool moveFriendToGroup(qint64 userId, qint64 friendId, qint64 groupId);

    /**
     * @brief 更新好友备注
     * @param userId 当前用户ID
     * @param friendId 好友用户ID
     * @param note 备注内容
     * @return 是否成功
     */
    bool updateFriendNote(qint64 userId, qint64 friendId, const QString& note);

signals:
    /**
     * @brief 好友请求发送信号
     * @param fromUserId 发起用户ID
     * @param toUserId 目标用户ID
     * @param friendshipId 好友关系ID
     * @param message 附加消息
     */
    void friendRequestSent(qint64 fromUserId, qint64 toUserId, qint64 friendshipId, const QString& message);

    /**
     * @brief 好友请求响应信号
     * @param friendshipId 好友关系ID
     * @param fromUserId 发起用户ID
     * @param toUserId 目标用户ID
     * @param accepted 是否接受
     */
    void friendRequestResponded(qint64 friendshipId, qint64 fromUserId, qint64 toUserId, bool accepted);

    /**
     * @brief 好友删除信号
     * @param userId1 用户1 ID
     * @param userId2 用户2 ID
     */
    void friendRemoved(qint64 userId1, qint64 userId2);

private:
    /**
     * @brief 根据标识符查找用户ID
     * @param identifier 用户标识符（用户名、邮箱或用户ID）
     * @return 用户ID，未找到返回-1
     */
    qint64 findUserByIdentifier(const QString& identifier);

    /**
     * @brief 创建好友请求通知
     * @param friendshipId 好友关系ID
     * @param fromUserId 发起用户ID
     * @param toUserId 目标用户ID
     * @param type 通知类型
     * @param message 附加消息
     * @return 是否成功
     */
    bool createFriendNotification(qint64 friendshipId, qint64 fromUserId, qint64 toUserId,
                                 const QString& type, const QString& message = QString());

    /**
     * @brief 获取好友关系状态（内部方法，不加锁）
     * @param userId1 用户1 ID
     * @param userId2 用户2 ID
     * @return 好友关系状态
     */
    FriendshipStatus getFriendshipStatusInternal(qint64 userId1, qint64 userId2);

    /**
     * @brief 获取数据库连接
     */
    QSqlDatabase getDatabase();

    static FriendService* s_instance;
    static QMutex s_instanceMutex;
    
    bool _initialized;
    mutable QMutex _mutex;
};

#endif // FRIENDSERVICE_H
