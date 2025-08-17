#include "FriendService.h"
#include "OnlineStatusService.h"
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
        "SELECT id, status FROM friend_requests WHERE "
        "(requester_id = ? AND target_id = ?) OR "
        "(requester_id = ? AND target_id = ?)",
        {fromUserId, toUserId, toUserId, fromUserId}
    );
    
    if (existingRequestQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to check existing friend request: %1").arg(existingRequestQuery.lastError().text()));
        return DatabaseError;
    }
    
    if (existingRequestQuery.next()) {
        QString status = existingRequestQuery.value("status").toString();
        if (status == "pending") {
            return AlreadyRequested;
        } else if (status == "accepted") {
            return AlreadyFriends;
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
        if (status == "accepted") {
            return AlreadyFriends;
        } else if (status == "blocked") {
            return UserBlocked;
        }
    }
    
    try {
        // 创建好友请求记录
        QSqlQuery insertQuery = dbConn.executeQuery(
            "INSERT INTO friend_requests (requester_id, target_id, message, status, requested_at) VALUES (?, ?, ?, 'pending', NOW())",
            {fromUserId, toUserId, message}
        );
        
        if (insertQuery.lastError().isValid()) {
            LOG_ERROR(QString("Failed to create friend request: %1").arg(insertQuery.lastError().text()));
            return DatabaseError;
        }
        
        // 获取插入的请求ID
        qint64 requestId = insertQuery.lastInsertId().toLongLong();
        
        // 创建好友请求通知记录
        QSqlQuery notificationQuery = dbConn.executeQuery(
            "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_received', ?)",
            {requestId, toUserId, message}
        );
        
        if (notificationQuery.lastError().isValid()) {
            LOG_WARNING(QString("Failed to create friend request notification: %1").arg(notificationQuery.lastError().text()));
        }
        
        // 检查目标用户在线状态并发送通知
        OnlineStatusService* statusService = OnlineStatusService::instance();
        LOG_INFO(QString("Checking online status for user %1").arg(toUserId));
        if (statusService && statusService->isUserOnline(toUserId)) {
            // 用户在线，实时推送
            LOG_INFO(QString("User %1 is online, sending real-time friend request notification").arg(toUserId));
            
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
                    LOG_INFO(QString("Real-time friend request notification sent to user %1").arg(toUserId));
                } else {
                    LOG_WARNING(QString("Failed to send real-time friend request notification to user %1").arg(toUserId));
                }
            } else {
                LOG_ERROR("ThreadPoolServer instance not available for friend request notification");
            }
        } else {
            // 用户离线，存储到离线消息队列
            LOG_INFO(QString("User %1 is offline, storing friend request in offline queue").arg(toUserId));
            
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
    
    LOG_INFO(QString("=== 开始处理好友请求响应 ==="));
    LOG_INFO(QString("用户ID: %1, 请求ID: %2, 是否接受: %3, 备注: '%4', 分组: '%5'")
            .arg(userId).arg(requestId).arg(accept ? "是" : "否").arg(note).arg(groupName));
    
    // 添加详细的调试信息
    LOG_INFO(QString("=== 调试信息 ==="));
    LOG_INFO(QString("当前时间: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    LOG_INFO(QString("线程ID: %1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId())));
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend request response");
        return false;
    }
    
    // 验证好友请求
    QSqlQuery query = dbConn.executeQuery(
        "SELECT requester_id, target_id, status FROM friend_requests WHERE id = ? AND target_id = ? AND status = 'pending'", 
        {requestId, userId}
    );
    
    if (query.lastError().isValid() || !query.next()) {
        LOG_WARNING(QString("Invalid friend request: %1 for user %2").arg(requestId).arg(userId));
        return false;
    }
    
    qint64 requesterId = query.value("requester_id").toLongLong();
    LOG_INFO(QString("验证好友请求成功 - 请求者ID: %1, 目标ID: %2").arg(requesterId).arg(userId));
    
    try {
        if (accept) {
            LOG_INFO(QString("开始接受好友请求 - 调用AcceptFriendRequest存储过程"));
            
            // 调用存储过程处理接受好友请求
            QSqlQuery acceptQuery = dbConn.executeQuery(
                "CALL AcceptFriendRequest(?, ?, ?)",
                {requestId, note, groupName}
            );
            
            if (acceptQuery.lastError().isValid()) {
                LOG_ERROR(QString("AcceptFriendRequest存储过程执行失败: %1").arg(acceptQuery.lastError().text()));
                throw std::runtime_error(acceptQuery.lastError().text().toStdString());
            }
            
            LOG_INFO(QString("AcceptFriendRequest存储过程执行成功"));
            
            // 详细验证好友关系是否创建成功
            LOG_INFO(QString("=== 开始验证好友关系创建结果 ==="));
            LOG_INFO(QString("检查用户 %1 和 %2 之间的好友关系").arg(requesterId).arg(userId));
            
            // 检查请求者到接受者的关系
            QSqlQuery checkRequesterToTarget = dbConn.executeQuery(
                "SELECT id, status, accepted_at FROM friendships WHERE user_id = ? AND friend_id = ?",
                {requesterId, userId}
            );
            
            if (!checkRequesterToTarget.lastError().isValid()) {
                if (checkRequesterToTarget.next()) {
                    LOG_INFO(QString("找到请求者到接受者的关系: ID=%1, 状态=%2, 接受时间=%3")
                            .arg(checkRequesterToTarget.value("id").toLongLong())
                            .arg(checkRequesterToTarget.value("status").toString())
                            .arg(checkRequesterToTarget.value("accepted_at").toDateTime().toString(Qt::ISODate)));
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
                    LOG_INFO(QString("找到接受者到请求者的关系: ID=%1, 状态=%2, 接受时间=%3")
                            .arg(checkTargetToRequester.value("id").toLongLong())
                            .arg(checkTargetToRequester.value("status").toString())
                            .arg(checkTargetToRequester.value("accepted_at").toDateTime().toString(Qt::ISODate)));
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
                LOG_INFO(QString("验证好友关系创建结果: 找到 %1 条好友关系记录").arg(friendshipCount));
                
                if (friendshipCount == 0) {
                    LOG_ERROR("好友关系创建失败 - 没有找到任何好友关系记录");
                    // 手动创建好友关系作为备选方案
                    LOG_INFO("尝试手动创建好友关系...");
                    
                    // 为请求者创建好友关系
                    QSqlQuery createFriendship1 = dbConn.executeQuery(
                        "INSERT INTO friendships (user_id, friend_id, status, accepted_at) VALUES (?, ?, 'accepted', NOW())",
                        {requesterId, userId}
                    );
                    
                    if (createFriendship1.lastError().isValid()) {
                        LOG_ERROR(QString("创建请求者好友关系失败: %1").arg(createFriendship1.lastError().text()));
                    } else {
                        LOG_INFO("请求者好友关系创建成功");
                    }
                    
                    // 为接受者创建好友关系
                    QSqlQuery createFriendship2 = dbConn.executeQuery(
                        "INSERT INTO friendships (user_id, friend_id, status, accepted_at) VALUES (?, ?, 'accepted', NOW())",
                        {userId, requesterId}
                    );
                    
                    if (createFriendship2.lastError().isValid()) {
                        LOG_ERROR(QString("创建接受者好友关系失败: %1").arg(createFriendship2.lastError().text()));
                    } else {
                        LOG_INFO("接受者好友关系创建成功");
                    }
                }
            }
            
            LOG_INFO(QString("Friend request accepted: requestId=%1, userId=%2, note='%3', group='%4'")
                    .arg(requestId).arg(userId).arg(note).arg(groupName));
            
            // 发送信号
            emit friendRequestResponded(requestId, requesterId, userId, true);
            
            // 向发起者发送实时通知
            OnlineStatusService* statusService = OnlineStatusService::instance();
            LOG_INFO(QString("=== 检查用户在线状态 ==="));
            LOG_INFO(QString("当前时间: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            LOG_INFO(QString("发起者用户ID: %1").arg(requesterId));
            LOG_INFO(QString("接受者用户ID: %1").arg(userId));
            
            if (statusService) {
                bool requesterOnline = statusService->isUserOnline(requesterId);
                bool accepterOnline = statusService->isUserOnline(userId);
                
                LOG_INFO(QString("发起者在线状态: %1").arg(requesterOnline ? "在线" : "离线"));
                LOG_INFO(QString("接受者在线状态: %1").arg(accepterOnline ? "在线" : "离线"));
                
                if (requesterOnline) {
                    LOG_INFO(QString("User %1 is online, sending real-time friend acceptance notification").arg(requesterId));
                    
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
                            LOG_INFO(QString("Real-time friend acceptance notification sent to user %1").arg(requesterId));
                        } else {
                            LOG_WARNING(QString("Failed to send real-time friend acceptance notification to user %1").arg(requesterId));
                        }
                    } else {
                        LOG_ERROR("ThreadPoolServer instance not available for friend acceptance notification");
                    }
                } else {
                    LOG_INFO(QString("User %1 is offline, storing friend acceptance in offline queue").arg(requesterId));
                    if (!addToOfflineQueue(requesterId, requestId, 1)) {
                        LOG_ERROR(QString("Failed to add friend acceptance to offline queue for user %1").arg(requesterId));
                    }
                }
            } else {
                LOG_ERROR("OnlineStatusService实例不可用");
            }
            
            // 发送好友列表更新通知
            LOG_INFO("=== 发送好友列表更新通知 ===");
            LOG_INFO(QString("当前时间: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            LOG_INFO(QString("发起者用户ID: %1").arg(requesterId));
            LOG_INFO(QString("接受者用户ID: %1").arg(userId));
            
            ThreadPoolServer* server = ThreadPoolServer::instance();
            
            if (server && statusService) {
                // 向发起者发送好友列表更新通知（仅在线用户）
                LOG_INFO(QString("检查发起者 %1 在线状态...").arg(requesterId));
                bool requesterOnline = statusService->isUserOnline(requesterId);
                LOG_INFO(QString("发起者在线状态: %1").arg(requesterOnline ? "在线" : "离线"));
                
                if (requesterOnline) {
                    LOG_INFO(QString("向发起者 %1 发送好友列表更新通知").arg(requesterId));
                    QJsonObject notification;
                    notification["notification_type"] = "friend_list_update";
                    notification["message"] = "Your friend list has been updated";
                    notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                    
                    bool sent = server->sendMessageToUser(requesterId, notification);
                    LOG_INFO(QString("向发起者 %1 发送通知结果: %2").arg(requesterId).arg(sent ? "成功" : "失败"));
                } else {
                    LOG_INFO(QString("发起者 %1 离线，跳过好友列表更新通知").arg(requesterId));
                }
                
                // 向接受者发送好友列表更新通知（仅在线用户）
                LOG_INFO(QString("检查接受者 %1 在线状态...").arg(userId));
                bool accepterOnline = statusService->isUserOnline(userId);
                LOG_INFO(QString("接受者在线状态: %1").arg(accepterOnline ? "在线" : "离线"));
                
                if (accepterOnline) {
                    LOG_INFO(QString("向接受者 %1 发送好友列表更新通知").arg(userId));
                    QJsonObject notification;
                    notification["notification_type"] = "friend_list_update";
                    notification["message"] = "Your friend list has been updated";
                    notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                    
                    bool sent = server->sendMessageToUser(userId, notification);
                    LOG_INFO(QString("向接受者 %1 发送通知结果: %2").arg(userId).arg(sent ? "成功" : "失败"));
                } else {
                    LOG_INFO(QString("接受者 %1 离线，跳过好友列表更新通知").arg(userId));
                }
            } else {
                LOG_ERROR("ThreadPoolServer或OnlineStatusService不可用，无法发送通知");
            }
            
            LOG_INFO(QString("=== 好友请求接受处理完成 ==="));
            
        } else {
            // 调用存储过程处理拒绝好友请求
            QSqlQuery rejectQuery = dbConn.executeQuery(
                "CALL RejectFriendRequest(?)",
                {requestId}
            );
            
            if (rejectQuery.lastError().isValid()) {
                throw std::runtime_error(rejectQuery.lastError().text().toStdString());
            }
            
            LOG_INFO(QString("Friend request rejected: requestId=%1, userId=%2").arg(requestId).arg(userId));
            
            // 确保请求状态已更新
            QSqlQuery verifyQuery = dbConn.executeQuery(
                "SELECT status FROM friend_requests WHERE id = ?",
                {requestId}
            );
            
            if (!verifyQuery.lastError().isValid() && verifyQuery.next()) {
                QString status = verifyQuery.value("status").toString();
                LOG_INFO(QString("Verified friend request status after rejection: %1").arg(status));
            }
            
            // 发送信号
            emit friendRequestResponded(requestId, requesterId, userId, false);
            
            // 向发起者发送实时通知
            OnlineStatusService* statusService = OnlineStatusService::instance();
            if (statusService && statusService->isUserOnline(requesterId)) {
                LOG_INFO(QString("User %1 is online, sending real-time friend rejection notification").arg(requesterId));
                
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
                        LOG_INFO(QString("Real-time friend rejection notification sent to user %1").arg(requesterId));
                    } else {
                        LOG_WARNING(QString("Failed to send real-time friend rejection notification to user %1").arg(requesterId));
                    }
                } else {
                    LOG_ERROR("ThreadPoolServer instance not available for friend rejection notification");
                }
            } else {
                LOG_INFO(QString("User %1 is offline, storing friend rejection in offline queue").arg(requesterId));
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
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend request ignore");
        return false;
    }
    
    // 验证好友请求
    QSqlQuery query = dbConn.executeQuery(
        "SELECT requester_id, target_id, status FROM friend_requests WHERE id = ? AND target_id = ? AND status = 'pending'", 
        {requestId, userId}
    );
    
    if (query.lastError().isValid() || !query.next()) {
        LOG_WARNING(QString("Invalid friend request for ignore: %1 for user %2").arg(requestId).arg(userId));
        return false;
    }
    
    qint64 requesterId = query.value("requester_id").toLongLong();
    
    try {
        // 调用存储过程处理忽略好友请求
        QSqlQuery ignoreQuery = dbConn.executeQuery(
            "CALL IgnoreFriendRequest(?)",
            {requestId}
        );
        
        if (ignoreQuery.lastError().isValid()) {
            throw std::runtime_error(ignoreQuery.lastError().text().toStdString());
        }
        
        LOG_INFO(QString("Friend request ignored: requestId=%1, userId=%2").arg(requestId).arg(userId));
        
        // 确保请求状态已更新
        QSqlQuery verifyQuery = dbConn.executeQuery(
            "SELECT status FROM friend_requests WHERE id = ?",
            {requestId}
        );
        
        if (!verifyQuery.lastError().isValid() && verifyQuery.next()) {
            QString status = verifyQuery.value("status").toString();
            LOG_INFO(QString("Verified friend request status after ignore: %1").arg(status));
        }
        
        // 创建好友请求忽略通知
        int notificationResult = dbConn.executeUpdate(
            "INSERT INTO friend_request_notifications (request_id, user_id, notification_type, message) VALUES (?, ?, 'request_ignored', ?)",
            {requestId, requesterId, QString("您的好友请求已被忽略")}
        );
        
        if (notificationResult == -1) {
            LOG_WARNING(QString("Failed to create friend request ignored notification for user %1").arg(requesterId));
        }
        
        // 发送信号
        emit friendRequestResponded(requestId, requesterId, userId, false);
        
        // 检查发起者是否在线，如果离线则存储到离线队列
        OnlineStatusService* statusService = OnlineStatusService::instance();
        if (statusService && !statusService->isUserOnline(requesterId)) {
            LOG_INFO(QString("User %1 is offline, storing friend ignore notification in offline queue").arg(requesterId));
            if (!addToOfflineQueue(requesterId, requestId, 3)) { // 低优先级
                LOG_ERROR(QString("Failed to add friend ignore notification to offline queue for user %1").arg(requesterId));
            }
        } else {
            // 用户在线，立即发送通知
            ThreadPoolServer* server = ThreadPoolServer::instance();
            if (server) {
                QJsonObject notification;
                notification["action"] = "friend_request_notification";
                notification["notification_type"] = "request_ignored";
                notification["request_id"] = requestId;
                notification["message"] = "您的好友请求已被忽略";
                notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                
                server->sendMessageToUser(requesterId, notification);
                LOG_INFO(QString("Friend request ignored notification sent to online user %1").arg(requesterId));
            }
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
    
    LOG_INFO(QString("=== 开始获取好友列表 ==="));
    LOG_INFO(QString("用户ID: %1").arg(userId));
    LOG_INFO(QString("当前时间: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    LOG_INFO(QString("线程ID: %1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId())));
    
    QJsonArray friendList;
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend list");
        return friendList;
    }
    
    LOG_INFO("数据库连接获取成功，开始查询...");
    
    // 首先检查friendships表中是否有数据
    LOG_INFO("=== 检查数据库状态 ===");
    QSqlQuery checkQuery = dbConn.executeQuery(
        "SELECT COUNT(*) as total_count FROM friendships WHERE status = 'accepted'"
    );
    
    if (!checkQuery.lastError().isValid() && checkQuery.next()) {
        int totalCount = checkQuery.value("total_count").toInt();
        LOG_INFO(QString("friendships表中总共有 %1 条已接受的好友关系记录").arg(totalCount));
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
        LOG_INFO(QString("用户 %1 有 %2 条好友关系记录").arg(userId).arg(userCount));
    } else {
        LOG_ERROR(QString("查询用户 %1 好友关系数量失败: %2").arg(userId).arg(userCheckQuery.lastError().text()));
    }
    
    // 详细检查当前用户的所有friendships记录
    LOG_INFO(QString("=== 详细检查用户 %1 的所有friendships记录 ===").arg(userId));
    QSqlQuery detailedQuery = dbConn.executeQuery(
        "SELECT id, user_id, friend_id, status, accepted_at, note, group_id FROM friendships WHERE user_id = ?",
        {userId}
    );
    
    if (!detailedQuery.lastError().isValid()) {
        int recordCount = 0;
        while (detailedQuery.next()) {
            recordCount++;
            LOG_INFO(QString("记录 %1: ID=%2, 好友ID=%3, 状态=%4, 接受时间=%5, 备注='%6', 分组ID=%7")
                    .arg(recordCount)
                    .arg(detailedQuery.value("id").toLongLong())
                    .arg(detailedQuery.value("friend_id").toLongLong())
                    .arg(detailedQuery.value("status").toString())
                    .arg(detailedQuery.value("accepted_at").toDateTime().toString(Qt::ISODate))
                    .arg(detailedQuery.value("note").toString())
                    .arg(detailedQuery.value("group_id").toLongLong()));
        }
        LOG_INFO(QString("用户 %1 总共有 %2 条friendships记录").arg(userId).arg(recordCount));
    } else {
        LOG_ERROR(QString("查询用户 %1 详细friendships记录失败: %2").arg(userId).arg(detailedQuery.lastError().text()));
    }
    
    // 查询好友列表（关联分组信息）
    LOG_INFO(QString("执行好友列表查询SQL..."));
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
        
        LOG_INFO(QString("找到好友: %1 (ID: %2, 分组: %3)").arg(friendInfo["username"].toString()).arg(friendInfo["friend_id"].toVariant().toLongLong()).arg(friendInfo["group_name"].toString()));
    }
    
    LOG_INFO(QString("=== 好友列表获取完成 ==="));
    LOG_INFO(QString("返回 %1 个好友给用户 %2").arg(friendList.size()).arg(userId));
    
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
    
    LOG_INFO(QString("Getting pending friend requests for user %1").arg(userId));
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for pending friend requests");
        return requestList;
    }
    
    // 查询待处理的好友请求（作为接收者）
    QSqlQuery pendingQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.message, fr.requested_at, "
        "u.username as requester_username, u.display_name as requester_display_name, u.avatar_url as requester_avatar_url "
        "FROM friend_requests fr "
        "JOIN users u ON fr.requester_id = u.id "
        "WHERE fr.target_id = ? AND fr.status = 'pending' "
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

            requestList.append(requestInfo);
            
            LOG_INFO(QString("Found pending friend request: from %1 to %2, request_id: %3")
                    .arg(pendingQuery.value("requester_username").toString())
                    .arg(userId)
                    .arg(pendingQuery.value("request_id").toLongLong()));
        }
    }

    // 查询已处理的好友请求（作为发送者）
    QSqlQuery processedQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.status, fr.requested_at, fr.responded_at, "
        "fr.response_note, fr.response_group_id, "
        "u.username as target_username, u.display_name as target_display_name, u.avatar_url as target_avatar_url, "
        "fg.group_name as response_group_name "
        "FROM friend_requests fr "
        "JOIN users u ON fr.target_id = u.id "
        "LEFT JOIN friend_groups fg ON fr.response_group_id = fg.id "
        "WHERE fr.requester_id = ? AND fr.status IN ('accepted', 'rejected') "
        "ORDER BY fr.responded_at DESC",
        {userId}
    );
    
    if (processedQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get processed friend requests for user %1: %2").arg(userId).arg(processedQuery.lastError().text()));
    } else {
        // 处理已响应的请求（作为发送者）
        while (processedQuery.next()) {
            QJsonObject requestInfo;
            requestInfo["request_id"] = processedQuery.value("request_id").toLongLong();
            requestInfo["friendship_id"] = processedQuery.value("request_id").toLongLong(); // 兼容性
            requestInfo["requester_id"] = processedQuery.value("target_id").toLongLong(); // 显示目标用户信息
            requestInfo["requester_username"] = processedQuery.value("target_username").toString();
            requestInfo["requester_display_name"] = processedQuery.value("target_display_name").toString();
            requestInfo["requester_avatar_url"] = processedQuery.value("target_avatar_url").toString();
            requestInfo["requested_at"] = processedQuery.value("responded_at").toDateTime().toString(Qt::ISODate);
            requestInfo["message"] = processedQuery.value("response_note").toString();
            requestInfo["status"] = processedQuery.value("status").toString(); // accepted 或 rejected

            requestList.append(requestInfo);
            
            LOG_INFO(QString("Found processed friend request: from %1 to %2, request_id: %3, status: %4")
                    .arg(processedQuery.value("target_username").toString())
                    .arg(userId)
                    .arg(processedQuery.value("request_id").toLongLong())
                    .arg(processedQuery.value("status").toString()));
        }
    }

    // 查询已处理的好友请求（作为接收者）
    QSqlQuery receivedProcessedQuery = dbConn.executeQuery(
        "SELECT fr.id as request_id, fr.requester_id, fr.target_id, fr.status, fr.requested_at, fr.responded_at, "
        "fr.response_note, fr.response_group_id, "
        "u.username as requester_username, u.display_name as requester_display_name, u.avatar_url as requester_avatar_url, "
        "fg.group_name as response_group_name "
        "FROM friend_requests fr "
        "JOIN users u ON fr.requester_id = u.id "
        "LEFT JOIN friend_groups fg ON fr.response_group_id = fg.id "
        "WHERE fr.target_id = ? AND fr.status IN ('accepted', 'rejected') "
        "ORDER BY fr.responded_at DESC",
        {userId}
    );
    
    if (receivedProcessedQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get received processed friend requests for user %1: %2").arg(userId).arg(receivedProcessedQuery.lastError().text()));
    } else {
        // 处理已响应的请求（作为接收者）
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
            requestInfo["status"] = receivedProcessedQuery.value("status").toString(); // accepted 或 rejected

            requestList.append(requestInfo);
            
            LOG_INFO(QString("Found received processed friend request: from %1 to %2, request_id: %3, status: %4")
                    .arg(receivedProcessedQuery.value("requester_username").toString())
                    .arg(userId)
                    .arg(receivedProcessedQuery.value("request_id").toLongLong())
                    .arg(receivedProcessedQuery.value("status").toString()));
        }
    }

    LOG_INFO(QString("Found %1 total friend requests for user %2").arg(requestList.size()).arg(userId));
    
    // 获取待处理好友请求成功
    return requestList;
}

bool FriendService::deleteFriendRequestNotification(qint64 userId, qint64 requestId)
{
    QMutexLocker locker(&_mutex);
    
    LOG_INFO(QString("Deleting friend request notification: userId=%1, requestId=%2").arg(userId).arg(requestId));
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for deleting friend request notification");
        return false;
    }
    
    // 删除好友请求通知记录
    QSqlQuery deleteQuery = dbConn.executeQuery(
        "DELETE FROM friend_request_notifications WHERE user_id = ? AND request_id = ?",
        {userId, requestId}
    );
    
    if (deleteQuery.lastError().isValid()) {
        LOG_ERROR(QString("Failed to delete friend request notification: %1").arg(deleteQuery.lastError().text()));
        return false;
    }
    
    int affectedRows = deleteQuery.numRowsAffected();
    LOG_INFO(QString("Deleted %1 friend request notification records").arg(affectedRows));
    
    return affectedRows > 0;
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

        // 提交事务
        if (!dbConn.commitTransaction()) {
            throw std::runtime_error("Failed to commit remove friend transaction");
        }

        // 好友关系已删除

        // 发送信号
        emit friendRemoved(userId, friendId);

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
