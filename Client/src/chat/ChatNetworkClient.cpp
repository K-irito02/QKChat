#include "ChatNetworkClient.h"
#include "../auth/NetworkClient.h"
#include "../auth/AuthManager.h"
#include "../utils/Logger.h"
#include <QJsonDocument>
#include <QUuid>
#include <QDateTime>
#include <QThread>

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

bool ChatNetworkClient::isAuthenticated() const
{
    QMutexLocker locker(&_mutex);
    if (_networkClient) {
        return _networkClient->isAuthenticated();
    }
    return false;
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

void ChatNetworkClient::respondToFriendRequest(qint64 requestId, bool accept)
{
    QJsonObject data;
    data["accept"] = accept;
    data["friend_request_id"] = requestId;  // 使用friend_request_id避免与网络request_id冲突
    
    sendRequest("friend_response", data);
}

void ChatNetworkClient::respondToFriendRequestWithSettings(qint64 requestId, bool accept, const QString& note, const QString& groupName)
{
    QJsonObject data;
    data["accept"] = accept;
    data["friend_request_id"] = requestId;  // 使用friend_request_id避免与网络request_id冲突
    data["note"] = note;
    data["group_name"] = groupName;
    
    sendRequest("friend_response", data);
}

void ChatNetworkClient::ignoreFriendRequest(qint64 requestId)
{
    QJsonObject data;
    data["friend_request_id"] = requestId;
    
    sendRequest("friend_ignore", data);
}

void ChatNetworkClient::getFriendList()
{
    sendRequest("friend_list");
}

void ChatNetworkClient::getFriendRequests()
{
    sendRequest("friend_requests");
}

void ChatNetworkClient::deleteFriendRequestNotification(qint64 requestId)
{
    QJsonObject data;
    data["request_id"] = requestId;
    
    sendRequest("delete_friend_request_notification", data);
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
    if (!_networkClient) {
        return;
    }
    
    if (!_networkClient->isConnected()) {
        return;
    }
    
    if (!_networkClient->isAuthenticated()) {
        return;
    }
    
    QJsonObject data;
    data["client_id"] = _networkClient->clientId();
    data["user_id"] = _networkClient->userId();  // 添加用户ID
    
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
        action == "send_message_response" || action == "get_chat_history_response" ||
        action == "get_chat_history" || action == "friend_ignore_response") {
        
        if (action.startsWith("friend_")) {
            handleFriendResponse(response);
        } else if (action.startsWith("status_") || action == "heartbeat_response") {
            handleStatusResponse(response);
        } else if (action.startsWith("message_") || action == "send_message_response" || action == "get_chat_history_response" || action == "get_chat_history") {
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

    if (!_networkClient) {
        return;
    }

    // 修改条件：只要NetworkClient存在且已认证就发送心跳
    if (_networkClient && _networkClient->isAuthenticated()) {
        sendHeartbeat();
    }
}

void ChatNetworkClient::sendRequest(const QString& action, const QJsonObject& data)
{
    if (!_networkClient) {
        return;
    }

    if (!_networkClient->isConnected()) {
        return;
    }

    QJsonObject request;
    request["action"] = action;
    request["request_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 添加会话令牌
    if (_networkClient->isAuthenticated()) {
        request["session_token"] = _networkClient->sessionToken();
    }

    // 合并数据，但保护特定字段不被覆盖
    for (auto it = data.begin(); it != data.end(); ++it) {
        // 不要覆盖特定字段，因为它们包含数据库的ID
        if (it.key() != "friend_request_id" && it.key() != "request_id") {
            request[it.key()] = it.value();
        }
    }
    
    // 单独处理特殊字段
    if (data.contains("friend_request_id")) {
        request["friend_request_id"] = data["friend_request_id"];
    }

    // 使用专门的聊天请求发送方法
    _networkClient->sendChatRequest(request);
}

void ChatNetworkClient::handleFriendResponse(const QJsonObject& response)
{
    QString action = response["action"].toString();
    bool success = response["success"].toBool();
    QString message = response["error_message"].toString();

    if (action == "friend_request_response") {
        emit friendRequestSent(success, message);
    } else if (action == "friend_response" || action == "friend_response_response") {
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
    } else if (action == "friend_ignore_response") {
        if (success) {
            qint64 requestId = response["data"]["request_id"].toVariant().toLongLong();
            QString message = response["data"]["message"].toString();
        
            emit friendRequestIgnored(requestId, 0, "", "", "");
        } else {
            QString errorMessage = response["error_message"].toString();
            QString errorCode = response["error_code"].toString();
            LOG_ERROR(QString("忽略好友请求失败 - 代码: %1, 消息: %2").arg(errorCode).arg(errorMessage));
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
    } else if (action == "friend_request_accepted") {
        // 处理好友请求被接受的通知
        qint64 requestId = response["request_id"].toVariant().toLongLong();
        qint64 acceptedByUserId = response["accepted_by_user_id"].toVariant().toLongLong();
        QString acceptedByUsername = response["accepted_by_username"].toString();
        QString acceptedByDisplayName = response["accepted_by_display_name"].toString();
        QString note = response["note"].toString();
        QString groupName = response["group_name"].toString();
        QString timestamp = response["timestamp"].toString();
        
        // 发射好友请求被接受信号
        emit friendRequestAccepted(requestId, acceptedByUserId, acceptedByUsername, 
                                 acceptedByDisplayName, note, groupName, timestamp);
    } else if (action == "friend_request_rejected") {
        // 处理好友请求被拒绝的通知
        qint64 requestId = response["request_id"].toVariant().toLongLong();
        qint64 rejectedByUserId = response["rejected_by_user_id"].toVariant().toLongLong();
        QString rejectedByUsername = response["rejected_by_username"].toString();
        QString rejectedByDisplayName = response["rejected_by_display_name"].toString();
        QString timestamp = response["timestamp"].toString();
        
        // 发射好友请求被拒绝信号
        emit friendRequestRejected(requestId, rejectedByUserId, rejectedByUsername, 
                                 rejectedByDisplayName, timestamp);
    } else if (action == "friend_request_ignored") {
        // 处理好友请求被忽略的通知
        qint64 requestId = response["request_id"].toVariant().toLongLong();
        qint64 ignoredByUserId = response["ignored_by_user_id"].toVariant().toLongLong();
        QString ignoredByUsername = response["ignored_by_username"].toString();
        QString ignoredByDisplayName = response["ignored_by_display_name"].toString();
        QString timestamp = response["timestamp"].toString();
        
        // 发射好友请求被忽略信号
        emit friendRequestIgnored(requestId, ignoredByUserId, ignoredByUsername, 
                                ignoredByDisplayName, timestamp);
    } else if (action == "friend_request_notification") {
            // 处理好友请求通知（包括离线消息）
            qint64 requestId = response["request_id"].toVariant().toLongLong();
            QString fromUsername = response["from_username"].toString();
            QString fromDisplayName = response["from_display_name"].toString();
            QString notificationType = response["notification_type"].toString();
            QString message = response["message"].toString();
            QString timestamp = response["timestamp"].toString();
            bool isOfflineMessage = response["is_offline_message"].toBool();

            // 发射好友请求通知信号
            emit friendRequestNotification(requestId, 0, fromUsername,
                                         fromDisplayName, notificationType, message, timestamp, isOfflineMessage);
    } else if (action == "friend_list_update") {
        // 处理好友列表更新通知
        // 自动刷新好友列表
        getFriendList();
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
    } else if (action == "get_chat_history_response" || action == "get_chat_history") {
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
    } else if (notificationType == "friend_list_update") {
        // 自动刷新好友列表
        getFriendList();
        emit friendListUpdated();
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
