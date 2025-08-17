#ifndef CHATNETWORKCLIENT_H
#define CHATNETWORKCLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMutex>
#include "../utils/Logger.h"

// 前向声明
class NetworkClient;

/**
 * @brief 聊天网络客户端扩展
 * 负责处理聊天相关的网络通信功能
 */
class ChatNetworkClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY authenticationStateChanged)

public:
    explicit ChatNetworkClient(QObject *parent = nullptr);
    ~ChatNetworkClient();

    /**
     * @brief 获取单例实例
     */
    static ChatNetworkClient* instance();

    /**
     * @brief 检查是否已认证
     */
    Q_INVOKABLE bool isAuthenticated() const;

    /**
     * @brief 初始化聊天客户端
     */
    Q_INVOKABLE bool initialize();

    /**
     * @brief 设置网络客户端实例
     */
    void setNetworkClient(NetworkClient* client);

    // 好友管理相关方法
    /**
     * @brief 发送好友请求
     */
    Q_INVOKABLE void sendFriendRequest(const QString& userIdentifier, const QString& message = QString(),
                          const QString& remark = QString(), const QString& group = QString());

    /**
     * @brief 响应好友请求
     */
    Q_INVOKABLE void respondToFriendRequest(qint64 requestId, bool accept);

    /**
     * @brief 响应好友请求（带备注和分组设置）
     */
    Q_INVOKABLE void respondToFriendRequestWithSettings(qint64 requestId, bool accept, 
                                                       const QString& note = QString(), 
                                                       const QString& groupName = QString());

    /**
     * @brief 忽略好友请求
     */
    Q_INVOKABLE void ignoreFriendRequest(qint64 requestId);

    /**
     * @brief 获取好友列表
     */
    Q_INVOKABLE void getFriendList();

    /**
     * @brief 获取待处理的好友请求
     */
    Q_INVOKABLE void getFriendRequests();

    /**
     * @brief 删除好友请求通知
     */
    Q_INVOKABLE void deleteFriendRequestNotification(qint64 requestId);

    /**
     * @brief 删除好友
     */
    void removeFriend(qint64 friendId);

    /**
     * @brief 屏蔽用户
     */
    void blockUser(qint64 userId);

    /**
     * @brief 取消屏蔽用户
     */
    void unblockUser(qint64 userId);

    /**
     * @brief 搜索用户
     */
    Q_INVOKABLE void searchUsers(const QString& keyword, int limit = 20);

    /**
     * @brief 更新好友备注
     */
    void updateFriendNote(qint64 friendId, const QString& note);

    // 好友分组相关方法
    /**
     * @brief 获取好友分组列表
     */
    Q_INVOKABLE void getFriendGroups();

    /**
     * @brief 创建好友分组
     */
    void createFriendGroup(const QString& groupName);

    /**
     * @brief 删除好友分组
     */
    void deleteFriendGroup(qint64 groupId);

    /**
     * @brief 重命名好友分组
     */
    void renameFriendGroup(qint64 groupId, const QString& newName);

    /**
     * @brief 移动好友到指定分组
     */
    void moveFriendToGroup(qint64 friendId, qint64 groupId);

    // 在线状态相关方法
    /**
     * @brief 更新在线状态
     */
    void updateOnlineStatus(const QString& status);

    /**
     * @brief 获取好友在线状态
     */
    void getFriendsOnlineStatus();

    /**
     * @brief 发送心跳
     */
    void sendHeartbeat();

    // 消息相关方法
    /**
     * @brief 发送消息
     */
    void sendMessage(qint64 receiverId, const QString& content, const QString& type = "text");

    /**
     * @brief 获取聊天历史
     */
    void getChatHistory(qint64 userId, int limit = 50, int offset = 0);

    /**
     * @brief 获取聊天会话列表
     */
    void getChatSessions();

    /**
     * @brief 标记消息为已读
     */
    void markMessageAsRead(const QString& messageId);

    /**
     * @brief 批量标记消息为已读
     */
    void markMessagesAsRead(const QStringList& messageIds);

    /**
     * @brief 获取未读消息数量
     */
    void getUnreadMessageCount(qint64 fromUserId = -1);

    /**
     * @brief 获取离线消息
     */
    void getOfflineMessages();

    /**
     * @brief 删除消息
     */
    void deleteMessage(const QString& messageId);

    /**
     * @brief 撤回消息
     */
    void recallMessage(const QString& messageId);

    /**
     * @brief 搜索消息
     */
    void searchMessages(const QString& keyword, qint64 chatUserId = -1, int limit = 20);

signals:
    // 好友请求相关信号
    void friendRequestSent(bool success, const QString& message);
    void friendRequestResponded(bool success, const QString& message);
    void friendRequestAccepted(qint64 requestId, qint64 acceptedByUserId, const QString& acceptedByUsername,
                             const QString& acceptedByDisplayName, const QString& note, const QString& groupName, const QString& timestamp);
    void friendRequestRejected(qint64 requestId, qint64 rejectedByUserId, const QString& rejectedByUsername,
                                 const QString& rejectedByDisplayName, const QString& timestamp);
    void friendRequestIgnored(qint64 requestId, qint64 ignoredByUserId, const QString& ignoredByUsername,
                                const QString& ignoredByDisplayName, const QString& timestamp);
    void friendRequestNotification(qint64 requestId, qint64 fromUserId, const QString& fromUsername,
                                 const QString& fromDisplayName, const QString& notificationType,
                                 const QString& message, const QString& timestamp, bool isOfflineMessage);
    void friendListReceived(const QJsonArray& friends);
    void friendListUpdated();  // 新增：好友列表更新信号
    void friendRequestsReceived(const QJsonArray& requests);
    void friendRemoved(qint64 friendId, bool success);
    void userBlocked(qint64 userId, bool success);
    void userUnblocked(qint64 userId, bool success);
    void usersSearchResult(const QJsonArray& users);
    void searchFailed(const QString& errorCode, const QString& errorMessage);
    void friendNoteUpdated(qint64 friendId, bool success);

    // 好友分组信号
    void friendGroupsReceived(const QJsonArray& groups);
    void friendGroupCreated(const QString& groupName, bool success);
    void friendGroupDeleted(qint64 groupId, bool success);
    void friendGroupRenamed(qint64 groupId, const QString& newName, bool success);
    void friendMovedToGroup(qint64 friendId, qint64 groupId, bool success);

    // 好友状态变化信号
    void friendRequestReceived(const QJsonObject& request);
    void friendAdded(const QJsonObject& friendInfo);
    void friendStatusChanged(qint64 friendId, const QString& status, const QString& lastSeen);

    // 在线状态信号
    void onlineStatusUpdated(bool success);
    void friendsOnlineStatusReceived(const QJsonArray& statusList);

    // 消息信号
    void messageSent(const QString& messageId, bool success);
    void messageReceived(const QJsonObject& message);
    void chatHistoryReceived(qint64 userId, const QJsonArray& messages);
    void chatSessionsReceived(const QJsonArray& sessions);
    void messageMarkedAsRead(const QString& messageId, bool success);
    void unreadMessageCountReceived(int count);
    void offlineMessagesReceived(const QJsonArray& messages);
    void messageDeleted(const QString& messageId, bool success);
    void messageRecalled(const QString& messageId, bool success);
    void messagesSearchResult(const QJsonArray& messages);

    // 消息状态更新信号
    void messageStatusUpdated(const QString& messageId, const QString& status);

    // 认证状态变化信号
    void authenticationStateChanged(bool isAuthenticated);

private slots:
    /**
     * @brief 处理网络响应
     */
    void onNetworkResponse(const QJsonObject& response);

    /**
     * @brief 处理心跳定时器
     */
    void onHeartbeatTimer();

private:
    /**
     * @brief 发送请求
     */
    void sendRequest(const QString& action, const QJsonObject& data = QJsonObject());

    /**
     * @brief 处理好友相关响应
     */
    void handleFriendResponse(const QJsonObject& response);

    /**
     * @brief 处理状态相关响应
     */
    void handleStatusResponse(const QJsonObject& response);

    /**
     * @brief 处理消息相关响应
     */
    void handleMessageResponse(const QJsonObject& response);

    /**
     * @brief 处理实时通知
     */
    void handleNotification(const QJsonObject& notification);

    static ChatNetworkClient* s_instance;
    static QMutex s_instanceMutex;
    
    bool _initialized;
    NetworkClient* _networkClient;
    QTimer* _heartbeatTimer;
    
    mutable QMutex _mutex;
    
    // 心跳间隔（毫秒）
    static const int HEARTBEAT_INTERVAL = 10000; // 10秒（临时用于测试）
};

#endif // CHATNETWORKCLIENT_H
