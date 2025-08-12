#include "MessageService.h"
#include "FriendService.h"
#include "OnlineStatusService.h"
#include "../database/DatabaseManager.h"
#include "../network/ThreadPoolServer.h"
#include <QSqlRecord>
#include <QVariant>
#include <QUuid>

// 静态成员初始化
MessageService* MessageService::s_instance = nullptr;
QMutex MessageService::s_instanceMutex;

MessageService::MessageService(QObject *parent)
    : QObject(parent)
    , _initialized(false)
{
}

MessageService::~MessageService()
{
}

MessageService* MessageService::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new MessageService();
        }
    }
    return s_instance;
}

bool MessageService::initialize()
{
    QMutexLocker locker(&_mutex);
    
    if (_initialized) {
        return true;
    }
    
    // 测试数据库连接
    QSqlDatabase db = getDatabase();
    if (!db.isValid() || !db.isOpen()) {
        LOG_ERROR("Failed to initialize MessageService: database not available");
        return false;
    }
    
    _initialized = true;
    LOG_INFO("MessageService initialized successfully");
    return true;
}

QString MessageService::sendMessage(qint64 senderId, qint64 receiverId, MessageType type, const QString& content,
                                   const QString& fileUrl, qint64 fileSize, const QString& fileHash)
{
    QMutexLocker locker(&_mutex);
    
    // 检查用户是否为好友
    if (!areUsersFriends(senderId, receiverId)) {
        LOG_WARNING(QString("Cannot send message: users %1 and %2 are not friends").arg(senderId).arg(receiverId));
        return QString();
    }
    
    // 生成消息ID
    QString messageId = generateMessageId();
    
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    
    // 开始事务
    if (!db.transaction()) {
        LOG_ERROR("Failed to start transaction for sending message");
        return QString();
    }
    
    try {
        // 插入消息记录
        query.prepare("INSERT INTO messages (message_id, sender_id, receiver_id, message_type, content, "
                     "file_url, file_size, file_hash, delivery_status, created_at) "
                     "VALUES (:message_id, :sender_id, :receiver_id, :type, :content, "
                     ":file_url, :file_size, :file_hash, 'sent', NOW())");
        
        query.bindValue(":message_id", messageId);
        query.bindValue(":sender_id", senderId);
        query.bindValue(":receiver_id", receiverId);
        query.bindValue(":type", messageTypeToString(type));
        query.bindValue(":content", content);
        query.bindValue(":file_url", fileUrl.isEmpty() ? QVariant() : fileUrl);
        query.bindValue(":file_size", fileSize > 0 ? fileSize : QVariant());
        query.bindValue(":file_hash", fileHash.isEmpty() ? QVariant() : fileHash);
        
        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }
        
        qint64 dbMessageId = query.lastInsertId().toLongLong();
        
        // 提交事务
        if (!db.commit()) {
            throw std::runtime_error("Failed to commit send message transaction");
        }
        
        LOG_INFO(QString("Message sent: %1 from user %2 to user %3").arg(messageId).arg(senderId).arg(receiverId));
        
        // 构建消息JSON对象
        MessageInfo messageInfo;
        messageInfo.id = dbMessageId;
        messageInfo.messageId = messageId;
        messageInfo.senderId = senderId;
        messageInfo.receiverId = receiverId;
        messageInfo.type = type;
        messageInfo.content = content;
        messageInfo.fileUrl = fileUrl;
        messageInfo.fileSize = fileSize;
        messageInfo.fileHash = fileHash;
        messageInfo.status = Sent;
        messageInfo.createdAt = QDateTime::currentDateTime();
        
        QJsonObject messageJson = buildMessageJson(messageInfo);
        
        // 尝试实时推送给接收者
        bool pushed = pushMessageToUser(receiverId, messageJson);
        
        if (pushed) {
            // 更新消息状态为已投递
            updateMessageStatus(messageId, Delivered);
        } else {
            // 添加到离线消息队列
            addToOfflineQueue(receiverId, dbMessageId);
        }
        
        // 发送信号
        emit newMessage(senderId, receiverId, messageId, messageJson);
        
        return messageId;
        
    } catch (const std::exception& e) {
        db.rollback();
        LOG_ERROR(QString("Failed to send message: %1").arg(e.what()));
        return QString();
    }
}

QJsonArray MessageService::getChatHistory(qint64 userId1, qint64 userId2, int limit, int offset)
{
    QMutexLocker locker(&_mutex);
    
    QJsonArray messages;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    
    // 查询聊天历史
    query.prepare("SELECT m.*, "
                 "s.username as sender_username, s.display_name as sender_name, s.avatar_url as sender_avatar, "
                 "r.username as receiver_username, r.display_name as receiver_name, r.avatar_url as receiver_avatar "
                 "FROM messages m "
                 "JOIN users s ON m.sender_id = s.id "
                 "JOIN users r ON m.receiver_id = r.id "
                 "WHERE ((m.sender_id = :user1 AND m.receiver_id = :user2) OR "
                 "       (m.sender_id = :user2 AND m.receiver_id = :user1)) "
                 "ORDER BY m.created_at DESC "
                 "LIMIT :limit OFFSET :offset");
    
    query.bindValue(":user1", userId1);
    query.bindValue(":user2", userId2);
    query.bindValue(":limit", limit);
    query.bindValue(":offset", offset);
    
    if (!query.exec()) {
        LOG_ERROR(QString("Failed to get chat history: %1").arg(query.lastError().text()));
        return messages;
    }
    
    while (query.next()) {
        QJsonObject message;
        message["id"] = query.value("id").toLongLong();
        message["message_id"] = query.value("message_id").toString();
        message["sender_id"] = query.value("sender_id").toLongLong();
        message["receiver_id"] = query.value("receiver_id").toLongLong();
        message["type"] = query.value("message_type").toString();
        message["content"] = query.value("content").toString();
        message["file_url"] = query.value("file_url").toString();
        message["file_size"] = query.value("file_size").toLongLong();
        message["file_hash"] = query.value("file_hash").toString();
        message["status"] = query.value("delivery_status").toString();
        message["created_at"] = query.value("created_at").toDateTime().toString(Qt::ISODate);
        message["updated_at"] = query.value("updated_at").toDateTime().toString(Qt::ISODate);
        
        // 发送者信息
        message["sender_username"] = query.value("sender_username").toString();
        message["sender_name"] = query.value("sender_name").toString();
        message["sender_avatar"] = query.value("sender_avatar").toString();
        
        // 接收者信息
        message["receiver_username"] = query.value("receiver_username").toString();
        message["receiver_name"] = query.value("receiver_name").toString();
        message["receiver_avatar"] = query.value("receiver_avatar").toString();
        
        // 标记是否为当前用户发送的消息
        message["is_own"] = query.value("sender_id").toLongLong() == userId1;
        
        messages.append(message);
    }
    
    LOG_INFO(QString("Retrieved %1 messages for chat between users %2 and %3").arg(messages.size()).arg(userId1).arg(userId2));
    return messages;
}

QJsonArray MessageService::getChatSessions(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    QJsonArray sessions;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    
    // 查询用户的聊天会话
    query.prepare("SELECT DISTINCT "
                 "CASE WHEN m.sender_id = :user_id THEN m.receiver_id ELSE m.sender_id END as chat_user_id, "
                 "CASE WHEN m.sender_id = :user_id THEN r.username ELSE s.username END as chat_username, "
                 "CASE WHEN m.sender_id = :user_id THEN r.display_name ELSE s.display_name END as chat_display_name, "
                 "CASE WHEN m.sender_id = :user_id THEN r.avatar_url ELSE s.avatar_url END as chat_avatar_url, "
                 "MAX(m.created_at) as last_message_time, "
                 "COUNT(CASE WHEN m.receiver_id = :user_id AND mrs.read_at IS NULL THEN 1 END) as unread_count "
                 "FROM messages m "
                 "JOIN users s ON m.sender_id = s.id "
                 "JOIN users r ON m.receiver_id = r.id "
                 "LEFT JOIN message_read_status mrs ON m.id = mrs.message_id AND mrs.user_id = :user_id "
                 "WHERE m.sender_id = :user_id OR m.receiver_id = :user_id "
                 "GROUP BY chat_user_id, chat_username, chat_display_name, chat_avatar_url "
                 "ORDER BY last_message_time DESC");
    
    query.bindValue(":user_id", userId);
    
    if (!query.exec()) {
        LOG_ERROR(QString("Failed to get chat sessions: %1").arg(query.lastError().text()));
        return sessions;
    }
    
    while (query.next()) {
        QJsonObject session;
        session["chat_user_id"] = query.value("chat_user_id").toLongLong();
        session["chat_username"] = query.value("chat_username").toString();
        session["chat_display_name"] = query.value("chat_display_name").toString();
        session["chat_avatar_url"] = query.value("chat_avatar_url").toString();
        session["last_message_time"] = query.value("last_message_time").toDateTime().toString(Qt::ISODate);
        session["unread_count"] = query.value("unread_count").toInt();
        
        sessions.append(session);
    }
    
    LOG_INFO(QString("Retrieved %1 chat sessions for user %2").arg(sessions.size()).arg(userId));
    return sessions;
}

bool MessageService::markMessageAsRead(qint64 userId, const QString& messageId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 获取消息信息
    query.prepare("SELECT id, sender_id, receiver_id FROM messages WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (!query.exec() || !query.next()) {
        LOG_WARNING(QString("Message not found for read marking: %1").arg(messageId));
        return false;
    }

    qint64 dbMessageId = query.value("id").toLongLong();
    qint64 senderId = query.value("sender_id").toLongLong();
    qint64 receiverId = query.value("receiver_id").toLongLong();

    // 只有接收者可以标记消息为已读
    if (userId != receiverId) {
        LOG_WARNING(QString("User %1 cannot mark message %2 as read (not receiver)").arg(userId).arg(messageId));
        return false;
    }

    // 开始事务
    if (!db.transaction()) {
        LOG_ERROR("Failed to start transaction for marking message as read");
        return false;
    }

    try {
        // 插入或更新已读状态
        query.prepare("INSERT INTO message_read_status (message_id, user_id, read_at) "
                     "VALUES (:message_id, :user_id, NOW()) "
                     "ON DUPLICATE KEY UPDATE read_at = NOW()");
        query.bindValue(":message_id", dbMessageId);
        query.bindValue(":user_id", userId);

        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }

        // 更新消息投递状态为已读
        query.prepare("UPDATE messages SET delivery_status = 'read', updated_at = NOW() WHERE message_id = :message_id");
        query.bindValue(":message_id", messageId);

        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }

        // 提交事务
        if (!db.commit()) {
            throw std::runtime_error("Failed to commit mark message as read transaction");
        }

        LOG_INFO(QString("Message %1 marked as read by user %2").arg(messageId).arg(userId));

        // 发送信号
        emit messageRead(userId, messageId);
        emit messageStatusUpdated(messageId, Delivered, Read);

        // 通知发送者消息已读
        QJsonObject readNotification;
        readNotification["action"] = "message_read";
        readNotification["message_id"] = messageId;
        readNotification["reader_id"] = userId;
        readNotification["read_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        pushMessageToUser(senderId, readNotification);

        return true;

    } catch (const std::exception& e) {
        db.rollback();
        LOG_ERROR(QString("Failed to mark message as read: %1").arg(e.what()));
        return false;
    }
}

int MessageService::markMessagesAsRead(qint64 userId, const QStringList& messageIds)
{
    int successCount = 0;

    for (const QString& messageId : messageIds) {
        if (markMessageAsRead(userId, messageId)) {
            successCount++;
        }
    }

    LOG_INFO(QString("Marked %1 out of %2 messages as read for user %3").arg(successCount).arg(messageIds.size()).arg(userId));
    return successCount;
}

int MessageService::getUnreadMessageCount(qint64 userId, qint64 fromUserId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    QString sql = "SELECT COUNT(*) FROM messages m "
                  "LEFT JOIN message_read_status mrs ON m.id = mrs.message_id AND mrs.user_id = :user_id "
                  "WHERE m.receiver_id = :user_id AND mrs.read_at IS NULL";

    if (fromUserId != -1) {
        sql += " AND m.sender_id = :from_user_id";
    }

    query.prepare(sql);
    query.bindValue(":user_id", userId);
    if (fromUserId != -1) {
        query.bindValue(":from_user_id", fromUserId);
    }

    if (!query.exec() || !query.next()) {
        LOG_ERROR(QString("Failed to get unread message count: %1").arg(query.lastError().text()));
        return 0;
    }

    int count = query.value(0).toInt();
    LOG_INFO(QString("User %1 has %2 unread messages").arg(userId).arg(count));
    return count;
}

QJsonArray MessageService::getOfflineMessages(qint64 userId)
{
    QMutexLocker locker(&_mutex);

    QJsonArray messages;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 查询离线消息队列
    query.prepare("SELECT m.*, omq.priority, omq.created_at as queued_at, "
                 "s.username as sender_username, s.display_name as sender_name, s.avatar_url as sender_avatar "
                 "FROM offline_message_queue omq "
                 "JOIN messages m ON omq.message_id = m.id "
                 "JOIN users s ON m.sender_id = s.id "
                 "WHERE omq.user_id = :user_id AND omq.delivered_at IS NULL "
                 "ORDER BY omq.priority DESC, omq.created_at ASC");

    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to get offline messages: %1").arg(query.lastError().text()));
        return messages;
    }

    QList<qint64> processedMessageIds;

    while (query.next()) {
        QJsonObject message;
        qint64 messageId = query.value("id").toLongLong();

        message["id"] = messageId;
        message["message_id"] = query.value("message_id").toString();
        message["sender_id"] = query.value("sender_id").toLongLong();
        message["receiver_id"] = query.value("receiver_id").toLongLong();
        message["type"] = query.value("message_type").toString();
        message["content"] = query.value("content").toString();
        message["file_url"] = query.value("file_url").toString();
        message["file_size"] = query.value("file_size").toLongLong();
        message["file_hash"] = query.value("file_hash").toString();
        message["status"] = query.value("delivery_status").toString();
        message["created_at"] = query.value("created_at").toDateTime().toString(Qt::ISODate);
        message["priority"] = query.value("priority").toInt();
        message["queued_at"] = query.value("queued_at").toDateTime().toString(Qt::ISODate);

        // 发送者信息
        message["sender_username"] = query.value("sender_username").toString();
        message["sender_name"] = query.value("sender_name").toString();
        message["sender_avatar"] = query.value("sender_avatar").toString();

        message["is_own"] = false; // 离线消息都是接收的消息

        messages.append(message);
        processedMessageIds.append(messageId);
    }

    // 标记离线消息为已投递
    if (!processedMessageIds.isEmpty()) {
        QSqlQuery updateQuery(db);
        QString placeholders = QString("?,").repeated(processedMessageIds.size());
        placeholders.chop(1); // 移除最后一个逗号

        updateQuery.prepare(QString("UPDATE offline_message_queue SET delivered_at = NOW() "
                                   "WHERE user_id = :user_id AND message_id IN (%1)").arg(placeholders));
        updateQuery.bindValue(":user_id", userId);

        for (int i = 0; i < processedMessageIds.size(); ++i) {
            updateQuery.bindValue(i + 1, processedMessageIds[i]);
        }

        if (!updateQuery.exec()) {
            LOG_ERROR(QString("Failed to mark offline messages as delivered: %1").arg(updateQuery.lastError().text()));
        }
    }

    LOG_INFO(QString("Retrieved %1 offline messages for user %2").arg(messages.size()).arg(userId));
    return messages;
}

bool MessageService::deleteMessage(qint64 userId, const QString& messageId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 检查消息是否存在且属于该用户
    query.prepare("SELECT sender_id FROM messages WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (!query.exec() || !query.next()) {
        LOG_WARNING(QString("Message not found for deletion: %1").arg(messageId));
        return false;
    }

    qint64 senderId = query.value("sender_id").toLongLong();

    // 只有发送者可以删除消息
    if (userId != senderId) {
        LOG_WARNING(QString("User %1 cannot delete message %2 (not sender)").arg(userId).arg(messageId));
        return false;
    }

    // 删除消息（软删除，更新状态）
    query.prepare("UPDATE messages SET delivery_status = 'failed', updated_at = NOW() WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to delete message %1: %2").arg(messageId).arg(query.lastError().text()));
        return false;
    }

    LOG_INFO(QString("Message %1 deleted by user %2").arg(messageId).arg(userId));
    return true;
}

bool MessageService::recallMessage(qint64 userId, const QString& messageId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 检查消息是否存在且属于该用户
    query.prepare("SELECT sender_id, receiver_id, created_at FROM messages WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (!query.exec() || !query.next()) {
        LOG_WARNING(QString("Message not found for recall: %1").arg(messageId));
        return false;
    }

    qint64 senderId = query.value("sender_id").toLongLong();
    qint64 receiverId = query.value("receiver_id").toLongLong();
    QDateTime createdAt = query.value("created_at").toDateTime();

    // 只有发送者可以撤回消息
    if (userId != senderId) {
        LOG_WARNING(QString("User %1 cannot recall message %2 (not sender)").arg(userId).arg(messageId));
        return false;
    }

    // 检查撤回时间限制（2分钟内）
    if (createdAt.secsTo(QDateTime::currentDateTime()) > 120) {
        LOG_WARNING(QString("Message %1 cannot be recalled (time limit exceeded)").arg(messageId));
        return false;
    }

    // 更新消息内容为撤回提示
    query.prepare("UPDATE messages SET content = '[消息已撤回]', message_type = 'system', updated_at = NOW() WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to recall message %1: %2").arg(messageId).arg(query.lastError().text()));
        return false;
    }

    // 通知接收者消息已撤回
    QJsonObject recallNotification;
    recallNotification["action"] = "message_recalled";
    recallNotification["message_id"] = messageId;
    recallNotification["sender_id"] = senderId;
    recallNotification["recalled_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    pushMessageToUser(receiverId, recallNotification);

    LOG_INFO(QString("Message %1 recalled by user %2").arg(messageId).arg(userId));
    return true;
}

QJsonArray MessageService::searchMessages(qint64 userId, const QString& keyword, qint64 chatUserId, int limit)
{
    QMutexLocker locker(&_mutex);

    QJsonArray messages;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    QString sql = "SELECT m.*, "
                  "s.username as sender_username, s.display_name as sender_name, s.avatar_url as sender_avatar, "
                  "r.username as receiver_username, r.display_name as receiver_name, r.avatar_url as receiver_avatar "
                  "FROM messages m "
                  "JOIN users s ON m.sender_id = s.id "
                  "JOIN users r ON m.receiver_id = r.id "
                  "WHERE (m.sender_id = :user_id OR m.receiver_id = :user_id) "
                  "AND m.content LIKE :keyword "
                  "AND m.message_type = 'text'";

    if (chatUserId != -1) {
        sql += " AND ((m.sender_id = :chat_user_id AND m.receiver_id = :user_id) OR "
               "(m.sender_id = :user_id AND m.receiver_id = :chat_user_id))";
    }

    sql += " ORDER BY m.created_at DESC LIMIT :limit";

    query.prepare(sql);
    query.bindValue(":user_id", userId);
    query.bindValue(":keyword", QString("%%1%").arg(keyword));
    query.bindValue(":limit", limit);

    if (chatUserId != -1) {
        query.bindValue(":chat_user_id", chatUserId);
    }

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to search messages: %1").arg(query.lastError().text()));
        return messages;
    }

    while (query.next()) {
        QJsonObject message;
        message["id"] = query.value("id").toLongLong();
        message["message_id"] = query.value("message_id").toString();
        message["sender_id"] = query.value("sender_id").toLongLong();
        message["receiver_id"] = query.value("receiver_id").toLongLong();
        message["type"] = query.value("message_type").toString();
        message["content"] = query.value("content").toString();
        message["status"] = query.value("delivery_status").toString();
        message["created_at"] = query.value("created_at").toDateTime().toString(Qt::ISODate);

        // 发送者信息
        message["sender_username"] = query.value("sender_username").toString();
        message["sender_name"] = query.value("sender_name").toString();
        message["sender_avatar"] = query.value("sender_avatar").toString();

        // 接收者信息
        message["receiver_username"] = query.value("receiver_username").toString();
        message["receiver_name"] = query.value("receiver_name").toString();
        message["receiver_avatar"] = query.value("receiver_avatar").toString();

        message["is_own"] = query.value("sender_id").toLongLong() == userId;

        messages.append(message);
    }

    LOG_INFO(QString("Found %1 messages for keyword '%2' for user %3").arg(messages.size()).arg(keyword).arg(userId));
    return messages;
}

bool MessageService::updateMessageStatus(const QString& messageId, DeliveryStatus status)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("UPDATE messages SET delivery_status = :status, updated_at = NOW() WHERE message_id = :message_id");
    query.bindValue(":status", deliveryStatusToString(status));
    query.bindValue(":message_id", messageId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to update message status: %1").arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() > 0) {
        LOG_INFO(QString("Message %1 status updated to %2").arg(messageId).arg(deliveryStatusToString(status)));
        return true;
    }

    return false;
}

bool MessageService::pushMessageToUser(qint64 userId, const QJsonObject& message)
{
    // 检查用户是否在线
    OnlineStatusService* statusService = OnlineStatusService::instance();
    if (!statusService || !statusService->isUserOnline(userId)) {
        return false;
    }

    // 通过ThreadPoolServer发送消息
    ThreadPoolServer* server = ThreadPoolServer::instance();
    if (!server) {
        LOG_ERROR("ThreadPoolServer instance not available for message push");
        return false;
    }

    return server->sendMessageToUser(userId, message);
}

bool MessageService::addToOfflineQueue(qint64 userId, qint64 messageId, int priority)
{
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("INSERT INTO offline_message_queue (user_id, message_id, message_type, priority) "
                 "VALUES (:user_id, :message_id, 'private', :priority)");
    query.bindValue(":user_id", userId);
    query.bindValue(":message_id", messageId);
    query.bindValue(":priority", priority);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to add message to offline queue: %1").arg(query.lastError().text()));
        return false;
    }

    LOG_INFO(QString("Message %1 added to offline queue for user %2").arg(messageId).arg(userId));
    return true;
}

int MessageService::processOfflineQueue(qint64 userId)
{
    QMutexLocker locker(&_mutex);

    // 获取离线消息并推送
    QJsonArray offlineMessages = getOfflineMessages(userId);

    int processedCount = 0;
    for (const QJsonValue& value : offlineMessages) {
        QJsonObject message = value.toObject();
        if (pushMessageToUser(userId, message)) {
            processedCount++;
        }
    }

    LOG_INFO(QString("Processed %1 offline messages for user %2").arg(processedCount).arg(userId));
    return processedCount;
}

QString MessageService::messageTypeToString(MessageType type)
{
    switch (type) {
        case Text: return "text";
        case Image: return "image";
        case File: return "file";
        case Audio: return "audio";
        case Video: return "video";
        case System: return "system";
        case Location: return "location";
        case Contact: return "contact";
        default: return "text";
    }
}

MessageService::MessageType MessageService::stringToMessageType(const QString& typeStr)
{
    if (typeStr == "image") return Image;
    if (typeStr == "file") return File;
    if (typeStr == "audio") return Audio;
    if (typeStr == "video") return Video;
    if (typeStr == "system") return System;
    if (typeStr == "location") return Location;
    if (typeStr == "contact") return Contact;
    return Text;
}

QString MessageService::deliveryStatusToString(DeliveryStatus status)
{
    switch (status) {
        case Sent: return "sent";
        case Delivered: return "delivered";
        case Read: return "read";
        case Failed: return "failed";
        default: return "sent";
    }
}

MessageService::DeliveryStatus MessageService::stringToDeliveryStatus(const QString& statusStr)
{
    if (statusStr == "delivered") return Delivered;
    if (statusStr == "read") return Read;
    if (statusStr == "failed") return Failed;
    return Sent;
}

QSqlDatabase MessageService::getDatabase()
{
    return DatabaseManager::instance()->getConnection();
}

QString MessageService::generateMessageId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

MessageService::MessageInfo MessageService::getMessageInfo(const QString& messageId)
{
    MessageInfo info;

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT * FROM messages WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (query.exec() && query.next()) {
        info.id = query.value("id").toLongLong();
        info.messageId = query.value("message_id").toString();
        info.senderId = query.value("sender_id").toLongLong();
        info.receiverId = query.value("receiver_id").toLongLong();
        info.type = stringToMessageType(query.value("message_type").toString());
        info.content = query.value("content").toString();
        info.fileUrl = query.value("file_url").toString();
        info.fileSize = query.value("file_size").toLongLong();
        info.fileHash = query.value("file_hash").toString();
        info.status = stringToDeliveryStatus(query.value("delivery_status").toString());
        info.createdAt = query.value("created_at").toDateTime();
        info.updatedAt = query.value("updated_at").toDateTime();
    }

    return info;
}

QJsonObject MessageService::buildMessageJson(const MessageInfo& messageInfo)
{
    QJsonObject message;
    message["id"] = messageInfo.id;
    message["message_id"] = messageInfo.messageId;
    message["sender_id"] = messageInfo.senderId;
    message["receiver_id"] = messageInfo.receiverId;
    message["type"] = messageTypeToString(messageInfo.type);
    message["content"] = messageInfo.content;
    message["file_url"] = messageInfo.fileUrl;
    message["file_size"] = messageInfo.fileSize;
    message["file_hash"] = messageInfo.fileHash;
    message["status"] = deliveryStatusToString(messageInfo.status);
    message["created_at"] = messageInfo.createdAt.toString(Qt::ISODate);
    message["updated_at"] = messageInfo.updatedAt.toString(Qt::ISODate);

    return message;
}

bool MessageService::areUsersFriends(qint64 userId1, qint64 userId2)
{
    FriendService* friendService = FriendService::instance();
    if (!friendService) {
        return false;
    }

    return friendService->areFriends(userId1, userId2);
}
