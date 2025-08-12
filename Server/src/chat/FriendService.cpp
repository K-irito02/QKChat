#include "FriendService.h"
#include "../database/DatabaseManager.h"
#include <QSqlRecord>
#include <QVariant>
#include <QUuid>

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
    
    // 测试数据库连接
    QSqlDatabase db = getDatabase();
    if (!db.isValid() || !db.isOpen()) {
        LOG_ERROR("Failed to initialize FriendService: database not available");
        return false;
    }
    
    _initialized = true;
    LOG_INFO("FriendService initialized successfully");
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
    
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    
    // 检查现有关系
    query.prepare("SELECT id, status FROM friendships WHERE "
                 "(user_id = :user1 AND friend_id = :user2) OR "
                 "(user_id = :user2 AND friend_id = :user1)");
    query.bindValue(":user1", fromUserId);
    query.bindValue(":user2", toUserId);
    
    if (!query.exec()) {
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
    if (!db.transaction()) {
        LOG_ERROR("Failed to start transaction for friend request");
        return DatabaseError;
    }
    
    try {
        // 获取分组ID（如果指定了分组名称）
        qint64 groupId = 0;
        if (!groupName.isEmpty()) {
            QSqlQuery groupQuery(db);
            groupQuery.prepare("SELECT id FROM friend_groups WHERE user_id = :user_id AND group_name = :group_name");
            groupQuery.bindValue(":user_id", fromUserId);
            groupQuery.bindValue(":group_name", groupName);

            if (groupQuery.exec() && groupQuery.next()) {
                groupId = groupQuery.value("id").toLongLong();
            } else {
                // 如果分组不存在，创建新分组
                groupQuery.prepare("INSERT INTO friend_groups (user_id, group_name, group_order) "
                                  "VALUES (:user_id, :group_name, "
                                  "(SELECT COALESCE(MAX(group_order), 0) + 1 FROM friend_groups WHERE user_id = :user_id2))");
                groupQuery.bindValue(":user_id", fromUserId);
                groupQuery.bindValue(":group_name", groupName);
                groupQuery.bindValue(":user_id2", fromUserId);

                if (groupQuery.exec()) {
                    groupId = groupQuery.lastInsertId().toLongLong();
                }
            }
        }

        // 创建好友关系记录
        QString insertSql = "INSERT INTO friendships (user_id, friend_id, status, requested_at";
        QString valuesSql = "VALUES (:from_user, :to_user, 'pending', NOW()";

        if (!remark.isEmpty()) {
            insertSql += ", note";
            valuesSql += ", :note";
        }

        if (groupId > 0) {
            insertSql += ", group_id";
            valuesSql += ", :group_id";
        }

        insertSql += ") " + valuesSql + ")";

        query.prepare(insertSql);
        query.bindValue(":from_user", fromUserId);
        query.bindValue(":to_user", toUserId);

        if (!remark.isEmpty()) {
            query.bindValue(":note", remark);
        }

        if (groupId > 0) {
            query.bindValue(":group_id", groupId);
        }
        
        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }
        
        qint64 friendshipId = query.lastInsertId().toLongLong();
        
        // 创建通知记录
        if (!createFriendNotification(friendshipId, fromUserId, toUserId, "request", message)) {
            throw std::runtime_error("Failed to create friend request notification");
        }
        
        // 提交事务
        if (!db.commit()) {
            throw std::runtime_error("Failed to commit friend request transaction");
        }
        
        LOG_INFO(QString("Friend request sent: from user %1 to user %2").arg(fromUserId).arg(toUserId));
        
        // 发送信号
        emit friendRequestSent(fromUserId, toUserId, friendshipId, message);
        
        return Success;
        
    } catch (const std::exception& e) {
        db.rollback();
        LOG_ERROR(QString("Failed to send friend request: %1").arg(e.what()));
        return DatabaseError;
    }
}

bool FriendService::respondToFriendRequest(qint64 userId, qint64 friendshipId, bool accept)
{
    QMutexLocker locker(&_mutex);
    
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    
    // 验证好友请求
    query.prepare("SELECT user_id, friend_id, status FROM friendships WHERE id = :id AND friend_id = :user_id");
    query.bindValue(":id", friendshipId);
    query.bindValue(":user_id", userId);
    
    if (!query.exec() || !query.next()) {
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
    if (!db.transaction()) {
        LOG_ERROR("Failed to start transaction for friend request response");
        return false;
    }
    
    try {
        if (accept) {
            // 接受好友请求
            query.prepare("UPDATE friendships SET status = 'accepted', accepted_at = NOW() WHERE id = :id");
            query.bindValue(":id", friendshipId);
            
            if (!query.exec()) {
                throw std::runtime_error(query.lastError().text().toStdString());
            }
            
            // 创建反向好友关系（双向好友关系）
            query.prepare("INSERT INTO friendships (user_id, friend_id, status, requested_at, accepted_at) "
                         "VALUES (:user_id, :friend_id, 'accepted', NOW(), NOW())");
            query.bindValue(":user_id", userId);
            query.bindValue(":friend_id", fromUserId);
            
            if (!query.exec()) {
                throw std::runtime_error(query.lastError().text().toStdString());
            }
            
            LOG_INFO(QString("Friend request accepted: %1 between users %2 and %3").arg(friendshipId).arg(fromUserId).arg(userId));
        } else {
            // 拒绝好友请求
            query.prepare("UPDATE friendships SET status = 'deleted' WHERE id = :id");
            query.bindValue(":id", friendshipId);
            
            if (!query.exec()) {
                throw std::runtime_error(query.lastError().text().toStdString());
            }
            
            LOG_INFO(QString("Friend request rejected: %1 from user %2 to user %3").arg(friendshipId).arg(fromUserId).arg(userId));
        }
        
        // 创建响应通知
        QString notificationType = accept ? "accepted" : "rejected";
        if (!createFriendNotification(friendshipId, userId, fromUserId, notificationType)) {
            throw std::runtime_error("Failed to create friend response notification");
        }
        
        // 提交事务
        if (!db.commit()) {
            throw std::runtime_error("Failed to commit friend request response transaction");
        }
        
        // 发送信号
        emit friendRequestResponded(friendshipId, fromUserId, userId, accept);
        
        return true;
        
    } catch (const std::exception& e) {
        db.rollback();
        LOG_ERROR(QString("Failed to respond to friend request: %1").arg(e.what()));
        return false;
    }
}

QJsonArray FriendService::getFriendList(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    QJsonArray friendList;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    
    // 使用视图查询好友列表
    query.prepare("SELECT * FROM friend_list WHERE user_id = :user_id ORDER BY friend_online_status DESC, friend_last_seen DESC");
    query.bindValue(":user_id", userId);
    
    if (!query.exec()) {
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
    
    LOG_INFO(QString("Retrieved %1 friends for user %2").arg(friendList.size()).arg(userId));
    return friendList;
}

QJsonArray FriendService::getPendingFriendRequests(qint64 userId)
{
    QMutexLocker locker(&_mutex);

    QJsonArray requestList;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 使用视图查询待处理的好友请求
    query.prepare("SELECT * FROM pending_friend_requests WHERE target_id = :user_id ORDER BY requested_at DESC");
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
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

    LOG_INFO(QString("Retrieved %1 pending friend requests for user %2").arg(requestList.size()).arg(userId));
    return requestList;
}

bool FriendService::removeFriend(qint64 userId, qint64 friendId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 开始事务
    if (!db.transaction()) {
        LOG_ERROR("Failed to start transaction for removing friend");
        return false;
    }

    try {
        // 删除双向好友关系
        query.prepare("UPDATE friendships SET status = 'deleted' WHERE "
                     "(user_id = :user1 AND friend_id = :user2) OR "
                     "(user_id = :user2 AND friend_id = :user1)");
        query.bindValue(":user1", userId);
        query.bindValue(":user2", friendId);

        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }

        // 提交事务
        if (!db.commit()) {
            throw std::runtime_error("Failed to commit remove friend transaction");
        }

        LOG_INFO(QString("Friend relationship removed between users %1 and %2").arg(userId).arg(friendId));

        // 发送信号
        emit friendRemoved(userId, friendId);

        return true;

    } catch (const std::exception& e) {
        db.rollback();
        LOG_ERROR(QString("Failed to remove friend: %1").arg(e.what()));
        return false;
    }
}

bool FriendService::blockUser(qint64 userId, qint64 targetUserId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 检查现有关系
    query.prepare("SELECT id, status FROM friendships WHERE user_id = :user_id AND friend_id = :target_id");
    query.bindValue(":user_id", userId);
    query.bindValue(":target_id", targetUserId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to check existing relationship for blocking: %1").arg(query.lastError().text()));
        return false;
    }

    if (query.next()) {
        // 更新现有关系为屏蔽
        qint64 friendshipId = query.value("id").toLongLong();
        query.prepare("UPDATE friendships SET status = 'blocked', blocked_at = NOW() WHERE id = :id");
        query.bindValue(":id", friendshipId);
    } else {
        // 创建新的屏蔽关系
        query.prepare("INSERT INTO friendships (user_id, friend_id, status, blocked_at) "
                     "VALUES (:user_id, :target_id, 'blocked', NOW())");
        query.bindValue(":user_id", userId);
        query.bindValue(":target_id", targetUserId);
    }

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to block user %1 by user %2: %3").arg(targetUserId).arg(userId).arg(query.lastError().text()));
        return false;
    }

    LOG_INFO(QString("User %1 blocked user %2").arg(userId).arg(targetUserId));
    return true;
}

bool FriendService::unblockUser(qint64 userId, qint64 targetUserId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("UPDATE friendships SET status = 'deleted' WHERE user_id = :user_id AND friend_id = :target_id AND status = 'blocked'");
    query.bindValue(":user_id", userId);
    query.bindValue(":target_id", targetUserId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to unblock user %1 by user %2: %3").arg(targetUserId).arg(userId).arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() > 0) {
        LOG_INFO(QString("User %1 unblocked user %2").arg(userId).arg(targetUserId));
        return true;
    }

    LOG_WARNING(QString("No blocked relationship found between users %1 and %2").arg(userId).arg(targetUserId));
    return false;
}

bool FriendService::areFriends(qint64 userId1, qint64 userId2)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT COUNT(*) FROM friendships WHERE "
                 "((user_id = :user1 AND friend_id = :user2) OR "
                 "(user_id = :user2 AND friend_id = :user1)) AND status = 'accepted'");
    query.bindValue(":user1", userId1);
    query.bindValue(":user2", userId2);

    if (!query.exec() || !query.next()) {
        LOG_ERROR(QString("Failed to check friendship between users %1 and %2").arg(userId1).arg(userId2));
        return false;
    }

    return query.value(0).toInt() > 0;
}

FriendService::FriendshipStatus FriendService::getFriendshipStatus(qint64 userId1, qint64 userId2)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT status FROM friendships WHERE "
                 "(user_id = :user1 AND friend_id = :user2) OR "
                 "(user_id = :user2 AND friend_id = :user1) "
                 "ORDER BY requested_at DESC LIMIT 1");
    query.bindValue(":user1", userId1);
    query.bindValue(":user2", userId2);

    if (!query.exec() || !query.next()) {
        return Deleted; // 没有关系记录
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

    LOG_INFO(QString("Searching users in database with keyword: '%1', currentUserId: %2, limit: %3").arg(keyword).arg(currentUserId).arg(limit));

    QJsonArray userList;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 搜索用户（排除当前用户）
    query.prepare("SELECT id, user_id, username, display_name, avatar_url, status "
                 "FROM users WHERE "
                 "(username LIKE :keyword OR email LIKE :keyword OR display_name LIKE :keyword OR user_id = :exact_id) "
                 "AND id != :current_user AND status = 'active' "
                 "ORDER BY "
                 "CASE WHEN user_id = :exact_id THEN 1 "
                 "WHEN username = :exact_keyword THEN 2 "
                 "WHEN username LIKE :keyword THEN 3 "
                 "WHEN display_name LIKE :keyword THEN 4 "
                 "ELSE 5 END "
                 "LIMIT :limit");

    QString searchPattern = QString("%%1%").arg(keyword);
    query.bindValue(":keyword", searchPattern);
    query.bindValue(":exact_keyword", keyword);
    query.bindValue(":exact_id", keyword);
    query.bindValue(":current_user", currentUserId);
    query.bindValue(":limit", limit);

    LOG_INFO(QString("Executing search query with pattern: '%1'").arg(searchPattern));

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to search users with keyword '%1': %2").arg(keyword).arg(query.lastError().text()));
        return userList;
    }

    LOG_INFO(QString("Search query executed successfully, processing results..."));

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
        
        LOG_INFO(QString("Found user: %1 (ID: %2, status: %3)").arg(userInfo["username"].toString()).arg(userId).arg(statusStr));
    }

    LOG_INFO(QString("Search completed, found %1 users for keyword '%2'").arg(userList.size()).arg(keyword));
    return userList;
}

bool FriendService::updateFriendNote(qint64 userId, qint64 friendId, const QString& note)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("UPDATE friendships SET note = :note WHERE user_id = :user_id AND friend_id = :friend_id AND status = 'accepted'");
    query.bindValue(":note", note);
    query.bindValue(":user_id", userId);
    query.bindValue(":friend_id", friendId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to update friend note for user %1, friend %2: %3").arg(userId).arg(friendId).arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() > 0) {
        LOG_INFO(QString("Updated friend note for user %1, friend %2").arg(userId).arg(friendId));
        return true;
    }

    LOG_WARNING(QString("No friendship found to update note: user %1, friend %2").arg(userId).arg(friendId));
    return false;
}

qint64 FriendService::findUserByIdentifier(const QString& identifier)
{
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 尝试按用户ID、用户名或邮箱查找
    query.prepare("SELECT id FROM users WHERE (user_id = :identifier OR username = :identifier OR email = :identifier) AND status = 'active'");
    query.bindValue(":identifier", identifier);

    if (!query.exec()) {
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
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("INSERT INTO friend_request_notifications "
                 "(friendship_id, from_user_id, to_user_id, notification_type, message) "
                 "VALUES (:friendship_id, :from_user, :to_user, :type, :message)");
    query.bindValue(":friendship_id", friendshipId);
    query.bindValue(":from_user", fromUserId);
    query.bindValue(":to_user", toUserId);
    query.bindValue(":type", type);
    query.bindValue(":message", message);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to create friend notification: %1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

FriendService::FriendshipStatus FriendService::getFriendshipStatusInternal(qint64 userId1, qint64 userId2)
{
    // 注意：此方法不加锁，调用者必须已经持有锁
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT status FROM friendships WHERE "
                 "(user_id = :user1 AND friend_id = :user2) OR "
                 "(user_id = :user2 AND friend_id = :user1) "
                 "ORDER BY requested_at DESC LIMIT 1");
    query.bindValue(":user1", userId1);
    query.bindValue(":user2", userId2);

    if (!query.exec() || !query.next()) {
        return Deleted; // 没有关系记录
    }

    QString status = query.value("status").toString();
    if (status == "pending") return Pending;
    if (status == "accepted") return Accepted;
    if (status == "blocked") return Blocked;
    return Deleted;
}

QSqlDatabase FriendService::getDatabase()
{
    return DatabaseManager::instance()->getConnection();
}

// 好友分组相关方法实现
QJsonArray FriendService::getFriendGroups(qint64 userId)
{
    QMutexLocker locker(&_mutex);

    QJsonArray groupList;
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("SELECT id, group_name, group_order, friend_count "
                 "FROM user_friend_groups WHERE user_id = :user_id "
                 "ORDER BY group_order ASC");
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
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

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 获取最大排序号
    query.prepare("SELECT COALESCE(MAX(group_order), 0) + 1 as next_order "
                 "FROM friend_groups WHERE user_id = :user_id");
    query.bindValue(":user_id", userId);

    if (!query.exec() || !query.next()) {
        LOG_ERROR(QString("Failed to get next group order for user %1").arg(userId));
        return false;
    }

    int nextOrder = query.value("next_order").toInt();

    // 创建新分组
    query.prepare("INSERT INTO friend_groups (user_id, group_name, group_order) "
                 "VALUES (:user_id, :group_name, :group_order)");
    query.bindValue(":user_id", userId);
    query.bindValue(":group_name", groupName);
    query.bindValue(":group_order", nextOrder);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to create friend group '%1' for user %2: %3")
                 .arg(groupName).arg(userId).arg(query.lastError().text()));
        return false;
    }

    LOG_INFO(QString("Created friend group '%1' for user %2").arg(groupName).arg(userId));
    return true;
}

bool FriendService::deleteFriendGroup(qint64 userId, qint64 groupId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();

    if (!db.transaction()) {
        LOG_ERROR("Failed to start transaction for deleting friend group");
        return false;
    }

    try {
        QSqlQuery query(db);

        // 验证分组属于该用户
        query.prepare("SELECT id FROM friend_groups WHERE id = :group_id AND user_id = :user_id");
        query.bindValue(":group_id", groupId);
        query.bindValue(":user_id", userId);

        if (!query.exec() || !query.next()) {
            throw std::runtime_error("Friend group not found or access denied");
        }

        // 将该分组的好友移动到默认分组
        query.prepare("UPDATE friendships SET group_id = "
                     "(SELECT id FROM friend_groups WHERE user_id = :user_id AND group_name = '默认分组' LIMIT 1) "
                     "WHERE user_id = :user_id AND group_id = :group_id");
        query.bindValue(":user_id", userId);
        query.bindValue(":group_id", groupId);

        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }

        // 删除分组
        query.prepare("DELETE FROM friend_groups WHERE id = :group_id AND user_id = :user_id");
        query.bindValue(":group_id", groupId);
        query.bindValue(":user_id", userId);

        if (!query.exec()) {
            throw std::runtime_error(query.lastError().text().toStdString());
        }

        if (!db.commit()) {
            throw std::runtime_error("Failed to commit transaction");
        }

        LOG_INFO(QString("Deleted friend group %1 for user %2").arg(groupId).arg(userId));
        return true;

    } catch (const std::exception& e) {
        db.rollback();
        LOG_ERROR(QString("Failed to delete friend group %1 for user %2: %3")
                 .arg(groupId).arg(userId).arg(e.what()));
        return false;
    }
}

bool FriendService::renameFriendGroup(qint64 userId, qint64 groupId, const QString& newName)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    query.prepare("UPDATE friend_groups SET group_name = :new_name "
                 "WHERE id = :group_id AND user_id = :user_id");
    query.bindValue(":new_name", newName);
    query.bindValue(":group_id", groupId);
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to rename friend group %1 for user %2: %3")
                 .arg(groupId).arg(userId).arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0) {
        LOG_WARNING(QString("Friend group %1 not found for user %2").arg(groupId).arg(userId));
        return false;
    }

    LOG_INFO(QString("Renamed friend group %1 to '%2' for user %3").arg(groupId).arg(newName).arg(userId));
    return true;
}

bool FriendService::moveFriendToGroup(qint64 userId, qint64 friendId, qint64 groupId)
{
    QMutexLocker locker(&_mutex);

    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);

    // 验证分组属于该用户
    query.prepare("SELECT id FROM friend_groups WHERE id = :group_id AND user_id = :user_id");
    query.bindValue(":group_id", groupId);
    query.bindValue(":user_id", userId);

    if (!query.exec() || !query.next()) {
        LOG_ERROR(QString("Friend group %1 not found for user %2").arg(groupId).arg(userId));
        return false;
    }

    // 更新好友分组
    query.prepare("UPDATE friendships SET group_id = :group_id "
                 "WHERE user_id = :user_id AND friend_id = :friend_id AND status = 'accepted'");
    query.bindValue(":group_id", groupId);
    query.bindValue(":user_id", userId);
    query.bindValue(":friend_id", friendId);

    if (!query.exec()) {
        LOG_ERROR(QString("Failed to move friend %1 to group %2 for user %3: %4")
                 .arg(friendId).arg(groupId).arg(userId).arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0) {
        LOG_WARNING(QString("Friendship not found: user %1, friend %2").arg(userId).arg(friendId));
        return false;
    }

    LOG_INFO(QString("Moved friend %1 to group %2 for user %3").arg(friendId).arg(groupId).arg(userId));
    return true;
}
