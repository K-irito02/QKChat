#ifndef CHATPROTOCOLHANDLER_H
#define CHATPROTOCOLHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include "../utils/Logger.h"

// 前向声明
class FriendService;
class OnlineStatusService;
class MessageService;

/**
 * @brief 聊天协议处理器
 * 负责处理聊天相关的网络协议请求
 */
class ChatProtocolHandler : public QObject
{
    Q_OBJECT

public:
    explicit ChatProtocolHandler(QObject *parent = nullptr);
    ~ChatProtocolHandler();

    /**
     * @brief 获取单例实例
     */
    static ChatProtocolHandler* instance();

    /**
     * @brief 初始化处理器
     */
    bool initialize();

    /**
     * @brief 处理聊天相关请求
     * @param request 请求JSON对象
     * @param clientIP 客户端IP地址
     * @param userId 用户ID（已认证用户）
     * @return 响应JSON对象
     */
    QJsonObject handleChatRequest(const QJsonObject& request, const QString& clientIP, qint64 userId);

private:
    /**
     * @brief 处理好友请求相关操作
     */
    QJsonObject handleFriendOperations(const QJsonObject& request, qint64 userId);
    QJsonObject handleFriendRequest(const QJsonObject& request, qint64 userId);
    QJsonObject handleFriendResponse(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetFriendList(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetFriendRequests(const QJsonObject& request, qint64 userId);
    QJsonObject handleRemoveFriend(const QJsonObject& request, qint64 userId);
    QJsonObject handleBlockUser(const QJsonObject& request, qint64 userId);
    QJsonObject handleUnblockUser(const QJsonObject& request, qint64 userId);
    QJsonObject handleSearchUsers(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetFriendGroups(const QJsonObject& request, qint64 userId);
    QJsonObject handleCreateFriendGroup(const QJsonObject& request, qint64 userId);
    QJsonObject handleDeleteFriendGroup(const QJsonObject& request, qint64 userId);
    QJsonObject handleDeleteFriendRequestNotification(const QJsonObject& request, qint64 userId);
    QJsonObject handleRenameFriendGroup(const QJsonObject& request, qint64 userId);
    QJsonObject handleMoveFriendToGroup(const QJsonObject& request, qint64 userId);
    QJsonObject handleUpdateFriendNote(const QJsonObject& request, qint64 userId);

    /**
     * @brief 处理在线状态相关操作
     */
    QJsonObject handleStatusResponse(const QJsonObject& request, qint64 userId);
    QJsonObject handleUpdateStatus(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetFriendsStatus(const QJsonObject& request, qint64 userId);
    QJsonObject handleHeartbeat(const QJsonObject& request, qint64 userId);

    /**
     * @brief 处理消息相关操作
     */
    QJsonObject handleMessageResponse(const QJsonObject& request, qint64 userId);
    QJsonObject handleSendMessage(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetChatHistory(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetChatSessions(const QJsonObject& request, qint64 userId);
    QJsonObject handleMarkMessageRead(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetUnreadCount(const QJsonObject& request, qint64 userId);
    QJsonObject handleGetOfflineMessages(const QJsonObject& request, qint64 userId);
    QJsonObject handleDeleteMessage(const QJsonObject& request, qint64 userId);
    QJsonObject handleRecallMessage(const QJsonObject& request, qint64 userId);
    QJsonObject handleSearchMessages(const QJsonObject& request, qint64 userId);

    /**
     * @brief 创建成功响应
     */
    QJsonObject createSuccessResponse(const QString& requestId, const QString& action, const QJsonObject& data = QJsonObject());

    /**
     * @brief 创建错误响应
     */
    QJsonObject createErrorResponse(const QString& requestId, const QString& action, const QString& errorCode, const QString& errorMessage);

    /**
     * @brief 验证请求参数
     */
    bool validateRequest(const QJsonObject& request, const QStringList& requiredFields, QString& errorMessage);

    /**
     * @brief 记录请求日志
     */
    void logRequest(const QString& action, const QString& requestId, qint64 userId, const QString& clientIP);

    static ChatProtocolHandler* s_instance;
    static QMutex s_instanceMutex;
    
    bool _initialized;
    mutable QMutex _mutex;
    
    // 服务实例
    FriendService* _friendService;
    OnlineStatusService* _statusService;
    MessageService* _messageService;
};

#endif // CHATPROTOCOLHANDLER_H
