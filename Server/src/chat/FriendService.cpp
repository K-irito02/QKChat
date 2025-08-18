#include "FriendService.h"
#include "OnlineStatusService.h"
#include "MessageService.h"
#include "../database/DatabaseManager.h"
#include "../database/DatabaseConnectionPool.h"
#include "../cache/CacheManager.h"
#include "../rate_limit/RateLimitManager.h"
#include "../network/ThreadPoolServer.h"
#include <QSqlRecord>
#include <QVariant>
#include <QUuid>
#include <QCryptographicHash>
#include <QThread>

// 静态成员初始化
FriendService* FriendService::s_instance = nullptr;
QMutex FriendService::s_instanceMutex;

FriendService::FriendService(QObject *parent)
    : QObject(parent)
    , _initialized(false)
{
}

FriendService::~FriendService()
{
}

FriendService* FriendService::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new FriendService();
        }
    }
    return s_instance;
}

bool FriendService::initialize()
{
    QMutexLocker locker(&_mutex);
    
    if (_initialized) {
        return true;
    }
    
    // 测试数据库连接 - 使用RAII模式
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to initialize FriendService: database not available");
        return false;
    }
    
    _initialized = true;
    // FriendService初始化成功
    return true;
}

FriendService::FriendRequestResult FriendService::sendFriendRequest(qint64 fromUserId, const QString& toUserIdentifier, const QString& message, const QString& remark, const QString& groupName)
{
    QMutexLocker locker(&_mutex);
    
    // 查找目标用户
    qint64 toUserId = findUserByIdentifier(toUserIdentifier);
    if (toUserId == -1) {
        LOG_WARNING(QString("User not found: %1").arg(toUserIdentifier));
        return UserNotFound;
    }
    
    // 检查是否为自己
    if (fromUserId == toUserId) {
        LOG_WARNING(QString("User %1 tried to send friend request to self").arg(fromUserId));
        return SelfRequest;
    }
    
    // 使用RAII模式的DatabaseConnection确保连接自动释放
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend request");
        return DatabaseError;
    }
    
    // 检查是否已经存在好友请求
    QSqlQuery existingRequestQuery = dbConn.executeQuery(
        "SELECT id, requester_status, target_status FROM friend_requests WHERE "
        "(requester_id = ? AND target_id = ?) OR "
        "(requester_id = ? AND target_id = ?)",
        {fromUserId, toUserId, toUserId, fromUserId}
    );
    
    if (existingRequestQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to check existing friend request: %1").arg(existingRequestQuery.lastError().text()));
        return DatabaseError;
    }
    
    if (existingRequestQuery.next()) {
        QString requesterStatus = existingRequestQuery.value("requester_status").toString();
        QString targetStatus = existingRequestQuery.value("target_status").toString();
        
        // 检查是否有任何一方状态为pending
        if (requesterStatus == "pending" || targetStatus == "pending") {
            return AlreadyRequested;
        } else if (requesterStatus == "accepted" && targetStatus == "accepted") {
            return AlreadyFriends;
        }
        // 如果状态是 'rejected' 或 'ignored'，删除旧记录，允许重新申请
        else if (requesterStatus == "rejected" || targetStatus == "rejected" || 
                 requesterStatus == "ignored" || targetStatus == "ignored") {
        
            qint64 oldRequestId = existingRequestQuery.value("id").toLongLong();
            
            // 删除旧的好友申请记录
            QSqlQuery deleteOldRequestQuery = dbConn.executeQuery(
                "DELETE FROM friend_requests WHERE id = ?",
                {oldRequestId}
            );
            
            if (deleteOldRequestQuery.lastError().isValid()) {
                LOG_WARNING(QString("Failed to delete old rejected friend request: %1").arg(deleteOldRequestQuery.lastError().text()));
            } else {
            
            }
            
            // 删除相关的好友申请通知记录
            QSqlQuery deleteNotificationQuery = dbConn.executeQuery(
                "DELETE FROM friend_request_notifications WHERE request_id = ?",
                {oldRequestId}
            );
            
            if (deleteNotificationQuery.lastError().isValid()) {
                LOG_WARNING(QString("Failed to delete old friend request notification: %1").arg(deleteNotificationQuery.lastError().text()));
            } else {
            
            }
        }
    }
    
    // 检查是否已经是好友
    QSqlQuery friendshipQuery = dbConn.executeQuery(
        "SELECT id, status FROM friendships WHERE "
        "(user_id = ? AND friend_id = ?) OR "
        "(user_id = ? AND friend_id = ?)",
        {fromUserId, toUserId, toUserId, fromUserId}
    );
    
    if (friendshipQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to check existing friendship: %1").arg(friendshipQuery.lastError().text()));
        return DatabaseError;
    }
    
    if (friendshipQuery.next()) {
        QString status = friendshipQuery.value("status").toString();
        qint64 friendshipId = friendshipQuery.value("id").toLongLong();
        
        LOG_INFO(QString("Found existing friendship record ID %1 with status '%2' between users %3 and %4")
                 .arg(friendshipId).arg(status).arg(fromUserId).arg(toUserId));
        
        if (status == "accepted") {
            LOG_WARNING(QString("Users %1 and %2 are already friends").arg(fromUserId).arg(toUserId));
            return AlreadyFriends;
        } else if (status == "blocked") {
            LOG_WARNING(QString("User %1 is blocked by user %2").arg(fromUserId).arg(toUserId));
            return UserBlocked;
        } else if (status == "deleted") {
            LOG_INFO(QString("Found deleted friendship between users %1 and %2, cleaning up and allowing new friend request")
                     .arg(fromUserId).arg(toUserId));
            
            // 删除双向的deleted状态记录，确保数据一致性
            QSqlQuery deleteQuery = dbConn.executeQuery(
                "DELETE FROM friendships WHERE "
                "((user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)) "
                "AND status = 'deleted'",
                {fromUserId, toUserId, toUserId, fromUserId}
            );
            
            if (deleteQuery.lastError().isValid()) {
                LOG_WARNING(QString("Failed to delete deleted friendship records: %1").arg(deleteQuery.lastError().text()));
            } else {
                int deletedCount = deleteQuery.numRowsAffected();
                LOG_INFO(QString("Successfully deleted %1 deleted friendship records between users %2 and %3").arg(deletedCount).arg(fromUserId).arg(toUserId));
            }
            
            // 继续执行后续逻辑
        } else {
            LOG_INFO(QString("Found friendship with status '%1' between users %2 and %3, allowing new friend request")
                     .arg(status).arg(fromUserId).arg(toUserId));
        }
    } else {
        LOG_INFO(QString("No existing friendship found between users %1 and %2").arg(fromUserId).arg(toUserId));
    }
    
    try {
        // 创建好友请求记录
        QSqlQuery insertQuery = dbConn.executeQuery(
            "INSERT INTO friend_requests (requester_id, target_id, message, requester_status, target_status, requested_at) VALUES (?, ?, ?, 'pending', 'pending', NOW())",
            {fromUserId, toUserId, message}
        );
        
        if (insertQuery.lastError().isValid()) {
            LOG_ERROR(QString("Failed to create friend request: %1").arg(insertQuery.lastError().text()));
            return DatabaseError;
        }
        
        // 获取插入的请求ID
        qint64 requestId = insertQuery.lastInsertId().toLongLong();
        
        // 为接收者创建好友请求通知记录
        QSqlQuery notificationQuery = dbConn.executeQuery(
            "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_received', ?)",
            {requestId, toUserId, message}
        );
        
        if (notificationQuery.lastError().isValid()) {
            LOG_WARNING(QString("Failed to create friend request notification for target: %1").arg(notificationQuery.lastError().text()));
        }
        
        // 为发送者创建好友请求通知记录（用于跟踪发送状态）
        QSqlQuery senderNotificationQuery = dbConn.executeQuery(
            "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_sent', ?)",
            {requestId, fromUserId, message}
        );
        
        if (senderNotificationQuery.lastError().isValid()) {
            LOG_WARNING(QString("Failed to create friend request notification for sender: %1").arg(senderNotificationQuery.lastError().text()));
        }
        
        // 检查目标用户在线状态并发送通知
        OnlineStatusService* statusService = OnlineStatusService::instance();
    
        if (statusService && statusService->isUserOnline(toUserId)) {
            // 用户在线，实时推送
        
            
            // 构建好友请求通知消息
            QJsonObject notificationMessage;
            notificationMessage["action"] = "friend_request_notification";
            notificationMessage["notification_type"] = "friend_request";
            notificationMessage["request_id"] = requestId;
            notificationMessage["from_user_id"] = fromUserId;
            notificationMessage["from_username"] = getUsernameById(fromUserId);
            notificationMessage["from_display_name"] = getDisplayNameById(fromUserId);
            notificationMessage["message"] = message;
            notificationMessage["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            // 通过ThreadPoolServer发送实时通知
            ThreadPoolServer* server = ThreadPoolServer::instance();
            if (server) {
                bool sent = server->sendMessageToUser(toUserId, notificationMessage);
                if (sent) {
                
                } else {
                    LOG_WARNING(QString("Failed to send real-time friend request notification to user %1").arg(toUserId));
                }
            } else {
                LOG_ERROR("ThreadPoolServer instance not available for friend request notification");
            }
        } else {
            // 用户离线，存储到离线消息队列
        
            
            if (!addToOfflineQueue(toUserId, requestId, 2)) { // 优先级2：好友请求
                LOG_ERROR(QString("Failed to add friend request to offline queue for user %1").arg(toUserId));
            }
        }
        
        // 发送信号
        emit friendRequestSent(fromUserId, toUserId, requestId, message);
        
        // 好友请求已发送
        return Success;
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during friend request: %1").arg(e.what()));
        return DatabaseError;
    }
}



bool FriendService::respondToFriendRequest(qint64 userId, qint64 requestId, bool accept, 
                                         const QString& note, const QString& groupName)
{
    QMutexLocker locker(&_mutex);
    

    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend request response");
        return false;
    }
    
    // 验证好友请求
    QSqlQuery query = dbConn.executeQuery(
        "SELECT requester_id, target_id, target_status FROM friend_requests WHERE id = ? AND target_id = ? AND target_status = 'pending'", 
        {requestId, userId}
    );
    
    if (query.lastError().isValid() || !query.next()) {
        LOG_WARNING(QString("Invalid friend request: %1 for user %2").arg(requestId).arg(userId));
        return false;
    }
    
    qint64 requesterId = query.value("requester_id").toLongLong();

    
    try {
        if (accept) {

            
            // 调用存储过程处理接受好友请求
            QSqlQuery acceptQuery = dbConn.executeQuery(
                "CALL AcceptFriendRequest(?, ?, ?)",
                {requestId, note, groupName}
            );
            
            if (acceptQuery.lastError().isValid()) {
                LOG_ERROR(QString("AcceptFriendRequest存储过程执行失败: %1").arg(acceptQuery.lastError().text()));
                throw std::runtime_error(acceptQuery.lastError().text().toStdString());
            }
            

            
            // 为双方创建已处理的通知记录（如果不存在）
            QSqlQuery checkNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, requesterId}
            );
            
            if (!checkNotificationQuery.lastError().isValid() && checkNotificationQuery.next() && checkNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery notificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_accepted', ?)",
                    {requestId, requesterId, QString("好友请求已接受")}
                );
                
                if (notificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create acceptance notification for requester: %1").arg(notificationQuery.lastError().text()));
                }
            }
            
            QSqlQuery checkTargetNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, userId}
            );
            
            if (!checkTargetNotificationQuery.lastError().isValid() && checkTargetNotificationQuery.next() && checkTargetNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery targetNotificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_accepted', ?)",
                    {requestId, userId, QString("您已接受好友请求")}
                );
                
                if (targetNotificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create acceptance notification for target: %1").arg(targetNotificationQuery.lastError().text()));
                }
            }
            

            
            // 检查请求者到接受者的关系
            QSqlQuery checkRequesterToTarget = dbConn.executeQuery(
                "SELECT id, status, accepted_at FROM friendships WHERE user_id = ? AND friend_id = ?",
                {requesterId, userId}
            );
            
            if (!checkRequesterToTarget.lastError().isValid()) {
                if (checkRequesterToTarget.next()) {

                } else {
                    LOG_ERROR("未找到请求者到接受者的好友关系记录");
                }
            } else {
                LOG_ERROR(QString("查询请求者到接受者关系失败: %1").arg(checkRequesterToTarget.lastError().text()));
            }
            
            // 检查接受者到请求者的关系
            QSqlQuery checkTargetToRequester = dbConn.executeQuery(
                "SELECT id, status, accepted_at FROM friendships WHERE user_id = ? AND friend_id = ?",
                {userId, requesterId}
            );
            
            if (!checkTargetToRequester.lastError().isValid()) {
                if (checkTargetToRequester.next()) {

                } else {
                    LOG_ERROR("未找到接受者到请求者的好友关系记录");
                }
            } else {
                LOG_ERROR(QString("查询接受者到请求者关系失败: %1").arg(checkTargetToRequester.lastError().text()));
            }
            
            // 统计总的好友关系数量
            QSqlQuery verifyFriendshipQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friendships WHERE "
                "(user_id = ? AND friend_id = ? AND status = 'accepted') OR "
                "(user_id = ? AND friend_id = ? AND status = 'accepted')",
                {requesterId, userId, userId, requesterId}
            );
            
            if (!verifyFriendshipQuery.lastError().isValid() && verifyFriendshipQuery.next()) {
                int friendshipCount = verifyFriendshipQuery.value("count").toInt();
                
                // 如果未找到任何好友关系记录，则尝试为双方手动创建关系
                if (friendshipCount == 0) {
                    // 为请求者创建好友关系
                    QSqlQuery createFriendship1 = dbConn.executeQuery(
                        "INSERT INTO friendships (user_id, friend_id, status, accepted_at) VALUES (?, ?, 'accepted', NOW())",
                        {requesterId, userId}
                    );
                    if (createFriendship1.lastError().isValid()) {
                        LOG_ERROR(QString("创建请求者好友关系失败: %1").arg(createFriendship1.lastError().text()));
                    }

                    // 为接受者创建好友关系
                    QSqlQuery createFriendship2 = dbConn.executeQuery(
                        "INSERT INTO friendships (user_id, friend_id, status, accepted_at) VALUES (?, ?, 'accepted', NOW())",
                        {userId, requesterId}
                    );
                    if (createFriendship2.lastError().isValid()) {
                        LOG_ERROR(QString("创建接受者好友关系失败: %1").arg(createFriendship2.lastError().text()));
                    }
                }
            }
            
            // 发送信号
            emit friendRequestResponded(requestId, requesterId, userId, true);

            // 在线状态服务
            OnlineStatusService* statusService = OnlineStatusService::instance();
            
            if (statusService) {
                bool requesterOnline = statusService->isUserOnline(requesterId);
                bool accepterOnline = statusService->isUserOnline(userId);
                
                if (requesterOnline) {
                    
                    QJsonObject notificationMessage;
                    notificationMessage["action"] = "friend_request_accepted";
                    notificationMessage["request_id"] = requestId;
                    notificationMessage["accepted_by_user_id"] = userId;
                    notificationMessage["accepted_by_username"] = getUsernameById(userId);
                    notificationMessage["accepted_by_display_name"] = getDisplayNameById(userId);
                    notificationMessage["note"] = note;
                    notificationMessage["group_name"] = groupName;
                    notificationMessage["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                    
                    ThreadPoolServer* server = ThreadPoolServer::instance();
                    if (server) {
                        bool sent = server->sendMessageToUser(requesterId, notificationMessage);
                        if (sent) {
        
                        } else {
                            LOG_WARNING(QString("Failed to send real-time friend acceptance notification to user %1").arg(requesterId));
                        }
                    } else {
                        LOG_ERROR("ThreadPoolServer instance not available for friend acceptance notification");
                    }
                } else {

                    if (!addToOfflineQueue(requesterId, requestId, 1)) {
                        LOG_ERROR(QString("Failed to add friend acceptance to offline queue for user %1").arg(requesterId));
                    }
                }
            } else {
                LOG_ERROR("OnlineStatusService实例不可用");
            }
            
            ThreadPoolServer* server = ThreadPoolServer::instance();
            
            if (server && statusService) {
                // 向发起者发送好友列表更新通知（仅在线用户）

                bool requesterOnline = statusService->isUserOnline(requesterId);
                
                if (requesterOnline) {
                    
                    QJsonObject notification;
                    notification["notification_type"] = "friend_list_update";
                    notification["message"] = "Your friend list has been updated";
                    notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                    
                    bool sent = server->sendMessageToUser(requesterId, notification);
                    
                } else {
                    
                }
                
                // 向接受者发送好友列表更新通知（仅在线用户）
                
                bool accepterOnline = statusService->isUserOnline(userId);
                
                if (accepterOnline) {
                    
                    QJsonObject notification;
                    notification["notification_type"] = "friend_list_update";
                    notification["message"] = "Your friend list has been updated";
                    notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                    
                    bool sent = server->sendMessageToUser(userId, notification);
                    
                } else {
                    
                }
            } else {
                LOG_ERROR("ThreadPoolServer或OnlineStatusService不可用，无法发送通知");
            }
            
        } else {
            // 调用存储过程处理拒绝好友请求
            QSqlQuery rejectQuery = dbConn.executeQuery(
                "CALL RejectFriendRequest(?)",
                {requestId}
            );
            
            if (rejectQuery.lastError().isValid()) {
                throw std::runtime_error(rejectQuery.lastError().text().toStdString());
            }
            
            // 为双方创建已处理的通知记录（如果不存在）
            QSqlQuery checkNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, requesterId}
            );
            
            if (!checkNotificationQuery.lastError().isValid() && checkNotificationQuery.next() && checkNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery notificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_rejected', ?)",
                    {requestId, requesterId, QString("好友请求已被拒绝")}
                );
                
                if (notificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create rejection notification for requester: %1").arg(notificationQuery.lastError().text()));
                }
            }
            
            QSqlQuery checkTargetNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, userId}
            );
            
            if (!checkTargetNotificationQuery.lastError().isValid() && checkTargetNotificationQuery.next() && checkTargetNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery targetNotificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_rejected', ?)",
                    {requestId, userId, QString("您已拒绝好友请求")}
                );
                
                if (targetNotificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create rejection notification for target: %1").arg(targetNotificationQuery.lastError().text()));
                }
            }
            
            // 确保请求状态已更新
            QSqlQuery verifyQuery = dbConn.executeQuery(
                "SELECT status FROM friend_requests WHERE id = ?",
                {requestId}
            );
            
            if (!verifyQuery.lastError().isValid() && verifyQuery.next()) {
                QString status = verifyQuery.value("status").toString();
    
            }
            
            // 发送信号
            emit friendRequestResponded(requestId, requesterId, userId, false);
            
            // 向发起者发送实时通知
            OnlineStatusService* statusService = OnlineStatusService::instance();
            if (statusService && statusService->isUserOnline(requesterId)) {
                
                QJsonObject notificationMessage;
                notificationMessage["action"] = "friend_request_rejected";
                notificationMessage["request_id"] = requestId;
                notificationMessage["rejected_by_user_id"] = userId;
                notificationMessage["rejected_by_username"] = getUsernameById(userId);
                notificationMessage["rejected_by_display_name"] = getDisplayNameById(userId);
                notificationMessage["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                
                ThreadPoolServer* server = ThreadPoolServer::instance();
                if (server) {
                    bool sent = server->sendMessageToUser(requesterId, notificationMessage);
                    if (sent) {
    
                    } else {
                        LOG_WARNING(QString("Failed to send real-time friend rejection notification to user %1").arg(requesterId));
                    }
                } else {
                    LOG_ERROR("ThreadPoolServer instance not available for friend rejection notification");
                }
            } else {

                if (!addToOfflineQueue(requesterId, requestId, 2)) {
                    LOG_ERROR(QString("Failed to add friend rejection to offline queue for user %1").arg(requesterId));
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Failed to respond to friend request: %1").arg(e.what()));
        return false;
    }
}

bool FriendService::ignoreFriendRequest(qint64 userId, qint64 requestId)
{
    QMutexLocker locker(&_mutex);
    
    try {
        // 使用RAII包装器自动管理数据库连接
        DatabaseConnection dbConn;
        if (!dbConn.isValid()) {
            LOG_ERROR("Failed to acquire database connection for friend request ignore");
            return false;
        }
        
        // 验证好友请求 - 支持用户作为接收者或发送者
        QSqlQuery query = dbConn.executeQuery(
            "SELECT requester_id, target_id, requester_status, target_status FROM friend_requests WHERE id = ? AND (target_id = ? OR requester_id = ?)", 
            {requestId, userId, userId}
        );
        
        if (query.lastError().isValid() || !query.next()) {
            LOG_WARNING(QString("Invalid friend request for ignore: %1 for user %2").arg(requestId).arg(userId));
            return false;
        }
        
        qint64 requesterId = query.value("requester_id").toLongLong();
        qint64 targetId = query.value("target_id").toLongLong();
        QString requesterStatus = query.value("requester_status").toString();
        QString targetStatus = query.value("target_status").toString();

        // 判断用户角色
        bool isRequester = (requesterId == userId);
        bool isTarget = (targetId == userId);
        
        // 根据用户角色和申请状态决定处理方式
        if (isRequester && requesterStatus == "pending") {
            // 用户是请求者且申请状态为待处理 - 取消申请
    
            // 调用存储过程处理取消好友请求
            QSqlQuery cancelQuery = dbConn.executeQuery(
                "CALL CancelFriendRequest(?)",
                {requestId}
            );
            
            if (cancelQuery.lastError().isValid()) {
                throw std::runtime_error(cancelQuery.lastError().text().toStdString());
            }

            // 为双方创建已处理的通知记录（如果不存在）
            QSqlQuery checkNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, requesterId}
            );
            
            if (!checkNotificationQuery.lastError().isValid() && checkNotificationQuery.next() && checkNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery notificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_cancelled', ?)",
                    {requestId, requesterId, QString("好友请求已被取消")}
                );
                
                if (notificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create cancellation notification for requester: %1").arg(notificationQuery.lastError().text()));
                }
            }
            
            QSqlQuery checkTargetNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, targetId}
            );
            
            if (!checkTargetNotificationQuery.lastError().isValid() && checkTargetNotificationQuery.next() && checkTargetNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery targetNotificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_cancelled', ?)",
                    {requestId, targetId, QString("您已取消好友请求")}
                );
                
                if (targetNotificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create cancellation notification for target: %1").arg(targetNotificationQuery.lastError().text()));
                }
            }
            
        } else if (isTarget && targetStatus == "pending") {
            // 用户是目标且申请状态为待处理 - 忽略申请
            
            // 调用存储过程处理忽略好友请求
            QSqlQuery ignoreQuery = dbConn.executeQuery(
                "CALL IgnoreFriendRequest(?)",
                {requestId}
            );
            
            if (ignoreQuery.lastError().isValid()) {
                throw std::runtime_error(ignoreQuery.lastError().text().toStdString());
            }
            
            // 为双方创建已处理的通知记录（如果不存在）
            QSqlQuery checkNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, requesterId}
            );
            
            if (!checkNotificationQuery.lastError().isValid() && checkNotificationQuery.next() && checkNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery notificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_ignored', ?)",
                    {requestId, requesterId, QString("好友请求已被忽略")}
                );
                
                if (notificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create ignore notification for requester: %1").arg(notificationQuery.lastError().text()));
                }
            }
            
            QSqlQuery checkTargetNotificationQuery = dbConn.executeQuery(
                "SELECT COUNT(*) as count FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, targetId}
            );
            
            if (!checkTargetNotificationQuery.lastError().isValid() && checkTargetNotificationQuery.next() && checkTargetNotificationQuery.value("count").toInt() == 0) {
                QSqlQuery targetNotificationQuery = dbConn.executeQuery(
                    "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_ignored', ?)",
                    {requestId, targetId, QString("您已忽略好友请求")}
                );
                
                if (targetNotificationQuery.lastError().isValid()) {
                    LOG_WARNING(QString("Failed to create ignore notification for target: %1").arg(targetNotificationQuery.lastError().text()));
                }
            }
            
            // 检查请求者是否在线，如果在线则发送通知
            OnlineStatusService* statusService = OnlineStatusService::instance();
            if (statusService && statusService->isUserOnline(requesterId)) {
                QJsonObject notification;
                notification["action"] = "friend_request_notification";
                notification["notification_type"] = "request_ignored";
                notification["request_id"] = requestId;
                notification["message"] = "您的好友请求已被忽略";
                notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                
                ThreadPoolServer* server = ThreadPoolServer::instance();
                if (server) {
                    server->sendMessageToUser(requesterId, notification);
        
                }
            }
        } else {
            // 其他情况（已处理的申请）- 只清理当前用户的通知记录，不删除整个申请记录

            
            // 只删除当前用户的通知记录，保留申请记录供另一方查看
            QSqlQuery deleteNotificationQuery = dbConn.executeQuery(
                "DELETE FROM friend_request_notifications WHERE request_id = ? AND user_id = ?",
                {requestId, userId}
            );
            
            if (deleteNotificationQuery.lastError().isValid()) {
                LOG_WARNING(QString("Failed to delete friend request notifications: %1").arg(deleteNotificationQuery.lastError().text()));
            } else {
    
            }
            
            // 注意：不删除整个申请记录，也不调用CleanupProcessedFriendRequest
            // 这样确保另一方仍然可以看到申请状态
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during friend request ignore: %1").arg(e.what()));
        return false;
    }
}

QJsonArray FriendService::getFriendList(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    QJsonArray friendList;
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend list");
        return friendList;
    }
    
    // 首先检查friendships表中是否有数据

    QSqlQuery checkQuery = dbConn.executeQuery(
        "SELECT COUNT(*) as total_count FROM friendships WHERE status = 'accepted'"
    );
    
    if (!checkQuery.lastError().isValid() && checkQuery.next()) {
        int totalCount = checkQuery.value("total_count").toInt();
    
    } else {
        LOG_ERROR(QString("查询friendships表总数失败: %1").arg(checkQuery.lastError().text()));
    }
    
    // 检查当前用户的好友关系
    QSqlQuery userCheckQuery = dbConn.executeQuery(
        "SELECT COUNT(*) as user_count FROM friendships WHERE user_id = ? AND status = 'accepted'",
        {userId}
    );
    
    if (!userCheckQuery.lastError().isValid() && userCheckQuery.next()) {
        int userCount = userCheckQuery.value("user_count").toInt();
    
    } else {
        LOG_ERROR(QString("查询用户 %1 好友关系数量失败: %2").arg(userId).arg(userCheckQuery.lastError().text()));
    }
    
    // 详细检查当前用户的所有friendships记录

    QSqlQuery detailedQuery = dbConn.executeQuery(
        "SELECT id, user_id, friend_id, status, accepted_at, note, group_id FROM friendships WHERE user_id = ?",
        {userId}
    );
    
    if (!detailedQuery.lastError().isValid()) {
        int recordCount = 0;
        while (detailedQuery.next()) {
            recordCount++;
            
        }
    
    } else {
        LOG_ERROR(QString("查询用户 %1 详细friendships记录失败: %2").arg(userId).arg(detailedQuery.lastError().text()));
    }
    
    // 查询好友列表（关联分组信息）

    QSqlQuery query = dbConn.executeQuery(
        "SELECT f.id as friendship_id, f.friend_id, f.note, f.accepted_at, f.group_id, "
        "u.username, u.display_name, u.avatar_url, "
        "fg.group_name, fg.group_order "
        "FROM friendships f "
        "JOIN users u ON f.friend_id = u.id "
        "LEFT JOIN friend_groups fg ON f.group_id = fg.id "
        "WHERE f.user_id = ? AND f.status = 'accepted' "
        "ORDER BY COALESCE(fg.group_order, 999999), u.display_name ASC",
        {userId}
    );
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get friend list for user %1: %2").arg(userId).arg(query.lastError().text()));
        return friendList;
    }
    
    while (query.next()) {
        QJsonObject friendInfo;
        friendInfo["friendship_id"] = query.value("friendship_id").toLongLong();
        friendInfo["friend_id"] = query.value("friend_id").toLongLong();
        friendInfo["username"] = query.value("username").toString();
        friendInfo["display_name"] = query.value("display_name").toString();
        friendInfo["avatar_url"] = query.value("avatar_url").toString();
        friendInfo["online_status"] = "offline"; // 默认离线状态
        friendInfo["last_seen"] = query.value("accepted_at").toDateTime().toString(Qt::ISODate);
        friendInfo["note"] = query.value("note").toString();
        friendInfo["accepted_at"] = query.value("accepted_at").toDateTime().toString(Qt::ISODate);
        friendInfo["group_id"] = query.value("group_id").toLongLong();
        friendInfo["group_name"] = query.value("group_name").toString();
        friendInfo["group_order"] = query.value("group_order").toInt();
        
        friendList.append(friendInfo);
    }
    
    // 如果没有找到好友，记录详细信息
    if (friendList.isEmpty()) {
        LOG_WARNING(QString("用户 %1 没有找到任何好友，可能的原因:").arg(userId));
        LOG_WARNING("1. friendships表中没有该用户的好友关系记录");
        LOG_WARNING("2. 好友关系状态不是'accepted'");
        LOG_WARNING("3. 关联的用户数据不存在");
        
        // 检查该用户的所有friendships记录
        QSqlQuery allFriendshipsQuery = dbConn.executeQuery(
            "SELECT * FROM friendships WHERE user_id = ?",
            {userId}
        );
        
        if (!allFriendshipsQuery.lastError().isValid()) {
            int recordCount = 0;
            while (allFriendshipsQuery.next()) {
                recordCount++;
                qint64 friendId = allFriendshipsQuery.value("friend_id").toLongLong();
                QString status = allFriendshipsQuery.value("status").toString();
                LOG_WARNING(QString("记录 %1: friend_id=%2, status='%3'").arg(recordCount).arg(friendId).arg(status));
            }
            LOG_WARNING(QString("用户 %1 总共有 %2 条friendships记录").arg(userId).arg(recordCount));
        }
    }
    
    // 获取好友列表成功
    return friendList;
}

QJsonArray FriendService::getPendingFriendRequests(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    QJsonArray requestList;
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for pending friend requests");
        return requestList;
    }
    
    // 1. 查询待处理的好友请求（作为接收者）
    QSqlQuery pendingQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.message, fr.requested_at, "
        "u.username as requester_username, u.display_name as requester_display_name, u.avatar_url as requester_avatar_url "
        "FROM friend_requests fr "
        "JOIN users u ON fr.requester_id = u.id "
        "WHERE fr.target_id = ? AND fr.target_status = 'pending' "
        "ORDER BY fr.requested_at DESC",
        {userId}
    );
    
    if (pendingQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get pending friend requests for user %1: %2").arg(userId).arg(pendingQuery.lastError().text()));
    } else {
        // 处理待处理的请求
        while (pendingQuery.next()) {
            QJsonObject requestInfo;
            requestInfo["request_id"] = pendingQuery.value("request_id").toLongLong();
            requestInfo["friendship_id"] = pendingQuery.value("request_id").toLongLong(); // 兼容性
            requestInfo["requester_id"] = pendingQuery.value("requester_id").toLongLong();
            requestInfo["requester_username"] = pendingQuery.value("requester_username").toString();
            requestInfo["requester_display_name"] = pendingQuery.value("requester_display_name").toString();
            requestInfo["requester_avatar_url"] = pendingQuery.value("requester_avatar_url").toString();
            requestInfo["requested_at"] = pendingQuery.value("requested_at").toDateTime().toString(Qt::ISODate);
            requestInfo["message"] = pendingQuery.value("message").toString();
            requestInfo["status"] = "pending"; // 待处理状态
            requestInfo["request_type"] = "received"; // 标记为收到的申请

            requestList.append(requestInfo);
        }
    }

    // 2. 查询我发送的待处理好友请求
    QSqlQuery sentPendingQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.message, fr.requested_at, "
        "u.username as target_username, u.display_name as target_display_name, u.avatar_url as target_avatar_url "
        "FROM friend_requests fr "
        "JOIN users u ON fr.target_id = u.id "
        "WHERE fr.requester_id = ? AND fr.requester_status = 'pending' "
        "ORDER BY fr.requested_at DESC",
        {userId}
    );
    
    if (sentPendingQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get sent pending friend requests for user %1: %2").arg(userId).arg(sentPendingQuery.lastError().text()));
    } else {
        // 处理我发送的待处理请求
        while (sentPendingQuery.next()) {
            QJsonObject requestInfo;
            requestInfo["request_id"] = sentPendingQuery.value("request_id").toLongLong();
            requestInfo["friendship_id"] = sentPendingQuery.value("request_id").toLongLong(); // 兼容性
            requestInfo["requester_id"] = sentPendingQuery.value("target_id").toLongLong(); // 显示目标用户信息
            requestInfo["requester_username"] = sentPendingQuery.value("target_username").toString();
            requestInfo["requester_display_name"] = sentPendingQuery.value("target_display_name").toString();
            requestInfo["requester_avatar_url"] = sentPendingQuery.value("target_avatar_url").toString();
            requestInfo["requested_at"] = sentPendingQuery.value("requested_at").toDateTime().toString(Qt::ISODate);
            requestInfo["message"] = sentPendingQuery.value("message").toString();
            requestInfo["status"] = "pending"; // 待处理状态
            requestInfo["request_type"] = "sent"; // 标记为我发送的申请

            requestList.append(requestInfo);
        }
    }

    // 3. 查询我发送的已处理好友请求（我向别人发送的申请）
    QSqlQuery sentProcessedQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.requester_status, fr.target_status, fr.requested_at, fr.responded_at, "
        "fr.response_note, fr.response_group_id, "
        "u.username as target_username, u.display_name as target_display_name, u.avatar_url as target_avatar_url, "
        "fg.group_name as response_group_name "
        "FROM friend_requests fr "
        "JOIN users u ON fr.target_id = u.id "
        "LEFT JOIN friend_groups fg ON fr.response_group_id = fg.id "
        "WHERE fr.requester_id = ? AND fr.requester_status IN ('accepted', 'rejected', 'ignored', 'cancelled') "
        "AND EXISTS ("
        "    SELECT 1 FROM friend_request_notifications frn "
        "    WHERE frn.request_id = fr.id AND frn.user_id = ?"
        ") "
        "AND NOT EXISTS ("
        "    SELECT 1 FROM friendships f "
        "    WHERE (f.user_id = fr.requester_id AND f.friend_id = fr.target_id) "
        "    AND f.status = 'deleted'"
        ") "
        "ORDER BY fr.responded_at DESC",
        {userId, userId}
    );
    
    if (sentProcessedQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get sent processed friend requests for user %1: %2").arg(userId).arg(sentProcessedQuery.lastError().text()));
    } else {
        // 处理我发送的已处理请求
        while (sentProcessedQuery.next()) {
            QJsonObject requestInfo;
            requestInfo["request_id"] = sentProcessedQuery.value("request_id").toLongLong();
            requestInfo["friendship_id"] = sentProcessedQuery.value("request_id").toLongLong(); // 兼容性
            requestInfo["requester_id"] = sentProcessedQuery.value("target_id").toLongLong(); // 显示目标用户信息
            requestInfo["requester_username"] = sentProcessedQuery.value("target_username").toString();
            requestInfo["requester_display_name"] = sentProcessedQuery.value("target_display_name").toString();
            requestInfo["requester_avatar_url"] = sentProcessedQuery.value("target_avatar_url").toString();
            requestInfo["requested_at"] = sentProcessedQuery.value("responded_at").toDateTime().toString(Qt::ISODate);
            requestInfo["message"] = sentProcessedQuery.value("response_note").toString();
            requestInfo["status"] = sentProcessedQuery.value("requester_status").toString(); // accepted 或 rejected
            requestInfo["request_type"] = "sent_processed"; // 标记为我发送的已处理申请

            requestList.append(requestInfo);
        }
    }

    // 4. 查询我接收的已处理好友请求（别人向我发送的申请，我已经处理了）
    QSqlQuery receivedProcessedQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.requester_status, fr.target_status, fr.requested_at, fr.responded_at, "
        "fr.response_note, fr.response_group_id, "
        "u.username as requester_username, u.display_name as requester_display_name, u.avatar_url as requester_avatar_url, "
        "fg.group_name as response_group_name "
        "FROM friend_requests fr "
        "JOIN users u ON fr.requester_id = u.id "
        "LEFT JOIN friend_groups fg ON fr.response_group_id = fg.id "
        "WHERE fr.target_id = ? AND fr.target_status IN ('accepted', 'rejected', 'ignored') "
        "AND EXISTS ("
        "    SELECT 1 FROM friend_request_notifications frn "
        "    WHERE frn.request_id = fr.id AND frn.user_id = ?"
        ") "
        "AND NOT EXISTS ("
        "    SELECT 1 FROM friendships f "
        "    WHERE (f.user_id = fr.target_id AND f.friend_id = fr.requester_id) "
        "    AND f.status = 'deleted'"
        ") "
        "ORDER BY fr.responded_at DESC",
        {userId, userId}
    );
    
    if (receivedProcessedQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get received processed friend requests for user %1: %2").arg(userId).arg(receivedProcessedQuery.lastError().text()));
    } else {
        // 处理我接收的已处理请求
        while (receivedProcessedQuery.next()) {
            QJsonObject requestInfo;
            requestInfo["request_id"] = receivedProcessedQuery.value("request_id").toLongLong();
            requestInfo["friendship_id"] = receivedProcessedQuery.value("request_id").toLongLong(); // 兼容性
            requestInfo["requester_id"] = receivedProcessedQuery.value("requester_id").toLongLong(); // 显示请求者信息
            requestInfo["requester_username"] = receivedProcessedQuery.value("requester_username").toString();
            requestInfo["requester_display_name"] = receivedProcessedQuery.value("requester_display_name").toString();
            requestInfo["requester_avatar_url"] = receivedProcessedQuery.value("requester_avatar_url").toString();
            requestInfo["requested_at"] = receivedProcessedQuery.value("responded_at").toDateTime().toString(Qt::ISODate);
            requestInfo["message"] = receivedProcessedQuery.value("response_note").toString();
            requestInfo["status"] = receivedProcessedQuery.value("target_status").toString(); // accepted 或 rejected
            requestInfo["request_type"] = "received_processed"; // 标记为我接收的已处理申请

            requestList.append(requestInfo);
            
            LOG_INFO(QString("Found received processed friend request: from %1 to %2, request_id: %3, status: %4")
                    .arg(receivedProcessedQuery.value("requester_username").toString())
                    .arg(userId)
                    .arg(receivedProcessedQuery.value("request_id").toLongLong())
                    .arg(receivedProcessedQuery.value("status").toString()));
        }
    }

    // 获取待处理好友请求成功
    return requestList;
}

bool FriendService::deleteFriendRequestNotification(qint64 userId, qint64 requestId)
{
    QMutexLocker locker(&_mutex);
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for deleting friend request notification");
        return false;
    }
    
    // 开始事务
    if (!dbConn.beginTransaction()) {
        LOG_ERROR("Failed to start transaction for deleting friend request notification");
        return false;
    }
    
    try {
        // 删除好友请求通知记录
        QSqlQuery deleteNotificationQuery = dbConn.executeQuery(
            "DELETE FROM friend_request_notifications WHERE user_id = ? AND request_id = ?",
            {userId, requestId}
        );
        
        if (deleteNotificationQuery.lastError().isValid()) {
            throw std::runtime_error(deleteNotificationQuery.lastError().text().toStdString());
        }
        
        int affectedNotifications = deleteNotificationQuery.numRowsAffected();
    
        // 删除对应的好友申请记录
        QSqlQuery deleteRequestQuery = dbConn.executeQuery(
            "DELETE FROM friend_requests WHERE id = ?",
            {requestId}
        );
        
        if (deleteRequestQuery.lastError().isValid()) {
            throw std::runtime_error(deleteRequestQuery.lastError().text().toStdString());
        }
        
        int affectedRequests = deleteRequestQuery.numRowsAffected();
    
        // 提交事务
        if (!dbConn.commitTransaction()) {
            throw std::runtime_error("Failed to commit delete friend request notification transaction");
        }
        
        return (affectedNotifications > 0 || affectedRequests > 0);
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in deleteFriendRequestNotification: %1").arg(e.what()));
        dbConn.rollbackTransaction();
        return false;
    }
}

bool FriendService::removeFriend(qint64 userId, qint64 friendId)
{
    QMutexLocker locker(&_mutex);

    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend removal");
        return false;
    }

    // 开始事务
    if (!dbConn.beginTransaction()) {
        LOG_ERROR("Failed to start transaction for friend removal");
        return false;
    }

    try {
        // 删除双向好友关系
        QSqlQuery updateQuery = dbConn.executeQuery(
            "UPDATE friendships SET status = 'deleted' WHERE "
            "(user_id = ? AND friend_id = ?) OR "
            "(user_id = ? AND friend_id = ?)",
            {userId, friendId, friendId, userId}
        );

        if (updateQuery.lastError().isValid()) {
            throw std::runtime_error(updateQuery.lastError().text().toStdString());
        }

        // 删除两个用户之间的所有聊天记录
        QSqlQuery deleteMessagesQuery = dbConn.executeQuery(
            "DELETE FROM messages WHERE "
            "(sender_id = ? AND receiver_id = ?) OR "
            "(sender_id = ? AND receiver_id = ?)",
            {userId, friendId, friendId, userId}
        );

        if (deleteMessagesQuery.lastError().isValid()) {
            throw std::runtime_error(deleteMessagesQuery.lastError().text().toStdString());
        }

        // 删除相关的离线消息队列记录
        QSqlQuery deleteOfflineQuery = dbConn.executeQuery(
            "DELETE omq FROM offline_message_queue omq "
            "JOIN messages m ON omq.message_id = m.id "
            "WHERE (m.sender_id = ? AND m.receiver_id = ?) OR "
            "(m.sender_id = ? AND m.receiver_id = ?)",
            {userId, friendId, friendId, userId}
        );

        if (deleteOfflineQuery.lastError().isValid()) {
            // 离线消息删除失败不影响主流程，只记录警告
            LOG_WARNING(QString("Failed to delete offline messages: %1").arg(deleteOfflineQuery.lastError().text()));
        }

        // 删除消息已读状态记录
        QSqlQuery deleteReadStatusQuery = dbConn.executeQuery(
            "DELETE mrs FROM message_read_status mrs "
            "JOIN messages m ON mrs.message_id = m.id "
            "WHERE (m.sender_id = ? AND m.receiver_id = ?) OR "
            "(m.sender_id = ? AND m.receiver_id = ?)",
            {userId, friendId, friendId, userId}
        );

        if (deleteReadStatusQuery.lastError().isValid()) {
            // 已读状态删除失败不影响主流程，只记录警告
            LOG_WARNING(QString("Failed to delete message read status: %1").arg(deleteReadStatusQuery.lastError().text()));
        }

        // 删除两个用户之间的所有好友申请记录
        QSqlQuery deleteFriendRequestsQuery = dbConn.executeQuery(
            "DELETE FROM friend_requests WHERE "
            "(requester_id = ? AND target_id = ?) OR "
            "(requester_id = ? AND target_id = ?)",
            {userId, friendId, friendId, userId}
        );

        if (deleteFriendRequestsQuery.lastError().isValid()) {
            // 好友申请删除失败不影响主流程，只记录警告
            LOG_WARNING(QString("Failed to delete friend requests: %1").arg(deleteFriendRequestsQuery.lastError().text()));
        } else {
            int affectedRequests = deleteFriendRequestsQuery.numRowsAffected();
            LOG_INFO(QString("Deleted %1 friend request records between users %2 and %3").arg(affectedRequests).arg(userId).arg(friendId));
        }

        // 删除相关的好友申请通知记录
        QSqlQuery deleteNotificationsQuery = dbConn.executeQuery(
            "DELETE frn FROM friend_request_notifications frn "
            "JOIN friend_requests fr ON frn.request_id = fr.id "
            "WHERE (fr.requester_id = ? AND fr.target_id = ?) OR "
            "(fr.requester_id = ? AND fr.target_id = ?)",
            {userId, friendId, friendId, userId}
        );

        if (deleteNotificationsQuery.lastError().isValid()) {
            // 通知删除失败不影响主流程，只记录警告
            LOG_WARNING(QString("Failed to delete friend request notifications: %1").arg(deleteNotificationsQuery.lastError().text()));
        } else {
            int affectedNotifications = deleteNotificationsQuery.numRowsAffected();
            LOG_INFO(QString("Deleted %1 friend request notification records between users %2 and %3").arg(affectedNotifications).arg(userId).arg(friendId));
        }

        // 提交事务
        if (!dbConn.commitTransaction()) {
            throw std::runtime_error("Failed to commit remove friend transaction");
        }

        // 好友关系和聊天记录已删除
        LOG_INFO(QString("Successfully removed friend relationship and chat history between users %1 and %2")
                 .arg(userId).arg(friendId));

        // 发送信号
        emit friendRemoved(userId, friendId);
        
        // 通知被删除的好友
        QJsonObject notification;
        notification["action"] = "friend_removed";
        notification["remover_id"] = userId;
        notification["removed_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        // 通过MessageService推送通知
        MessageService* messageService = MessageService::instance();
        if (messageService) {
            messageService->pushMessageToUser(friendId, notification);
        }

        return true;

    } catch (const std::exception& e) {
        dbConn.rollbackTransaction();
        LOG_ERROR(QString("Failed to remove friend: %1").arg(e.what()));
        return false;
    }
}

bool FriendService::blockUser(qint64 userId, qint64 targetUserId)
{
    QMutexLocker locker(&_mutex);
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for blocking user");
        return false;
    }
    
    // 检查是否已存在好友关系
    QSqlQuery query = dbConn.executeQuery("SELECT id, status FROM friendships WHERE user_id = ? AND friend_id = ?",
                                         {userId, targetUserId});

    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to check existing relationship for blocking: %1").arg(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        // 更新现有关系为屏蔽
        qint64 friendshipId = query.value("id").toLongLong();
        QSqlQuery updateQuery = dbConn.executeQuery(
            "UPDATE friendships SET status = 'blocked', blocked_at = NOW() WHERE id = ?",
            {friendshipId}
        );
        
        if (updateQuery.lastError().isValid()) {
            LOG_ERROR(QString("Failed to block user %1 by user %2: %3").arg(targetUserId).arg(userId).arg(updateQuery.lastError().text()));
            return false;
        }
    } else {
        // 创建新的屏蔽关系
        QSqlQuery insertQuery = dbConn.executeQuery(
            "INSERT INTO friendships (user_id, friend_id, status, blocked_at) "
            "VALUES (?, ?, 'blocked', NOW())",
            {userId, targetUserId}
        );
        
        if (insertQuery.lastError().isValid()) {
            LOG_ERROR(QString("Failed to block user %1 by user %2: %3").arg(targetUserId).arg(userId).arg(insertQuery.lastError().text()));
            return false;
        }
    }

    // 用户已屏蔽
    return true;
}

bool FriendService::unblockUser(qint64 userId, qint64 targetUserId)
{
    QMutexLocker locker(&_mutex);
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for unblocking user");
        return false;
    }
    
    auto result = dbConn.executeUpdate("UPDATE friendships SET status = 'deleted' WHERE user_id = ? AND friend_id = ? AND status = 'blocked'",
                           {userId, targetUserId});
    if (result == -1) {
        LOG_ERROR(QString("Failed to unblock user %1 by user %2").arg(targetUserId).arg(userId));
        return false;
    }

    if (result > 0) {
        // 用户已取消屏蔽
        return true;
    }

    LOG_WARNING(QString("No blocked relationship found between users %1 and %2").arg(userId).arg(targetUserId));
    return false;
}

bool FriendService::areFriends(qint64 userId1, qint64 userId2)
{
    QMutexLocker locker(&_mutex);

    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for checking friendship");
        return false;
    }

    QSqlQuery query = dbConn.executeQuery("SELECT COUNT(*) FROM friendships WHERE "
                                         "((user_id = ? AND friend_id = ?) OR "
                                         "(user_id = ? AND friend_id = ?)) AND status = 'accepted'",
                                         {userId1, userId2, userId2, userId1});

    if (query.lastError().isValid() || !query.next()) {
        LOG_ERROR(QString("Failed to check friendship between users %1 and %2").arg(userId1).arg(userId2));
        return false;
    }

    return query.value(0).toInt() > 0;
}

FriendService::FriendshipStatus FriendService::getFriendshipStatus(qint64 userId1, qint64 userId2)
{
    QMutexLocker locker(&_mutex);

    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for getting friendship status");
        return FriendshipStatus::Deleted; // 使用Deleted代替None
    }

    QSqlQuery query = dbConn.executeQuery("SELECT status FROM friendships WHERE "
                                         "(user_id = ? AND friend_id = ?) OR "
                                         "(user_id = ? AND friend_id = ?) "
                                         "ORDER BY requested_at DESC LIMIT 1",
                                         {userId1, userId2, userId2, userId1});

    if (query.lastError().isValid() || !query.next()) {
        return FriendshipStatus::Deleted; // 没有关系记录，使用Deleted代替None
    }

    QString status = query.value("status").toString();
    if (status == "pending") return Pending;
    if (status == "accepted") return Accepted;
    if (status == "blocked") return Blocked;
    return Deleted;
}

QJsonArray FriendService::searchUsers(const QString& keyword, qint64 currentUserId, int limit)
{
    QMutexLocker locker(&_mutex);

    // 在数据库中搜索用户

    // 1. 限流检查
    QString clientId = QString::number(currentUserId);
    if (!RateLimitManager::instance()->checkRateLimit(clientId, "friend_search", currentUserId)) {
        LOG_WARNING(QString("Rate limit exceeded for user %1 searching keyword '%2'").arg(currentUserId).arg(keyword));
        return QJsonArray(); // 返回空结果
    }

    // 2. 检查L1缓存（内存缓存）
    CacheManager* cacheManager = CacheManager::instance();
    QJsonArray cachedResults = cacheManager->getSearchCache(keyword, currentUserId);
    if (!cachedResults.isEmpty()) {
        // L1缓存命中
        return cachedResults;
    }

    // 3. 检查L2缓存（数据库缓存）
    QString l2CacheKey = QString("search:%1:%2").arg(keyword, QString::number(currentUserId));
    QJsonObject l2CacheData = cacheManager->getL2Cache(l2CacheKey);
    if (!l2CacheData.isEmpty()) {
        QJsonArray l2Results = l2CacheData["cache_data"].toArray();
        if (!l2Results.isEmpty()) {
            // L2缓存命中
            
            // 将L2缓存结果提升到L1缓存
            cacheManager->setSearchCache(keyword, currentUserId, l2Results, 300);
            
            return l2Results;
        }
    }

    // 4. 检查是否为热点数据，如果是则优先处理
    bool isHotData = cacheManager->isHotData("user_search", keyword, 5);
    if (isHotData) {
        // 检测到热点数据
    }

    // 5. 数据库查询
    QJsonArray userList;
    
    // 使用RAII模式的DatabaseConnection确保连接自动释放
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for user search");
        return userList;
    }
    
    // 使用优化的查询语句
    QSqlQuery query = dbConn.executeQuery(
        "SELECT id, user_id, username, display_name, avatar_url, status "
        "FROM users WHERE "
        "(username LIKE ? OR email LIKE ? OR display_name LIKE ? OR user_id = ?) "
        "AND id != ? AND status = 'active' "
        "ORDER BY "
        "CASE WHEN user_id = ? THEN 1 "
        "WHEN username = ? THEN 2 "
        "WHEN username LIKE ? THEN 3 "
        "WHEN display_name LIKE ? THEN 4 "
        "ELSE 5 END "
        "LIMIT ?",
        {
            QString("%%1%").arg(keyword),  // username LIKE
            QString("%%1%").arg(keyword),  // email LIKE  
            QString("%%1%").arg(keyword),  // display_name LIKE
            keyword,                       // user_id = exact_id
            currentUserId,                 // current_user
            keyword,                       // exact_id for ordering
            keyword,                       // exact_keyword for ordering
            QString("%%1%").arg(keyword),  // username LIKE for ordering
            QString("%%1%").arg(keyword),  // display_name LIKE for ordering
            limit                          // limit
        }
    );

    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to search users with keyword '%1': %2").arg(keyword).arg(query.lastError().text()));
        return userList;
    }

    // 搜索查询执行成功

    while (query.next()) {
        QJsonObject userInfo;
        qint64 userId = query.value("id").toLongLong();

        userInfo["id"] = userId;
        userInfo["user_id"] = query.value("user_id").toString();
        userInfo["username"] = query.value("username").toString();
        userInfo["display_name"] = query.value("display_name").toString();
        userInfo["avatar_url"] = query.value("avatar_url").toString();

        // 获取与当前用户的关系状态（使用内部方法避免递归锁）
        FriendshipStatus status = getFriendshipStatusInternal(currentUserId, userId);
        QString statusStr;
        switch (status) {
            case Pending: statusStr = "pending"; break;
            case Accepted: statusStr = "friends"; break;
            case Blocked: statusStr = "blocked"; break;
            default: statusStr = "none"; break;
        }
        userInfo["friendship_status"] = statusStr;

        userList.append(userInfo);
        
        // 找到用户
    }

    // 6. 缓存结果到L1和L2缓存
    if (!userList.isEmpty()) {
        // 缓存到L1（内存缓存，5分钟）
        cacheManager->setSearchCache(keyword, currentUserId, userList, 300);
        
        // 缓存到L2（数据库缓存，30分钟）
        QJsonObject l2Data;
        l2Data["results"] = userList;
        l2Data["keyword"] = keyword;
        l2Data["user_id"] = currentUserId;
        l2Data["timestamp"] = QDateTime::currentSecsSinceEpoch();
        
        cacheManager->setL2Cache(l2CacheKey, l2Data, 1800); // 30分钟TTL
        
        // 搜索结果已缓存
    }

    // 搜索完成
    return userList;
}

bool FriendService::updateFriendNote(qint64 userId, qint64 friendId, const QString& note)
{
    QMutexLocker locker(&_mutex);

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return false;
    }
    //     QSqlQuery query(db); - 需要手动处理查询

    int result = dbConn.executeUpdate("UPDATE friendships SET note = ? WHERE user_id = ? AND friend_id = ? AND status = 'accepted'", {note, userId, friendId});
    if (result == -1) {
        LOG_ERROR(QString("Failed to update friend note for user %1, friend %2: %3").arg(userId).arg(friendId).arg(dbConn.getLastError()));
        return false;
    }

    if (result > 0) {
        // 好友备注已更新
        return true;
    }

    LOG_WARNING(QString("No friendship found to update note: user %1, friend %2").arg(userId).arg(friendId));
    return false;
}

qint64 FriendService::findUserByIdentifier(const QString& identifier)
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return -1;
    }
    //     QSqlQuery query(db); - 需要手动处理查询

    // 尝试按用户ID、用户名或邮箱查找
    QSqlQuery query = dbConn.executeQuery("SELECT id FROM users WHERE (user_id = ? OR username = ? OR email = ?) AND status = 'active'", {identifier, identifier, identifier});
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to find user by identifier '%1': %2").arg(identifier).arg(query.lastError().text()));
        return -1;
    }

    if (query.next()) {
        return query.value("id").toLongLong();
    }

    return -1;
}

QString FriendService::getUsernameById(qint64 userId)
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for username lookup");
        return QString();
    }
    
    QSqlQuery query = dbConn.executeQuery("SELECT username FROM users WHERE id = ?", {userId});
    
    if (query.lastError().isValid() || !query.next()) {
        LOG_WARNING(QString("User not found for ID %1").arg(userId));
        return QString();
    }
    
    return query.value("username").toString();
}

QString FriendService::getDisplayNameById(qint64 userId)
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for display name lookup");
        return QString();
    }
    
    QSqlQuery query = dbConn.executeQuery("SELECT display_name FROM users WHERE id = ?", {userId});
    
    if (query.lastError().isValid() || !query.next()) {
        LOG_WARNING(QString("User not found for ID %1").arg(userId));
        return QString();
    }
    
    return query.value("display_name").toString();
}

bool FriendService::addToOfflineQueue(qint64 userId, qint64 requestId, int priority)
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for offline queue");
        return false;
    }
    
    int result = dbConn.executeUpdate(
        "INSERT INTO offline_message_queue (user_id, message_id, message_type, priority, created_at) "
        "VALUES (?, ?, 'friend_request', ?, NOW())",
        {userId, requestId, priority}
    );
    
    if (result == -1) {
        LOG_ERROR(QString("Failed to add friend request to offline queue for user %1").arg(userId));
        return false;
    }
    
    LOG_INFO(QString("Friend request added to offline queue for user %1, requestId %2").arg(userId).arg(requestId));
    return true;
}



FriendService::FriendshipStatus FriendService::getFriendshipStatusInternal(qint64 userId1, qint64 userId2)
{
    // 注意：此方法不加锁，调用者必须已经持有锁
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return Deleted;
    }
    QSqlQuery query = dbConn.executeQuery(
        "SELECT status FROM friendships WHERE "
        "(user_id = ? AND friend_id = ?) OR "
        "(user_id = ? AND friend_id = ?) "
        "ORDER BY requested_at DESC LIMIT 1",
        {userId1, userId2, userId2, userId1}
    );

    if (query.lastError().isValid() || !query.next()) {
        return Deleted; // 没有关系记录
    }

    QString status = query.value("status").toString();
    if (status == "pending") return Pending;
    if (status == "accepted") return Accepted;
    if (status == "blocked") return Blocked;
    return Deleted;
}

// 好友分组相关方法实现
QJsonArray FriendService::getFriendGroups(qint64 userId)
{
    QMutexLocker locker(&_mutex);

    LOG_INFO(QString("Getting friend groups for user %1").arg(userId));

    QJsonArray groupList;
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for getting friend groups");
        return groupList;
    }
    
    QSqlQuery query = dbConn.executeQuery(
        "SELECT id, group_name, group_order, 0 as friend_count "
        "FROM friend_groups WHERE user_id = ? "
        "ORDER BY group_order ASC",
        {userId}
    );

    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get friend groups for user %1: %2").arg(userId).arg(query.lastError().text()));
        return groupList;
    }

    while (query.next()) {
        QJsonObject groupInfo;
        groupInfo["id"] = query.value("id").toLongLong();
        groupInfo["group_name"] = query.value("group_name").toString();
        groupInfo["group_order"] = query.value("group_order").toInt();
        groupInfo["friend_count"] = query.value("friend_count").toInt();

        groupList.append(groupInfo);
    }
    
    // 如果没有分组，创建默认分组
    if (groupList.isEmpty()) {
        bool success = createFriendGroup(userId, "默认分组");
        if (success) {
            // 重新查询分组列表
            query = dbConn.executeQuery(
                "SELECT id, group_name, group_order, 0 as friend_count "
                "FROM friend_groups WHERE user_id = ? "
                "ORDER BY group_order ASC",
                {userId}
            );
            
            if (!query.lastError().isValid()) {
                while (query.next()) {
                    QJsonObject groupInfo;
                    groupInfo["id"] = query.value("id").toLongLong();
                    groupInfo["group_name"] = query.value("group_name").toString();
                    groupInfo["group_order"] = query.value("group_order").toInt();
                    groupInfo["friend_count"] = query.value("friend_count").toInt();
                    
                    groupList.append(groupInfo);
                }
            }
        }
    }

    LOG_INFO(QString("Returning %1 friend groups for user %2").arg(groupList.size()).arg(userId));
    return groupList;
}

bool FriendService::createFriendGroup(qint64 userId, const QString& groupName)
{
    QMutexLocker locker(&_mutex);

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return false;
    }
    // 获取最大排序号
    QSqlQuery query = dbConn.executeQuery(
        "SELECT COALESCE(MAX(group_order), 0) + 1 as next_order "
        "FROM friend_groups WHERE user_id = ?",
        {userId}
    );

    if (query.lastError().isValid() || !query.next()) {
        LOG_ERROR(QString("Failed to get next group order for user %1").arg(userId));
        return false;
    }

    int nextOrder = query.value("next_order").toInt();

    // 创建新分组
    int result = dbConn.executeUpdate(
        "INSERT INTO friend_groups (user_id, group_name, group_order) "
        "VALUES (?, ?, ?)",
        {userId, groupName, nextOrder}
    );

    if (result == -1) {
        LOG_ERROR(QString("Failed to create friend group '%1' for user %2").arg(groupName).arg(userId));
        return false;
    }

    // 好友分组已创建
    return true;
}

bool FriendService::deleteFriendGroup(qint64 userId, qint64 groupId)
{
    QMutexLocker locker(&_mutex);

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return false;
    }

    if (!dbConn.beginTransaction()) {
        LOG_ERROR("Failed to start transaction for deleting friend group");
        return false;
    }

    try {
        // 验证分组属于该用户
        QSqlQuery query = dbConn.executeQuery("SELECT id FROM friend_groups WHERE id = ? AND user_id = ?", {groupId, userId});
    if (query.lastError().isValid() || !query.next()) {
            throw std::runtime_error("Friend group not found or access denied");
        }

        // 将该分组的好友移动到默认分组
        int result1 = dbConn.executeUpdate(
            "UPDATE friendships SET group_id = "
            "(SELECT id FROM friend_groups WHERE user_id = ? AND group_name = '默认分组' LIMIT 1) "
            "WHERE user_id = ? AND group_id = ?",
            {userId, userId, groupId}
        );

        if (result1 == -1) {
            throw std::runtime_error("Failed to move friends to default group");
        }

        // 删除分组
        int result2 = dbConn.executeUpdate(
            "DELETE FROM friend_groups WHERE id = ? AND user_id = ?",
            {groupId, userId}
        );

        if (result2 == -1) {
            throw std::runtime_error("Failed to delete friend group");
        }

        if (!dbConn.commitTransaction()) {
            throw std::runtime_error("Failed to commit transaction");
        }

        // 好友分组已删除
        return true;

    } catch (const std::exception& e) {
        dbConn.rollbackTransaction();
        LOG_ERROR(QString("Failed to delete friend group %1 for user %2: %3")
                 .arg(groupId).arg(userId).arg(e.what()));
        return false;
    }
}

bool FriendService::renameFriendGroup(qint64 userId, qint64 groupId, const QString& newName)
{
    QMutexLocker locker(&_mutex);

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return false;
    }
    //     QSqlQuery query(db); - 需要手动处理查询

    int result = dbConn.executeUpdate(
        "UPDATE friend_groups SET group_name = ? WHERE id = ? AND user_id = ?",
        {newName, groupId, userId}
    );

    if (result == -1) {
        LOG_ERROR(QString("Failed to rename friend group %1 for user %2").arg(groupId).arg(userId));
        return false;
    }

    if (result == 0) {
        LOG_WARNING(QString("Friend group %1 not found for user %2").arg(groupId).arg(userId));
        return false;
    }

    // 好友分组已重命名
    return true;
}

bool FriendService::moveFriendToGroup(qint64 userId, qint64 friendId, qint64 groupId)
{
    QMutexLocker locker(&_mutex);

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return false;
    }
    //     QSqlQuery query(db); - 需要手动处理查询

    // 验证分组属于该用户
    QSqlQuery query = dbConn.executeQuery("SELECT id FROM friend_groups WHERE id = ? AND user_id = ?", {groupId, userId});
    if (query.lastError().isValid() || !query.next()) {
        LOG_ERROR(QString("Friend group %1 not found for user %2").arg(groupId).arg(userId));
        return false;
    }

    // 更新好友分组
    int result = dbConn.executeUpdate(
        "UPDATE friendships SET group_id = ? WHERE user_id = ? AND friend_id = ? AND status = 'accepted'",
        {groupId, userId, friendId}
    );

    if (result == -1) {
        LOG_ERROR(QString("Failed to move friend %1 to group %2 for user %3").arg(friendId).arg(groupId).arg(userId));
        return false;
    }

    if (result == 0) {
        LOG_WARNING(QString("Friendship not found: user %1, friend %2").arg(userId).arg(friendId));
        return false;
    }

    // 好友已移动到分组
    return true;
}
