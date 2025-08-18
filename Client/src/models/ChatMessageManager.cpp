#include "ChatMessageManager.h"
#include "RecentContactsManager.h"
#include "../chat/ChatNetworkClient.h"
#include "../auth/NetworkClient.h"
#include "../auth/SessionManager.h"
#include "../utils/Logger.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QTimer>
#include <QUuid>
#include <algorithm>

// 静态成员初始化
ChatMessageManager* ChatMessageManager::s_instance = nullptr;
QMutex ChatMessageManager::s_instanceMutex;

ChatMessageManager::ChatMessageManager(QObject *parent)
    : QObject(parent)
    , _isLoading(false)
    , _hasMoreHistory(true)
    , _unreadCount(0)
    , _currentOffset(0)
{
    // 初始化自动刷新定时器
    _autoRefreshTimer = new QTimer(this);
    _autoRefreshTimer->setInterval(5000); // 5秒刷新一次
    _autoRefreshTimer->setSingleShot(false);
    connect(_autoRefreshTimer, &QTimer::timeout, this, &ChatMessageManager::onAutoRefreshTimer);
    
    // 连接ChatNetworkClient信号
    auto chatClient = ChatNetworkClient::instance();
    if (chatClient) {
        connect(chatClient, &ChatNetworkClient::messageSent, this, &ChatMessageManager::handleMessageSent);
        connect(chatClient, &ChatNetworkClient::chatHistoryReceived, this, &ChatMessageManager::handleChatHistoryReceived);
        connect(chatClient, &ChatNetworkClient::messageReceived, this, &ChatMessageManager::handleMessageReceived);
        connect(chatClient, &ChatNetworkClient::messageStatusUpdated, this, &ChatMessageManager::handleMessageStatusUpdated);
        connect(chatClient, &ChatNetworkClient::offlineMessagesReceived, this, &ChatMessageManager::handleOfflineMessagesReceived);
    }
    

}

ChatMessageManager* ChatMessageManager::instance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new ChatMessageManager();
    }
    return s_instance;
}

qint64 ChatMessageManager::getCurrentUserId() const
{
    // 从NetworkClient获取当前用户ID
    auto networkClient = NetworkClient::instance();
    if (networkClient) {
        return networkClient->userId();
    }
    return 0;
}

void ChatMessageManager::setCurrentChatUser(const QVariantMap& user)
{
    QMutexLocker locker(&_mutex);
    
    if (_currentChatUser == user) {
        return;
    }
    
    // 停止之前的自动刷新
    _autoRefreshTimer->stop();
    
    // 清空当前消息和状态
    _messages.clear();
    _currentOffset = 0;
    _hasMoreHistory = true;
    _unreadCount = 0;
    

    
    // 如果用户为空，说明是用户切换，只清理聊天消息，保留最近联系人数据
    // 最近联系人数据将在好友列表更新时通过filterByFriendList进行智能过滤
    if (user.isEmpty()) {
        // 不再清理最近联系人数据，避免登录时数据丢失
        // 无效联系人将在获取好友列表后自动过滤
    }
    
    // 如果设置了新用户，检查是否与当前用户是好友关系
    if (!user.isEmpty()) {
        qint64 userId = user.value("user_id").toLongLong();
        if (userId <= 0) {
            userId = user.value("id").toLongLong();
        }
        if (userId <= 0) {
            userId = user.value("friend_id").toLongLong();
        }
        
        if (userId > 0) {
            // 这里可以添加好友关系检查逻辑
            // 如果不是好友，可以清空聊天记录或显示提示
        }
    }
    
    _currentChatUser = user;
    
    // 获取用户ID（支持多种字段名）
    qint64 userId = user.value("user_id").toLongLong();
    if (userId <= 0) {
        userId = user.value("id").toLongLong();
    }
    if (userId <= 0) {
        userId = user.value("friend_id").toLongLong();
    }
    

    
    emit currentChatUserChanged();
    emit messagesChanged();
    emit hasMoreHistoryChanged();
    emit unreadCountChanged();
    
    // 如果有选中的用户，加载聊天历史
    if (!user.isEmpty()) {
        loadChatHistory();
        // 开始自动刷新
        _autoRefreshTimer->start();
    }
}

void ChatMessageManager::sendMessage(const QString& content, const QString& type)
{
    if (content.trimmed().isEmpty()) {
        LOG_WARNING("Cannot send empty message");
        emit messageSendResult(false, "消息内容不能为空");
        return;
    }
    
    if (_currentChatUser.isEmpty()) {
        LOG_WARNING("No chat user selected");
        emit messageSendResult(false, "请先选择聊天对象");
        return;
    }
    
    // 获取接收者ID（支持多种字段名）
    qint64 receiverId = _currentChatUser.value("user_id").toLongLong();
    if (receiverId <= 0) {
        receiverId = _currentChatUser.value("id").toLongLong();
    }
    if (receiverId <= 0) {
        receiverId = _currentChatUser.value("friend_id").toLongLong();
    }
    
    if (receiverId <= 0) {
        LOG_ERROR("Invalid receiver ID");
        emit messageSendResult(false, "无效的接收者ID");
        return;
    }
    
    // 通过ChatNetworkClient发送消息
    auto chatClient = ChatNetworkClient::instance();
    if (chatClient) {

        
        // 立即在本地添加发送的消息
        QVariantMap messageData;
        messageData["message_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        messageData["sender_id"] = getCurrentUserId();
        messageData["receiver_id"] = receiverId;
        messageData["content"] = content;
        messageData["type"] = type;
        messageData["time"] = QDateTime::currentDateTime().toString("hh:mm");
        messageData["date"] = QDateTime::currentDateTime().toString("yyyy-MM-dd");
        messageData["is_own"] = true;
        messageData["is_read"] = true;
        messageData["delivery_status"] = "sending";
        
        // 设置发送者信息
        messageData["sender_name"] = "我";
        auto sessionManager = SessionManager::instance();
        QString currentUserName;
        if (sessionManager && sessionManager->currentUser()) {
            currentUserName = sessionManager->currentUser()->username();
        }
        messageData["sender_avatar"] = currentUserName.isEmpty() ? QString("我") : QString(currentUserName.at(0).toUpper());
        
        addMessage(messageData, false); // 新消息添加到末尾，在TopToBottom布局中显示在底部
        
        chatClient->sendMessage(receiverId, content, type);
        
        // 更新最近联系人 - 使用接收者ID
        auto recentManager = RecentContactsManager::instance();
        if (recentManager) {
            QString timeStr = QDateTime::currentDateTime().toString("hh:mm");
            
            // 确保接收者被添加到最近联系人列表中，同时设置消息内容
            QVariantMap receiverContact;
            receiverContact["user_id"] = receiverId;
            receiverContact["username"] = _currentChatUser.value("username").toString();
            receiverContact["display_name"] = _currentChatUser.value("display_name", _currentChatUser.value("username")).toString();
            receiverContact["avatar_url"] = _currentChatUser.value("avatar_url").toString();
            receiverContact["last_message"] = content;
            receiverContact["last_message_time"] = timeStr;
            recentManager->addRecentContact(receiverContact);
            
            LOG_INFO(QString("Updated recent contact for sent message to %1: content='%2', time='%3'").arg(receiverId).arg(content).arg(timeStr));
        }
    } else {
        LOG_ERROR("ChatNetworkClient not available");
        emit messageSendResult(false, "网络客户端不可用");
    }
}

void ChatMessageManager::loadChatHistory(int limit, int offset)
{
    if (_currentChatUser.isEmpty()) {
        LOG_WARNING("No chat user selected for loading history");
        return;
    }
    
    // 获取用户ID（支持多种字段名）
    qint64 userId = _currentChatUser.value("user_id").toLongLong();
    if (userId <= 0) {
        userId = _currentChatUser.value("id").toLongLong();
    }
    if (userId <= 0) {
        userId = _currentChatUser.value("friend_id").toLongLong();
    }
    
    if (userId <= 0) {
        LOG_ERROR("Invalid user ID for loading history");
        return;
    }
    
    setIsLoading(true);
    
    auto chatClient = ChatNetworkClient::instance();
    if (chatClient) {

        chatClient->getChatHistory(userId, limit, offset);
    } else {
        LOG_ERROR("ChatNetworkClient not available for loading history");
        setIsLoading(false);
    }
}

void ChatMessageManager::loadMoreHistory()
{
    if (!_hasMoreHistory || _isLoading) {
        return;
    }
    
    _currentOffset += DEFAULT_LIMIT;
    loadChatHistory(DEFAULT_LIMIT, _currentOffset);
}

void ChatMessageManager::markMessagesAsRead()
{
    if (_currentChatUser.isEmpty() || _unreadCount == 0) {
        return;
    }
    
    // 收集未读消息ID
    QStringList messageIds;
    for (const auto& messageVar : _messages) {
        QVariantMap message = messageVar.toMap();
        if (!message.value("is_read", true).toBool()) {
            messageIds.append(message.value("message_id").toString());
        }
    }
    
    if (messageIds.isEmpty()) {
        return;
    }
    
    auto chatClient = ChatNetworkClient::instance();
    if (chatClient) {
    
        chatClient->markMessagesAsRead(messageIds);
        
        // 立即更新本地状态
        for (int i = 0; i < _messages.size(); ++i) {
            QVariantMap message = _messages[i].toMap();
            if (messageIds.contains(message.value("message_id").toString())) {
                message["is_read"] = true;
                _messages[i] = message;
            }
        }
        
        setUnreadCount(0);
        emit messagesChanged();
    }
}

void ChatMessageManager::clearMessages()
{
    QMutexLocker locker(&_mutex);
    _messages.clear();
    _currentOffset = 0;
    _hasMoreHistory = true;
    _unreadCount = 0;
    
    emit messagesChanged();
    emit hasMoreHistoryChanged();
    emit unreadCountChanged();
}

void ChatMessageManager::clearMessagesForUser(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    // 如果当前聊天用户是被删除的用户，清空消息
    qint64 currentUserId = _currentChatUser.value("user_id").toLongLong();
    if (currentUserId <= 0) {
        currentUserId = _currentChatUser.value("id").toLongLong();
    }
    if (currentUserId <= 0) {
        currentUserId = _currentChatUser.value("friend_id").toLongLong();
    }
    
    if (currentUserId == userId) {
        _messages.clear();
        _currentOffset = 0;
        _hasMoreHistory = true;
        _unreadCount = 0;
        
        emit messagesChanged();
        emit hasMoreHistoryChanged();
        emit unreadCountChanged();
        
    
    }
}

void ChatMessageManager::refreshMessages()
{
    if (_currentChatUser.isEmpty()) {
        return;
    }
    
    _currentOffset = 0;
    _hasMoreHistory = true;
    loadChatHistory();
}

void ChatMessageManager::handleMessageReceived(const QJsonObject& message)
{
    QMutexLocker locker(&_mutex);
    
    // 创建消息数据，确保在整个函数范围内可用
    QVariantMap messageData = createMessageData(message);
    
    // 检查消息是否来自当前聊天用户
    qint64 senderId = message.value("sender_id").toVariant().toLongLong();
    qint64 currentUserId = getCurrentUserId();
    qint64 chatUserId = _currentChatUser.value("user_id").toLongLong();
    if (chatUserId <= 0) {
        chatUserId = _currentChatUser.value("id").toLongLong();
    }
    if (chatUserId <= 0) {
        chatUserId = _currentChatUser.value("friend_id").toLongLong();
    }
    
    // 严格检查消息是否与当前用户相关
    qint64 receiverId = message.value("receiver_id").toVariant().toLongLong();
    
    // 消息必须是由当前用户发送或接收的
    if (senderId != currentUserId && receiverId != currentUserId) {
        LOG_WARNING(QString("Received message not related to current user: sender=%1, receiver=%2, current=%3")
                    .arg(senderId).arg(receiverId).arg(currentUserId));
        return;
    }
    
    // 如果当前有选中的聊天用户，消息还必须与聊天用户相关
    if (!_currentChatUser.isEmpty()) {
        if (senderId != chatUserId && receiverId != chatUserId) {
            LOG_WARNING(QString("Received message not related to current chat user: sender=%1, receiver=%2, chatUser=%3")
                        .arg(senderId).arg(receiverId).arg(chatUserId));
            return;
        }
    }
    
    // 更新最近联系人列表 - 只处理接收到的他人消息
    if (senderId != currentUserId) {
        setUnreadCount(_unreadCount + 1);
        
        auto recentManager = RecentContactsManager::instance();
        if (recentManager) {
            QString content = messageData.value("content").toString();
            QString timeStr = messageData.value("time").toString();
            
            // 接收到他人消息时，更新发送者的最近联系人信息
            QVariantMap senderContact;
            senderContact["user_id"] = senderId;
            senderContact["username"] = messageData.value("sender_name").toString();
            senderContact["display_name"] = messageData.value("sender_name").toString();
            senderContact["avatar_url"] = messageData.value("sender_avatar").toString();
            senderContact["last_message"] = content;
            senderContact["last_message_time"] = timeStr;
            recentManager->addRecentContact(senderContact);
            
            // 更新未读计数
            recentManager->updateUnreadCount(senderId, _unreadCount);
            
            LOG_INFO(QString("Updated recent contact for received message from %1: content='%2', time='%3'").arg(senderId).arg(content).arg(timeStr));
        }
    }
    // 注意：自己发送的消息确认不需要更新最近联系人列表，因为发送时已经更新过了
    
    // 只有当当前有选中的聊天用户且消息与聊天用户相关时，才添加到聊天消息列表
    if (!_currentChatUser.isEmpty() && 
        (senderId == chatUserId || receiverId == chatUserId)) {
        addMessage(messageData, false); // 新消息添加到开头，在BottomToTop布局中显示在底部
    }
    

    emit newMessageReceived(messageData);
}

void ChatMessageManager::handleMessageSent(const QString& messageId, bool success)
{
    if (success) {
        // 静默处理发送成功，不显示"已发送"状态
        emit messageSendResult(true, "消息发送成功");
        
        // 更新本地消息状态为已发送，但不显示状态指示器
        for (int i = 0; i < _messages.size(); ++i) {
            QVariantMap message = _messages[i].toMap();
            if (message.value("delivery_status").toString() == "sending") {
                message["delivery_status"] = "sent";
                message["message_id"] = messageId; // 使用服务器返回的消息ID
                _messages[i] = message;
                break;
            }
        }
        emit messagesChanged();
    } else {
        LOG_ERROR(QString("Failed to send message: %1").arg(messageId));
        emit messageSendResult(false, "消息发送失败");
        
        // 更新本地消息状态为发送失败，显示红色感叹号
        for (int i = 0; i < _messages.size(); ++i) {
            QVariantMap message = _messages[i].toMap();
            if (message.value("delivery_status").toString() == "sending") {
                message["delivery_status"] = "failed";
                _messages[i] = message;
                break;
            }
        }
        emit messagesChanged();
    }
}

void ChatMessageManager::handleChatHistoryReceived(qint64 userId, const QJsonArray& messages)
{
    QMutexLocker locker(&_mutex);
    
    // 检查是否是当前聊天用户的历史记录
    qint64 chatUserId = _currentChatUser.value("user_id").toLongLong();
    if (chatUserId <= 0) {
        chatUserId = _currentChatUser.value("id").toLongLong();
    }
    if (chatUserId <= 0) {
        chatUserId = _currentChatUser.value("friend_id").toLongLong();
    }
    if (userId != chatUserId) {
        LOG_WARNING(QString("Received chat history for different user: %1 vs %2")
                    .arg(userId).arg(chatUserId));
        setIsLoading(false);
        return;
    }
    
    if (_currentOffset == 0) {
        // 首次加载，清空现有消息
        _messages.clear();
    }
    
    // 处理接收到的消息
    int unreadCount = 0;
    qint64 currentUserId = getCurrentUserId();
    
    for (const auto& messageValue : messages) {
        QJsonObject messageObj = messageValue.toObject();
        
        // 严格过滤：只处理与当前用户相关的消息
        qint64 senderId = messageObj.value("sender_id").toVariant().toLongLong();
        qint64 receiverId = messageObj.value("receiver_id").toVariant().toLongLong();
        
        // 消息必须是由当前用户发送或接收的
        if (senderId != currentUserId && receiverId != currentUserId) {
            continue; // 跳过不相关的消息
        }
        
        QVariantMap messageData = createMessageData(messageObj);
        
        if (_currentOffset == 0) {
            // 首次加载，使用prepend确保较早的消息在顶部
            addMessage(messageData, true);
        } else {
            // 加载更多历史，添加到开头
            addMessage(messageData, true);
        }
        
        // 统计未读消息
        if (!messageData.value("is_read", true).toBool() && 
            !messageData.value("is_own", false).toBool()) {
            unreadCount++;
        }
    }
    
    // 如果是首次加载，对消息按时间排序（较早的在前面）
    if (_currentOffset == 0 && !_messages.isEmpty()) {
        std::sort(_messages.begin(), _messages.end(), [](const QVariant& a, const QVariant& b) {
            QVariantMap msgA = a.toMap();
            QVariantMap msgB = b.toMap();
            QDateTime timeA = msgA.value("created_at").toDateTime();
            QDateTime timeB = msgB.value("created_at").toDateTime();
            return timeA < timeB; // 较早的时间在前
        });
    }
    
    // 更新状态
    setUnreadCount(unreadCount);
    setHasMoreHistory(messages.size() >= DEFAULT_LIMIT);
    setIsLoading(false);
    

}

void ChatMessageManager::handleMessageStatusUpdated(const QString& messageId, const QString& status)
{
    updateMessageStatus(messageId, status);
    emit messageStatusChanged(messageId, status);
}

void ChatMessageManager::onAutoRefreshTimer()
{
    if (!_currentChatUser.isEmpty() && !_isLoading) {
        // 只刷新最新的消息
        loadChatHistory(10, 0);
    }
}

void ChatMessageManager::setIsLoading(bool loading)
{
    if (_isLoading != loading) {
        _isLoading = loading;
        emit isLoadingChanged();
    }
}

void ChatMessageManager::setHasMoreHistory(bool hasMore)
{
    if (_hasMoreHistory != hasMore) {
        _hasMoreHistory = hasMore;
        emit hasMoreHistoryChanged();
    }
}

void ChatMessageManager::setUnreadCount(int count)
{
    if (_unreadCount != count) {
        _unreadCount = count;
        emit unreadCountChanged();
    }
}

QVariantMap ChatMessageManager::createMessageData(const QJsonObject& message)
{
    QVariantMap data;
    
    data["message_id"] = message.value("message_id").toString();
    data["sender_id"] = message.value("sender_id").toVariant().toLongLong();
    data["receiver_id"] = message.value("receiver_id").toVariant().toLongLong();
    data["content"] = message.value("content").toString();
    data["message_type"] = message.value("message_type").toString("text");
    data["delivery_status"] = message.value("delivery_status").toString("sent");
    
    // 解析时间
    QString createdAtStr = message.value("created_at").toString();
    QDateTime createdAt = QDateTime::fromString(createdAtStr, Qt::ISODate);
    if (!createdAt.isValid()) {
        createdAt = QDateTime::currentDateTime();
    }
    data["created_at"] = createdAt;
    data["time"] = formatMessageTime(createdAt);
    
    // 判断是否是自己发送的消息
    qint64 senderId = message.value("sender_id").toVariant().toLongLong();
    qint64 currentUserId = getCurrentUserId();
    data["is_own"] = (senderId == currentUserId);
    
    // 设置发送者信息
    if (data["is_own"].toBool()) {
        data["sender_name"] = "我";
        // 从SessionManager获取当前用户信息用于头像显示
        auto sessionManager = SessionManager::instance();
        QString currentUserName;
        if (sessionManager && sessionManager->currentUser()) {
            currentUserName = sessionManager->currentUser()->username();
        }
        data["sender_avatar"] = currentUserName.isEmpty() ? QString("我") : QString(currentUserName.at(0).toUpper());
    } else {
        data["sender_name"] = _currentChatUser.value("display_name", _currentChatUser.value("username")).toString();
        QString senderName = data["sender_name"].toString();
        data["sender_avatar"] = senderName.isEmpty() ? QString("?") : QString(senderName.at(0).toUpper());
    }
    
    // 设置已读状态
    data["is_read"] = message.value("is_read").toBool(true);
    return data;
}

void ChatMessageManager::addMessage(const QVariantMap& message, bool prepend)
{
    // 使用TopToBottom布局，确保消息按时间顺序从下往上显示
    // 较早的消息应该在列表开头（显示在顶部），较新的消息应该在列表末尾（显示在底部）
    if (prepend) {
        // 历史消息加载时使用prepend添加到列表开头（较早的消息）
        _messages.prepend(message);
    } else {
        // 新消息使用append添加到列表末尾，在TopToBottom布局中显示在底部（较新的消息）
        _messages.append(message);
    }
    
    emit messagesChanged();
}

void ChatMessageManager::updateMessageStatus(const QString& messageId, const QString& status)
{
    int index = findMessageIndex(messageId);
    if (index >= 0) {
        QVariantMap message = _messages[index].toMap();
        message["delivery_status"] = status;
        _messages[index] = message;
        emit messagesChanged();
    }
}

int ChatMessageManager::findMessageIndex(const QString& messageId)
{
    for (int i = 0; i < _messages.size(); ++i) {
        QVariantMap message = _messages[i].toMap();
        if (message.value("message_id").toString() == messageId) {
            return i;
        }
    }
    return -1;
}

QString ChatMessageManager::formatMessageTime(const QDateTime& dateTime)
{
    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();
    QDate messageDate = dateTime.date();
    
    if (messageDate == today) {
        // 今天的消息只显示时间
        return dateTime.toString("hh:mm");
    } else if (messageDate == today.addDays(-1)) {
        // 昨天的消息
        return QString("昨天 %1").arg(dateTime.toString("hh:mm"));
    } else if (messageDate.year() == today.year()) {
        // 今年的消息
        return dateTime.toString("MM-dd hh:mm");
    } else {
        // 其他年份的消息
        return dateTime.toString("yyyy-MM-dd hh:mm");
    }
}

void ChatMessageManager::handleOfflineMessagesReceived(const QJsonArray& messages)
{
    QMutexLocker locker(&_mutex);
    
    for (const auto& messageValue : messages) {
        QJsonObject messageObj = messageValue.toObject();
        QVariantMap messageData = createMessageData(messageObj);
        
        qint64 senderId = messageObj.value("sender_id").toVariant().toLongLong();
        qint64 receiverId = messageObj.value("receiver_id").toVariant().toLongLong();
        qint64 currentUserId = getCurrentUserId();
        
        if (senderId != currentUserId && receiverId != currentUserId) {
            continue;
        }
        
        if (senderId != currentUserId) {
            setUnreadCount(_unreadCount + 1);
            
            auto recentManager = RecentContactsManager::instance();
            if (recentManager) {
                QString content = messageData.value("content").toString();
                QString timeStr = messageData.value("time").toString();
                
                QVariantMap senderContact;
                senderContact["user_id"] = senderId;
                senderContact["username"] = messageData.value("sender_name").toString();
                senderContact["display_name"] = messageData.value("sender_name").toString();
                senderContact["avatar_url"] = messageData.value("sender_avatar").toString();
                senderContact["last_message"] = content;
                senderContact["last_message_time"] = timeStr;
                
                recentManager->addRecentContact(senderContact);
                recentManager->updateUnreadCount(senderId, _unreadCount);
            }
        }
        
        qint64 chatUserId = _currentChatUser.value("user_id").toLongLong();
        if (chatUserId <= 0) {
            chatUserId = _currentChatUser.value("id").toLongLong();
        }
        if (chatUserId <= 0) {
            chatUserId = _currentChatUser.value("friend_id").toLongLong();
        }
        
        if (!_currentChatUser.isEmpty() && 
            (senderId == chatUserId || receiverId == chatUserId)) {
            addMessage(messageData, false);
        }
        
        emit newMessageReceived(messageData);
    }
}