#include "ChatNetworkClient.h"
#include "../auth/NetworkClient.h"
#include "../utils/Logger.h"
#include <QJsonDocument>
#include <QUuid>
#include <QDateTime>

// 静态成员初始化
ChatNetworkClient* ChatNetworkClient::s_instance = nullptr;
QMutex ChatNetworkClient::s_instanceMutex;

ChatNetworkClient::ChatNetworkClient(QObject *parent)
    : QObject(parent)
    , _initialized(false)
    , _networkClient(nullptr)
    , _heartbeatTimer(new QTimer(this))
{
    // 设置心跳定时器
    _heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);
    connect(_heartbeatTimer, &QTimer::timeout, this, &ChatNetworkClient::onHeartbeatTimer);
}

ChatNetworkClient::~ChatNetworkClient()
{
    if (_heartbeatTimer) {
        _heartbeatTimer->stop();
    }
}

ChatNetworkClient* ChatNetworkClient::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new ChatNetworkClient();
        }
    }
    return s_instance;
}

bool ChatNetworkClient::initialize()
{
    QMutexLocker locker(&_mutex);
    
    if (_initialized) {
        return true;
    }
    
    // 获取NetworkClient实例
    _networkClient = NetworkClient::instance();
    if (!_networkClient) {
        LOG_ERROR("Failed to initialize ChatNetworkClient: NetworkClient not available");
        return false;
    }
    
    // 连接网络客户端的信号（使用队列连接确保线程安全）
    connect(_networkClient, &NetworkClient::messageReceived, this, &ChatNetworkClient::onNetworkResponse, Qt::QueuedConnection);
    
    // 启动心跳定时器
    _heartbeatTimer->start();
    
    _initialized = true;
    // LOG_INFO removed
    return true;
}

void ChatNetworkClient::setNetworkClient(NetworkClient* client)
{
    QMutexLocker locker(&_mutex);
    _networkClient = client;
}

void ChatNetworkClient::sendFriendRequest(const QString& userIdentifier, const QString& message, const QString& remark, const QString& group)
{
    QJsonObject data;
    data["user_identifier"] = userIdentifier;
    if (!message.isEmpty()) {
        data["message"] = message;
    }
    if (!remark.isEmpty()) {
        data["remark"] = remark;
    }
    if (!group.isEmpty()) {
        data["group"] = group;
    }

    sendRequest("friend_request", data);
}

void ChatNetworkClient::respondToFriendRequest(qint64 friendshipId, bool accept)
{
    QJsonObject data;
    data["friendship_id"] = friendshipId;
    data["accept"] = accept;
    
    sendRequest("friend_response", data);
}

void ChatNetworkClient::getFriendList()
{
    sendRequest("friend_list");
}

void ChatNetworkClient::getFriendRequests()
{
    sendRequest("friend_requests");
}

void ChatNetworkClient::removeFriend(qint64 friendId)
{
    QJsonObject data;
    data["friend_id"] = friendId;
    
    sendRequest("friend_remove", data);
}

void ChatNetworkClient::blockUser(qint64 userId)
{
    QJsonObject data;
    data["target_user_id"] = userId;
    
    sendRequest("friend_block", data);
}

void ChatNetworkClient::unblockUser(qint64 userId)
{
    QJsonObject data;
    data["target_user_id"] = userId;
    
    sendRequest("friend_unblock", data);
}

void ChatNetworkClient::searchUsers(const QString& keyword, int limit)
{
    if (!_networkClient) {
        LOG_ERROR("NetworkClient is null, cannot send search request");
        return;
    }
    
    QJsonObject data;
    data["keyword"] = keyword;
    data["limit"] = limit;
    
    sendRequest("friend_search", data);
}

void ChatNetworkClient::updateFriendNote(qint64 friendId, const QString& note)
{
    QJsonObject data;
    data["friend_id"] = friendId;
    data["note"] = note;

    sendRequest("friend_note_update", data);
}

// 好友分组相关方法实现
void ChatNetworkClient::getFriendGroups()
{
    sendRequest("friend_groups");
}

void ChatNetworkClient::createFriendGroup(const QString& groupName)
{
    QJsonObject data;
    data["group_name"] = groupName;

    sendRequest("friend_group_create", data);
}

void ChatNetworkClient::deleteFriendGroup(qint64 groupId)
{
    QJsonObject data;
    data["group_id"] = groupId;

    sendRequest("friend_group_delete", data);
}

void ChatNetworkClient::renameFriendGroup(qint64 groupId, const QString& newName)
{
    QJsonObject data;
    data["group_id"] = groupId;
    data["new_name"] = newName;

    sendRequest("friend_group_rename", data);
}

void ChatNetworkClient::moveFriendToGroup(qint64 friendId, qint64 groupId)
{
    QJsonObject data;
    data["friend_id"] = friendId;
    data["group_id"] = groupId;

    sendRequest("friend_group_move", data);
}

void ChatNetworkClient::updateOnlineStatus(const QString& status)
{
    QJsonObject data;
    data["status"] = status;
    data["client_id"] = _networkClient ? _networkClient->clientId() : QString();
    
    sendRequest("status_update", data);
}

void ChatNetworkClient::getFriendsOnlineStatus()
{
    sendRequest("status_get_friends");
}

void ChatNetworkClient::sendHeartbeat()
{
    QJsonObject data;
    data["client_id"] = _networkClient ? _networkClient->clientId() : QString();
    
    sendRequest("heartbeat", data);
}

void ChatNetworkClient::sendMessage(qint64 receiverId, const QString& content, const QString& type)
{
    QJsonObject data;
    data["receiver_id"] = receiverId;
    data["content"] = content;
    data["type"] = type;
    
    sendRequest("send_message", data);
}

void ChatNetworkClient::getChatHistory(qint64 userId, int limit, int offset)
{
    QJsonObject data;
    data["chat_user_id"] = userId;
    data["limit"] = limit;
    data["offset"] = offset;
    
    sendRequest("get_chat_history", data);
}

void ChatNetworkClient::getChatSessions()
{
    sendRequest("get_chat_sessions");
}

void ChatNetworkClient::markMessageAsRead(const QString& messageId)
{
    QJsonObject data;
    data["message_id"] = messageId;
    
    sendRequest("message_mark_read", data);
}

void ChatNetworkClient::markMessagesAsRead(const QStringList& messageIds)
{
    for (const QString& messageId : messageIds) {
        markMessageAsRead(messageId);
    }
}

void ChatNetworkClient::getUnreadMessageCount(qint64 fromUserId)
{
    QJsonObject data;
    if (fromUserId != -1) {
        data["from_user_id"] = fromUserId;
    }
    
    sendRequest("message_unread_count", data);
}

void ChatNetworkClient::getOfflineMessages()
{
    sendRequest("message_offline");
}

void ChatNetworkClient::deleteMessage(const QString& messageId)
{
    QJsonObject data;
    data["message_id"] = messageId;
    
    sendRequest("message_delete", data);
}

void ChatNetworkClient::recallMessage(const QString& messageId)
{
    QJsonObject data;
    data["message_id"] = messageId;
    
    sendRequest("message_recall", data);
}

void ChatNetworkClient::searchMessages(const QString& keyword, qint64 chatUserId, int limit)
{
    QJsonObject data;
    data["keyword"] = keyword;
    data["limit"] = limit;
    if (chatUserId != -1) {
        data["chat_user_id"] = chatUserId;
    }
    
    sendRequest("message_search", data);
}

void ChatNetworkClient::onNetworkResponse(const QJsonObject& response)
{
    QString action = response["action"].toString();
    QString requestId = response["request_id"].toString();
    
    // 检查是否为聊天相关的响应
    if (action.startsWith("friend_") || action.startsWith("message_") || 
        action.startsWith("status_") || action == "heartbeat_response" ||
        action == "send_message_response" || action == "get_chat_history_response") {
        
        if (action.startsWith("friend_")) {
            handleFriendResponse(response);
        } else if (action.startsWith("status_") || action == "heartbeat_response") {
            handleStatusResponse(response);
        } else if (action.startsWith("message_") || action == "send_message_response" || action == "get_chat_history_response") {
            handleMessageResponse(response);
        }
    } else {
        LOG_WARNING(QString("Response not identified as chat-related: %1").arg(action));
    }
    
    // 检查是否为实时通知
    if (response.contains("notification_type")) {
        handleNotification(response);
    }
}

void ChatNetworkClient::onHeartbeatTimer()
{
    QMutexLocker locker(&_mutex);

    if (_networkClient && _networkClient->isConnected()) {
        sendHeartbeat();
    }
}

void ChatNetworkClient::sendRequest(const QString& action, const QJsonObject& data)
{
    if (!_networkClient) {
        LOG_ERROR("NetworkClient not available for chat request");
        return;
    }

    if (!_networkClient->isConnected()) {
        LOG_ERROR("NetworkClient not connected to server");
        return;
    }

    QJsonObject request;
    request["action"] = action;
    request["request_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 添加会话令牌
    if (_networkClient->isAuthenticated()) {
        request["session_token"] = _networkClient->sessionToken();
    } else {
        LOG_WARNING("User not authenticated, no session token added");
    }

    // 合并数据
    for (auto it = data.begin(); it != data.end(); ++it) {
        request[it.key()] = it.value();
    }

    // 使用专门的聊天请求发送方法
    QString requestId = _networkClient->sendChatRequest(request);
    if (requestId.isEmpty()) {
        LOG_ERROR("Failed to send chat request - sendChatRequest returned empty requestId");
    }
}

void ChatNetworkClient::handleFriendResponse(const QJsonObject& response)
{
    QString action = response["action"].toString();
    bool success = response["success"].toBool();
    QString message = response["error_message"].toString();

    if (action == "friend_request_response") {
        emit friendRequestSent(success, message);
    } else if (action == "friend_response_response") {
        emit friendRequestResponded(success, message);
    } else if (action == "friend_list_response") {
        if (success) {
            QJsonArray friends = response["data"]["friends"].toArray();
            emit friendListReceived(friends);
        }
    } else if (action == "friend_requests_response") {
        if (success) {
            QJsonArray requests = response["data"]["requests"].toArray();
            emit friendRequestsReceived(requests);
        }
    } else if (action == "friend_remove_response") {
        if (success) {
            qint64 friendId = response["data"]["friend_id"].toVariant().toLongLong();
            emit friendRemoved(friendId, success);
        }
    } else if (action == "friend_block_response") {
        if (success) {
            qint64 userId = response["data"]["target_user_id"].toVariant().toLongLong();
            emit userBlocked(userId, success);
        }
    } else if (action == "friend_unblock_response") {
        if (success) {
            qint64 userId = response["data"]["target_user_id"].toVariant().toLongLong();
            emit userUnblocked(userId, success);
        }
    } else if (action == "friend_search" || action == "friend_search_response") {
        if (success) {
            QJsonArray users = response["data"]["users"].toArray();
            emit usersSearchResult(users);
        } else {
            QString errorMessage = response["error_message"].toString();
            QString errorCode = response["error_code"].toString();
            LOG_ERROR(QString("Search failed - Code: %1, Message: %2").arg(errorCode).arg(errorMessage));
            
            // 发射搜索失败信号
            emit searchFailed(errorCode, errorMessage);
        }
    } else if (action == "friend_note_update_response") {
        if (success) {
            qint64 friendId = response["data"]["friend_id"].toVariant().toLongLong();
            emit friendNoteUpdated(friendId, success);
        }
    } else if (action == "friend_groups_response") {
        if (success) {
            QJsonArray groups = response["data"]["groups"].toArray();
            emit friendGroupsReceived(groups);
        }
    } else if (action == "friend_group_create_response") {
        QString groupName = response["data"]["group_name"].toString();
        emit friendGroupCreated(groupName, success);
    } else if (action == "friend_group_delete_response") {
        qint64 groupId = response["data"]["group_id"].toVariant().toLongLong();
        emit friendGroupDeleted(groupId, success);
    } else if (action == "friend_group_rename_response") {
        qint64 groupId = response["data"]["group_id"].toVariant().toLongLong();
        QString newName = response["data"]["new_name"].toString();
        emit friendGroupRenamed(groupId, newName, success);
    } else if (action == "friend_group_move_response") {
        qint64 friendId = response["data"]["friend_id"].toVariant().toLongLong();
        qint64 groupId = response["data"]["group_id"].toVariant().toLongLong();
        emit friendMovedToGroup(friendId, groupId, success);
    }
}

void ChatNetworkClient::handleStatusResponse(const QJsonObject& response)
{
    QString action = response["action"].toString();
    bool success = response["success"].toBool();

    if (action == "status_update_response") {
        emit onlineStatusUpdated(success);
    } else if (action == "status_get_friends_response") {
        if (success) {
            QJsonArray statusList = response["data"]["friends_status"].toArray();
            emit friendsOnlineStatusReceived(statusList);
        }
    } else if (action == "heartbeat_response") {
        // 心跳响应，通常不需要特殊处理
    }
}

void ChatNetworkClient::handleMessageResponse(const QJsonObject& response)
{
    QString action = response["action"].toString();
    bool success = response["success"].toBool();
    QString message = response["error_message"].toString();

    if (action == "send_message_response") {
        if (success) {
            QString messageId = response["data"]["message_id"].toString();
            emit messageSent(messageId, success);
        } else {
            emit messageSent("", false);
        }
    } else if (action == "get_chat_history_response") {
        if (success) {
            qint64 userId = response["data"]["chat_user_id"].toVariant().toLongLong();
            QJsonArray messages = response["data"]["messages"].toArray();
            emit chatHistoryReceived(userId, messages);
        }
    } else if (action == "get_chat_sessions_response") {
        if (success) {
            QJsonArray sessions = response["data"]["sessions"].toArray();
            emit chatSessionsReceived(sessions);
        }
    } else if (action == "message_mark_read_response") {
        if (success) {
            QString messageId = response["data"]["message_id"].toString();
            emit messageMarkedAsRead(messageId, success);
        }
    } else if (action == "message_unread_count_response") {
        if (success) {
            int count = response["data"]["unread_count"].toInt();
            emit unreadMessageCountReceived(count);
        }
    } else if (action == "message_offline_response") {
        if (success) {
            QJsonArray messages = response["data"]["messages"].toArray();
            emit offlineMessagesReceived(messages);
        }
    } else if (action == "message_delete_response") {
        if (success) {
            QString messageId = response["data"]["message_id"].toString();
            emit messageDeleted(messageId, success);
        }
    } else if (action == "message_recall_response") {
        if (success) {
            QString messageId = response["data"]["message_id"].toString();
            emit messageRecalled(messageId, success);
        }
    } else if (action == "message_search_response") {
        if (success) {
            QJsonArray messages = response["data"]["messages"].toArray();
            emit messagesSearchResult(messages);
        }
    }
}

void ChatNetworkClient::handleNotification(const QJsonObject& notification)
{
    QString notificationType = notification["notification_type"].toString();

    if (notificationType == "friend_request") {
        emit friendRequestReceived(notification);
    } else if (notificationType == "friend_added") {
        emit friendAdded(notification);
    } else if (notificationType == "friend_status_changed") {
        qint64 friendId = notification["user_id"].toVariant().toLongLong();
        QString status = notification["status"].toString();
        QString lastSeen = notification["last_seen"].toString();
        emit friendStatusChanged(friendId, status, lastSeen);
    } else if (notificationType == "new_message") {
        emit messageReceived(notification);
    } else if (notificationType == "message_status_updated") {
        QString messageId = notification["message_id"].toString();
        QString status = notification["status"].toString();
        emit messageStatusUpdated(messageId, status);
    }
}
