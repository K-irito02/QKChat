#include "ChatProtocolHandler.h"
#include "FriendService.h"
#include "OnlineStatusService.h"
#include "MessageService.h"
#include <QJsonDocument>
#include <QUuid>

// 静态成员初始化
ChatProtocolHandler* ChatProtocolHandler::s_instance = nullptr;
QMutex ChatProtocolHandler::s_instanceMutex;

ChatProtocolHandler::ChatProtocolHandler(QObject *parent)
    : QObject(parent)
    , _initialized(false)
    , _friendService(nullptr)
    , _statusService(nullptr)
    , _messageService(nullptr)
{
}

ChatProtocolHandler::~ChatProtocolHandler()
{
}

ChatProtocolHandler* ChatProtocolHandler::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new ChatProtocolHandler();
        }
    }
    return s_instance;
}

bool ChatProtocolHandler::initialize()
{
    QMutexLocker locker(&_mutex);
    
    if (_initialized) {
        LOG_INFO("ChatProtocolHandler already initialized");
        return true;
    }
    
    // Initializing chat protocol handler
    
    // 获取服务实例
    _friendService = FriendService::instance();
    LOG_INFO(QString("FriendService instance: %1").arg(_friendService ? "valid" : "null"));
    
    _statusService = OnlineStatusService::instance();
    LOG_INFO(QString("OnlineStatusService instance: %1").arg(_statusService ? "valid" : "null"));
    
    _messageService = MessageService::instance();
    LOG_INFO(QString("MessageService instance: %1").arg(_messageService ? "valid" : "null"));
    
    if (!_friendService || !_statusService || !_messageService) {
        LOG_ERROR("Failed to initialize ChatProtocolHandler: service instances not available");
        return false;
    }
    
    // 初始化服务
    bool friendInit = _friendService->initialize();
    LOG_INFO(QString("FriendService initialization: %1").arg(friendInit ? "success" : "failed"));
    
    bool statusInit = _statusService->initialize();
    LOG_INFO(QString("OnlineStatusService initialization: %1").arg(statusInit ? "success" : "failed"));
    
    bool messageInit = _messageService->initialize();
    LOG_INFO(QString("MessageService initialization: %1").arg(messageInit ? "success" : "failed"));
    
    if (!friendInit || !statusInit || !messageInit) {
        LOG_ERROR("Failed to initialize ChatProtocolHandler: service initialization failed");
        return false;
    }
    
    _initialized = true;
    LOG_INFO("ChatProtocolHandler initialized successfully");
    // Chat protocol handler initialized
    return true;
}

QJsonObject ChatProtocolHandler::handleChatRequest(const QJsonObject& request, const QString& clientIP, qint64 userId)
{
    // Processing chat request
    
    QString action = request["action"].toString();
    QString requestId = request["request_id"].toString();
    
    LOG_INFO(QString("Received request - Action: %1, RequestID: %2, UserID: %3, ClientIP: %4")
            .arg(action).arg(requestId).arg(userId).arg(clientIP));
    
    if (requestId.isEmpty()) {
        requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        LOG_INFO(QString("Generated new request ID: %1").arg(requestId));
    }
    
    // 记录请求日志
    logRequest(action, requestId, userId, clientIP);
    
    QJsonObject result;
    
    // 根据动作类型分发处理
    if (action.startsWith("friend_")) {
        LOG_INFO("Routing to friend operations");
        result = handleFriendOperations(request, userId);
    } else if (action.startsWith("status_") || action == "heartbeat") {
        LOG_INFO("Routing to status operations");
        result = handleStatusResponse(request, userId);
    } else if (action.startsWith("message_") || action == "send_message" || action == "get_chat_history") {
        LOG_INFO("Routing to message operations");
        result = handleMessageResponse(request, userId);
    } else {
        LOG_ERROR(QString("Unknown action: %1").arg(action));
        result = createErrorResponse(requestId, action, "INVALID_ACTION", "Unknown action: " + action);
    }
    
    LOG_INFO(QString("Request handled - Success: %1").arg(result["success"].toBool()));
    // Chat request processed
    
    return result;
}

QJsonObject ChatProtocolHandler::handleFriendOperations(const QJsonObject& request, qint64 userId)
{
    QString action = request["action"].toString();
    QString requestId = request["request_id"].toString();
    
    // Processing friend operations
    LOG_INFO(QString("Action: %1").arg(action));
    LOG_INFO(QString("Request ID: %1").arg(requestId));
    LOG_INFO(QString("User ID: %1").arg(userId));
    
    QJsonObject result;
    
    if (action == "friend_request") {
        LOG_INFO("Handling friend_request");
        result = handleFriendRequest(request, userId);
    } else if (action == "friend_response") {
        LOG_INFO("Handling friend_response");
        result = handleFriendResponse(request, userId);
    } else if (action == "friend_list") {
        LOG_INFO("Handling friend_list");
        result = handleGetFriendList(request, userId);
    } else if (action == "friend_requests") {
        LOG_INFO("Handling friend_requests");
        result = handleGetFriendRequests(request, userId);
    } else if (action == "friend_remove") {
        LOG_INFO("Handling friend_remove");
        result = handleRemoveFriend(request, userId);
    } else if (action == "friend_block") {
        LOG_INFO("Handling friend_block");
        result = handleBlockUser(request, userId);
    } else if (action == "friend_unblock") {
        LOG_INFO("Handling friend_unblock");
        result = handleUnblockUser(request, userId);
    } else if (action == "friend_search") {
        LOG_INFO("Handling friend_search");
        result = handleSearchUsers(request, userId);
    } else if (action == "friend_note_update") {
        LOG_INFO("Handling friend_note_update");
        result = handleUpdateFriendNote(request, userId);
    } else if (action == "friend_groups") {
        LOG_INFO("Handling friend_groups");
        result = handleGetFriendGroups(request, userId);
    } else if (action == "friend_group_create") {
        LOG_INFO("Handling friend_group_create");
        result = handleCreateFriendGroup(request, userId);
    } else if (action == "friend_group_delete") {
        LOG_INFO("Handling friend_group_delete");
        result = handleDeleteFriendGroup(request, userId);
    } else if (action == "friend_group_rename") {
        LOG_INFO("Handling friend_group_rename");
        result = handleRenameFriendGroup(request, userId);
    } else if (action == "friend_group_move") {
        LOG_INFO("Handling friend_group_move");
        result = handleMoveFriendToGroup(request, userId);
    } else {
        LOG_ERROR(QString("Unknown friend action: %1").arg(action));
        result = createErrorResponse(requestId, action, "INVALID_ACTION", "Unknown friend action: " + action);
    }
    
    LOG_INFO(QString("Operation result - Success: %1").arg(result["success"].toBool()));
    // Friend operations processed
    
    return result;
}

QJsonObject ChatProtocolHandler::handleFriendRequest(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"user_identifier"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }
    
    QString userIdentifier = request["user_identifier"].toString();
    QString message = request["message"].toString();
    QString remark = request["remark"].toString();
    QString groupName = request["group"].toString();

    FriendService::FriendRequestResult result = _friendService->sendFriendRequest(userId, userIdentifier, message, remark, groupName);
    
    switch (result) {
        case FriendService::Success:
            return createSuccessResponse(requestId, action, {{"message", "Friend request sent successfully"}});
        case FriendService::AlreadyFriends:
            return createErrorResponse(requestId, action, "ALREADY_FRIENDS", "You are already friends with this user");
        case FriendService::AlreadyRequested:
            return createErrorResponse(requestId, action, "ALREADY_REQUESTED", "Friend request already sent");
        case FriendService::SelfRequest:
            return createErrorResponse(requestId, action, "SELF_REQUEST", "Cannot send friend request to yourself");
        case FriendService::UserNotFound:
            return createErrorResponse(requestId, action, "USER_NOT_FOUND", "User not found");
        case FriendService::UserBlocked:
            return createErrorResponse(requestId, action, "BLOCKED", "Cannot send friend request to blocked user");
        default:
            return createErrorResponse(requestId, action, "DATABASE_ERROR", "Failed to send friend request");
    }
}

QJsonObject ChatProtocolHandler::handleFriendResponse(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"friendship_id", "accept"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }
    
    qint64 friendshipId = request["friendship_id"].toVariant().toLongLong();
    bool accept = request["accept"].toBool();
    
    bool success = _friendService->respondToFriendRequest(userId, friendshipId, accept);
    
    if (success) {
        QJsonObject data;
        data["friendship_id"] = friendshipId;
        data["accepted"] = accept;
        data["message"] = accept ? "Friend request accepted" : "Friend request rejected";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to respond to friend request");
    }
}

QJsonObject ChatProtocolHandler::handleGetFriendList(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    QJsonArray friendList = _friendService->getFriendList(userId);
    
    QJsonObject data;
    data["friends"] = friendList;
    data["count"] = friendList.size();
    
    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleGetFriendRequests(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    QJsonArray requestList = _friendService->getPendingFriendRequests(userId);
    
    QJsonObject data;
    data["requests"] = requestList;
    data["count"] = requestList.size();
    
    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleRemoveFriend(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"friend_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }
    
    qint64 friendId = request["friend_id"].toVariant().toLongLong();
    
    bool success = _friendService->removeFriend(userId, friendId);
    
    if (success) {
        QJsonObject data;
        data["friend_id"] = friendId;
        data["message"] = "Friend removed successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to remove friend");
    }
}

QJsonObject ChatProtocolHandler::handleBlockUser(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    
    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"target_user_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }
    
    qint64 targetUserId = request["target_user_id"].toVariant().toLongLong();
    
    bool success = _friendService->blockUser(userId, targetUserId);
    
    if (success) {
        QJsonObject data;
        data["target_user_id"] = targetUserId;
        data["message"] = "User blocked successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to block user");
    }
}

QJsonObject ChatProtocolHandler::handleUnblockUser(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"target_user_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 targetUserId = request["target_user_id"].toVariant().toLongLong();

    bool success = _friendService->unblockUser(userId, targetUserId);

    if (success) {
        QJsonObject data;
        data["target_user_id"] = targetUserId;
        data["message"] = "User unblocked successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to unblock user");
    }
}

QJsonObject ChatProtocolHandler::handleSearchUsers(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    LOG_INFO(QString("Handling search users request from user %1, keyword: '%2'").arg(userId).arg(request["keyword"].toString()));

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"keyword"}, errorMessage)) {
        LOG_ERROR(QString("Invalid search request parameters: %1").arg(errorMessage));
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString keyword = request["keyword"].toString();
    int limit = request["limit"].toInt(20);

    LOG_INFO(QString("Searching users with keyword: '%1', limit: %2").arg(keyword).arg(limit));

    QJsonArray users = _friendService->searchUsers(keyword, userId, limit);

    LOG_INFO(QString("Search completed, found %1 users").arg(users.size()));

    QJsonObject data;
    data["users"] = users;
    data["count"] = users.size();
    data["keyword"] = keyword;

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleUpdateFriendNote(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"friend_id", "note"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 friendId = request["friend_id"].toVariant().toLongLong();
    QString note = request["note"].toString();

    bool success = _friendService->updateFriendNote(userId, friendId, note);

    if (success) {
        QJsonObject data;
        data["friend_id"] = friendId;
        data["note"] = note;
        data["message"] = "Friend note updated successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to update friend note");
    }
}

QJsonObject ChatProtocolHandler::handleStatusResponse(const QJsonObject& request, qint64 userId)
{
    QString action = request["action"].toString();
    QString requestId = request["request_id"].toString();

    if (action == "status_update") {
        return handleUpdateStatus(request, userId);
    } else if (action == "status_get_friends") {
        return handleGetFriendsStatus(request, userId);
    } else if (action == "heartbeat") {
        return handleHeartbeat(request, userId);
    }

    return createErrorResponse(requestId, action, "INVALID_ACTION", "Unknown status action: " + action);
}

QJsonObject ChatProtocolHandler::handleUpdateStatus(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"status"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString statusStr = request["status"].toString();
    QString clientId = request["client_id"].toString();

    OnlineStatusService::OnlineStatus status = OnlineStatusService::stringToStatus(statusStr);

    bool success = _statusService->updateUserStatus(userId, status, clientId);

    if (success) {
        QJsonObject data;
        data["status"] = statusStr;
        data["message"] = "Status updated successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to update status");
    }
}

QJsonObject ChatProtocolHandler::handleGetFriendsStatus(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    QJsonArray friendsStatus = _statusService->getFriendsOnlineStatus(userId);

    QJsonObject data;
    data["friends_status"] = friendsStatus;
    data["count"] = friendsStatus.size();

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleHeartbeat(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();
    QString clientId = request["client_id"].toString();

    bool success = _statusService->updateHeartbeat(userId, clientId);

    if (success) {
        QJsonObject data;
        data["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        data["message"] = "Heartbeat received";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to update heartbeat");
    }
}

QJsonObject ChatProtocolHandler::handleMessageResponse(const QJsonObject& request, qint64 userId)
{
    QString action = request["action"].toString();
    QString requestId = request["request_id"].toString();

    if (action == "send_message") {
        return handleSendMessage(request, userId);
    } else if (action == "get_chat_history") {
        return handleGetChatHistory(request, userId);
    } else if (action == "get_chat_sessions") {
        return handleGetChatSessions(request, userId);
    } else if (action == "message_mark_read") {
        return handleMarkMessageRead(request, userId);
    } else if (action == "message_unread_count") {
        return handleGetUnreadCount(request, userId);
    } else if (action == "message_offline") {
        return handleGetOfflineMessages(request, userId);
    } else if (action == "message_delete") {
        return handleDeleteMessage(request, userId);
    } else if (action == "message_recall") {
        return handleRecallMessage(request, userId);
    } else if (action == "message_search") {
        return handleSearchMessages(request, userId);
    }

    return createErrorResponse(requestId, action, "INVALID_ACTION", "Unknown message action: " + action);
}

QJsonObject ChatProtocolHandler::handleSendMessage(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"receiver_id", "content"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 receiverId = request["receiver_id"].toVariant().toLongLong();
    QString content = request["content"].toString();
    QString type = request["type"].toString("text");
    QString fileUrl = request["file_url"].toString();
    qint64 fileSize = request["file_size"].toVariant().toLongLong();
    QString fileHash = request["file_hash"].toString();

    MessageService::MessageType messageType = MessageService::stringToMessageType(type);

    QString messageId = _messageService->sendMessage(userId, receiverId, messageType, content, fileUrl, fileSize, fileHash);

    if (!messageId.isEmpty()) {
        QJsonObject data;
        data["message_id"] = messageId;
        data["receiver_id"] = receiverId;
        data["type"] = type;
        data["message"] = "Message sent successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "SEND_FAILED", "Failed to send message");
    }
}

QJsonObject ChatProtocolHandler::handleGetChatHistory(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"chat_user_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 chatUserId = request["chat_user_id"].toVariant().toLongLong();
    int limit = request["limit"].toInt(50);
    int offset = request["offset"].toInt(0);

    QJsonArray messages = _messageService->getChatHistory(userId, chatUserId, limit, offset);

    QJsonObject data;
    data["messages"] = messages;
    data["count"] = messages.size();
    data["chat_user_id"] = chatUserId;
    data["limit"] = limit;
    data["offset"] = offset;

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleGetChatSessions(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    QJsonArray sessions = _messageService->getChatSessions(userId);

    QJsonObject data;
    data["sessions"] = sessions;
    data["count"] = sessions.size();

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleMarkMessageRead(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"message_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString messageId = request["message_id"].toString();

    bool success = _messageService->markMessageAsRead(userId, messageId);

    if (success) {
        QJsonObject data;
        data["message_id"] = messageId;
        data["message"] = "Message marked as read";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to mark message as read");
    }
}

QJsonObject ChatProtocolHandler::handleGetUnreadCount(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    qint64 fromUserId = request["from_user_id"].toVariant().toLongLong();
    if (fromUserId == 0 && !request["from_user_id"].toVariant().isValid()) {
        fromUserId = -1;
    }

    int count = _messageService->getUnreadMessageCount(userId, fromUserId);

    QJsonObject data;
    data["unread_count"] = count;
    if (fromUserId != -1) {
        data["from_user_id"] = fromUserId;
    }

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleGetOfflineMessages(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    QJsonArray messages = _messageService->getOfflineMessages(userId);

    QJsonObject data;
    data["messages"] = messages;
    data["count"] = messages.size();

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleDeleteMessage(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"message_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString messageId = request["message_id"].toString();

    bool success = _messageService->deleteMessage(userId, messageId);

    if (success) {
        QJsonObject data;
        data["message_id"] = messageId;
        data["message"] = "Message deleted successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to delete message");
    }
}

QJsonObject ChatProtocolHandler::handleRecallMessage(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"message_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString messageId = request["message_id"].toString();

    bool success = _messageService->recallMessage(userId, messageId);

    if (success) {
        QJsonObject data;
        data["message_id"] = messageId;
        data["message"] = "Message recalled successfully";
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "OPERATION_FAILED", "Failed to recall message");
    }
}

QJsonObject ChatProtocolHandler::handleSearchMessages(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"keyword"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString keyword = request["keyword"].toString();
    qint64 chatUserId = request["chat_user_id"].toVariant().toLongLong();
    if (chatUserId == 0 && !request["chat_user_id"].toVariant().isValid()) {
        chatUserId = -1;
    }
    int limit = request["limit"].toInt(20);

    QJsonArray messages = _messageService->searchMessages(userId, keyword, chatUserId, limit);

    QJsonObject data;
    data["messages"] = messages;
    data["count"] = messages.size();
    data["keyword"] = keyword;
    if (chatUserId != -1) {
        data["chat_user_id"] = chatUserId;
    }

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::createSuccessResponse(const QString& requestId, const QString& action, const QJsonObject& data)
{
    QJsonObject response;
    response["request_id"] = requestId;
    response["action"] = action;
    response["success"] = true;
    response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (!data.isEmpty()) {
        response["data"] = data;
    }

    return response;
}

QJsonObject ChatProtocolHandler::createErrorResponse(const QString& requestId, const QString& action, const QString& errorCode, const QString& errorMessage)
{
    QJsonObject response;
    response["request_id"] = requestId;
    response["action"] = action;
    response["success"] = false;
    response["error_code"] = errorCode;
    response["error_message"] = errorMessage;
    response["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    return response;
}

bool ChatProtocolHandler::validateRequest(const QJsonObject& request, const QStringList& requiredFields, QString& errorMessage)
{
    for (const QString& field : requiredFields) {
        if (!request.contains(field) || request[field].isNull()) {
            errorMessage = QString("Missing required field: %1").arg(field);
            return false;
        }

        // 检查字符串字段是否为空
        if (request[field].isString() && request[field].toString().isEmpty()) {
            errorMessage = QString("Empty required field: %1").arg(field);
            return false;
        }
    }

    return true;
}

void ChatProtocolHandler::logRequest(const QString& action, const QString& requestId, qint64 userId, const QString& clientIP)
{
    LOG_INFO(QString("Chat request: action=%1, request_id=%2, user_id=%3, client_ip=%4")
             .arg(action).arg(requestId).arg(userId).arg(clientIP));
}

// 好友分组相关方法实现
QJsonObject ChatProtocolHandler::handleGetFriendGroups(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    QJsonArray groups = _friendService->getFriendGroups(userId);

    QJsonObject data;
    data["groups"] = groups;
    data["count"] = groups.size();

    return createSuccessResponse(requestId, action, data);
}

QJsonObject ChatProtocolHandler::handleCreateFriendGroup(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"group_name"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    QString groupName = request["group_name"].toString();

    bool success = _friendService->createFriendGroup(userId, groupName);

    if (success) {
        QJsonObject data;
        data["group_name"] = groupName;
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "CREATE_FAILED", "Failed to create friend group");
    }
}

QJsonObject ChatProtocolHandler::handleDeleteFriendGroup(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"group_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 groupId = request["group_id"].toVariant().toLongLong();

    bool success = _friendService->deleteFriendGroup(userId, groupId);

    if (success) {
        QJsonObject data;
        data["group_id"] = groupId;
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "DELETE_FAILED", "Failed to delete friend group");
    }
}

QJsonObject ChatProtocolHandler::handleRenameFriendGroup(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"group_id", "new_name"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 groupId = request["group_id"].toVariant().toLongLong();
    QString newName = request["new_name"].toString();

    bool success = _friendService->renameFriendGroup(userId, groupId, newName);

    if (success) {
        QJsonObject data;
        data["group_id"] = groupId;
        data["new_name"] = newName;
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "RENAME_FAILED", "Failed to rename friend group");
    }
}

QJsonObject ChatProtocolHandler::handleMoveFriendToGroup(const QJsonObject& request, qint64 userId)
{
    QString requestId = request["request_id"].toString();
    QString action = request["action"].toString();

    // 验证必需参数
    QString errorMessage;
    if (!validateRequest(request, {"friend_id", "group_id"}, errorMessage)) {
        return createErrorResponse(requestId, action, "INVALID_PARAMS", errorMessage);
    }

    qint64 friendId = request["friend_id"].toVariant().toLongLong();
    qint64 groupId = request["group_id"].toVariant().toLongLong();

    bool success = _friendService->moveFriendToGroup(userId, friendId, groupId);

    if (success) {
        QJsonObject data;
        data["friend_id"] = friendId;
        data["group_id"] = groupId;
        return createSuccessResponse(requestId, action, data);
    } else {
        return createErrorResponse(requestId, action, "MOVE_FAILED", "Failed to move friend to group");
    }
}
