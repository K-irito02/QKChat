#include "FriendService.h"
#include "../database/DatabaseManager.h"
#include "../database/DatabaseConnectionPool.h"
#include "../cache/CacheManager.h"
#include "../rate_limit/RateLimitManager.h"
#include <QSqlRecord>
#include <QVariant>
#include <QUuid>
#include <QCryptographicHash>

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
    
    QSqlQuery query = dbConn.executeQuery(
        "SELECT id, status FROM friendships WHERE "
        "(user_id = ? AND friend_id = ?) OR "
        "(user_id = ? AND friend_id = ?)",
        {fromUserId, toUserId, toUserId, fromUserId}
    );
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to check existing friendship: %1").arg(query.lastError().text()));
        return DatabaseError;
    }
    
    if (query.next()) {
        QString status = query.value("status").toString();
        if (status == "accepted") {
            return AlreadyFriends;
        } else if (status == "pending") {
            return AlreadyRequested;
        } else if (status == "blocked") {
            return UserBlocked;
        }
    }
    
    // 开始事务
    if (!dbConn.beginTransaction()) {
        LOG_ERROR("Failed to start transaction for friend request");
        return DatabaseError;
    }
    
    try {
        // 获取分组ID（如果指定了分组名称）
        qint64 groupId = 0;
        if (!groupName.isEmpty()) {
            QSqlQuery groupQuery = dbConn.executeQuery(
                "SELECT id FROM friend_groups WHERE user_id = ? AND group_name = ?",
                {fromUserId, groupName}
            );
            
            if (!groupQuery.lastError().isValid() && groupQuery.next()) {
                groupId = groupQuery.value("id").toLongLong();
            } else {
                // 如果分组不存在，创建新分组
                QSqlQuery maxOrderQuery = dbConn.executeQuery(
                    "SELECT COALESCE(MAX(group_order), 0) + 1 as next_order FROM friend_groups WHERE user_id = ?",
                    {fromUserId}
                );
                
                int nextOrder = 1;
                if (!maxOrderQuery.lastError().isValid() && maxOrderQuery.next()) {
                    nextOrder = maxOrderQuery.value("next_order").toInt();
                }
                
                QSqlQuery insertGroupQuery = dbConn.executeQuery(
                    "INSERT INTO friend_groups (user_id, group_name, group_order) VALUES (?, ?, ?)",
                    {fromUserId, groupName, nextOrder}
                );
                
                if (!insertGroupQuery.lastError().isValid()) {
                    groupId = insertGroupQuery.lastInsertId().toLongLong();
                }
            }
        }

        // 创建好友关系记录
        QSqlQuery insertQuery = dbConn.executeQuery(
            "INSERT INTO friendships (user_id, friend_id, status, requested_at, note) VALUES (?, ?, 'pending', NOW(), ?)",
            {fromUserId, toUserId, remark}
        );
        
        if (insertQuery.lastError().isValid()) {
            LOG_ERROR(QString("Failed to create friendship request: %1").arg(insertQuery.lastError().text()));
            dbConn.rollbackTransaction();
            return DatabaseError;
        }
        
        // 提交事务
        if (!dbConn.commitTransaction()) {
            LOG_ERROR("Failed to commit friend request transaction");
            dbConn.rollbackTransaction();
            return DatabaseError;
        }
        
        // 好友请求已发送
        return Success;
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during friend request: %1").arg(e.what()));
        dbConn.rollbackTransaction();
        return DatabaseError;
    }
}

bool FriendService::respondToFriendRequest(qint64 userId, qint64 friendshipId, bool accept)
{
    QMutexLocker locker(&_mutex);
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for friend request response");
        return false;
    }
    
    // 验证好友请求
    QSqlQuery query = dbConn.executeQuery("SELECT user_id, friend_id, status FROM friendships WHERE id = ? AND friend_id = ?", 
                                         {friendshipId, userId});
    
    if (query.lastError().isValid() || !query.next()) {
        LOG_WARNING(QString("Invalid friend request: %1 for user %2").arg(friendshipId).arg(userId));
        return false;
    }
    
    QString currentStatus = query.value("status").toString();
    if (currentStatus != "pending") {
        LOG_WARNING(QString("Friend request %1 is not pending: %2").arg(friendshipId).arg(currentStatus));
        return false;
    }
    
    qint64 fromUserId = query.value("user_id").toLongLong();
    
    // 开始事务
    if (!dbConn.beginTransaction()) {
        LOG_ERROR("Failed to start transaction for friend request response");
        return false;
    }
    
    try {
        if (accept) {
            // 接受好友请求
            QSqlQuery updateQuery = dbConn.executeQuery(
                "UPDATE friendships SET status = 'accepted', accepted_at = NOW() WHERE id = ?",
                {friendshipId}
            );
            
            if (updateQuery.lastError().isValid()) {
                throw std::runtime_error(updateQuery.lastError().text().toStdString());
            }
            
            // 创建反向好友关系（双向好友关系）
            QSqlQuery insertQuery = dbConn.executeQuery(
                "INSERT INTO friendships (user_id, friend_id, status, requested_at, accepted_at) "
                "VALUES (?, ?, 'accepted', NOW(), NOW())",
                {userId, fromUserId}
            );
            
            if (insertQuery.lastError().isValid()) {
                throw std::runtime_error(insertQuery.lastError().text().toStdString());
            }
            
            // 好友请求已接受
        } else {
            // 拒绝好友请求
            QSqlQuery deleteQuery = dbConn.executeQuery(
                "UPDATE friendships SET status = 'deleted' WHERE id = ?",
                {friendshipId}
            );
            
            if (deleteQuery.lastError().isValid()) {
                throw std::runtime_error(deleteQuery.lastError().text().toStdString());
            }
            
            // 好友请求已拒绝
        }
        
        // 创建响应通知
        QString notificationType = accept ? "accepted" : "rejected";
        if (!createFriendNotification(friendshipId, userId, fromUserId, notificationType)) {
            throw std::runtime_error("Failed to create friend response notification");
        }
        
        // 提交事务
        if (!dbConn.commitTransaction()) {
            throw std::runtime_error("Failed to commit friend request response transaction");
        }
        
        // 发送信号
        emit friendRequestResponded(friendshipId, fromUserId, userId, accept);
        
        return true;
        
    } catch (const std::exception& e) {
        dbConn.rollbackTransaction();
        LOG_ERROR(QString("Failed to respond to friend request: %1").arg(e.what()));
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
    
    // 查询好友列表（使用视图获取详细信息）
    QSqlQuery query = dbConn.executeQuery("SELECT * FROM friend_list WHERE user_id = ? ORDER BY friend_online_status DESC, friend_last_seen DESC",
                                         {userId});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get friend list for user %1: %2").arg(userId).arg(query.lastError().text()));
        return friendList;
    }
    
    while (query.next()) {
        QJsonObject friendInfo;
        friendInfo["friendship_id"] = query.value("friendship_id").toLongLong();
        friendInfo["friend_id"] = query.value("friend_id").toLongLong();
        friendInfo["friend_user_id"] = query.value("friend_user_id").toString();
        friendInfo["username"] = query.value("friend_username").toString();
        friendInfo["display_name"] = query.value("friend_display_name").toString();
        friendInfo["avatar_url"] = query.value("friend_avatar_url").toString();
        friendInfo["online_status"] = query.value("friend_online_status").toString();
        friendInfo["last_seen"] = query.value("friend_last_seen").toDateTime().toString(Qt::ISODate);
        friendInfo["note"] = query.value("note").toString();
        friendInfo["accepted_at"] = query.value("accepted_at").toDateTime().toString(Qt::ISODate);
        
        friendList.append(friendInfo);
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
    
    // 查询待处理的好友请求
    QSqlQuery query = dbConn.executeQuery("SELECT * FROM pending_friend_requests WHERE target_id = ? ORDER BY requested_at DESC",
                                         {userId});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get pending friend requests for user %1: %2").arg(userId).arg(query.lastError().text()));
        return requestList;
    }

    while (query.next()) {
        QJsonObject requestInfo;
        requestInfo["friendship_id"] = query.value("friendship_id").toLongLong();
        requestInfo["requester_id"] = query.value("requester_id").toLongLong();
        requestInfo["requester_username"] = query.value("requester_username").toString();
        requestInfo["requester_display_name"] = query.value("requester_display_name").toString();
        requestInfo["requester_avatar_url"] = query.value("requester_avatar_url").toString();
        requestInfo["requested_at"] = query.value("requested_at").toDateTime().toString(Qt::ISODate);
        requestInfo["message"] = query.value("request_message").toString();
        requestInfo["is_read"] = query.value("notification_read").toBool();

        requestList.append(requestInfo);
    }

    // 获取待处理好友请求成功
    return requestList;
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

bool FriendService::createFriendNotification(qint64 friendshipId, qint64 fromUserId, qint64 toUserId,
                                           const QString& type, const QString& message)
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return false;
    }
    int result = dbConn.executeUpdate(
        "INSERT INTO friend_request_notifications "
        "(friendship_id, from_user_id, to_user_id, notification_type, message) "
        "VALUES (?, ?, ?, ?, ?)",
        {friendshipId, fromUserId, toUserId, type, message}
    );

    if (result == -1) {
        LOG_ERROR("Failed to create friend notification");
        return false;
    }

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

    QJsonArray groupList;
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for getting friend groups");
        return groupList;
    }
    
    QSqlQuery query = dbConn.executeQuery(
        "SELECT id, group_name, group_order, friend_count "
        "FROM user_friend_groups WHERE user_id = ? "
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
