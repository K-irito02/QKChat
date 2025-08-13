#include "OnlineStatusService.h"
#include "FriendService.h"
#include "../database/DatabaseManager.h"
#include "../network/ThreadPoolServer.h"
#include <QSqlRecord>
#include <QVariant>

// 静态成员初始化
OnlineStatusService* OnlineStatusService::s_instance = nullptr;
QMutex OnlineStatusService::s_instanceMutex;

OnlineStatusService::OnlineStatusService(QObject *parent)
    : QObject(parent)
    , _initialized(false)
    , _cleanupTimer(new QTimer(this))
{
    // 设置清理定时器
    _cleanupTimer->setInterval(CLEANUP_INTERVAL);
    connect(_cleanupTimer, &QTimer::timeout, this, &OnlineStatusService::onCleanupTimer);
}

OnlineStatusService::~OnlineStatusService()
{
    if (_cleanupTimer) {
        _cleanupTimer->stop();
    }
}

OnlineStatusService* OnlineStatusService::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new OnlineStatusService();
        }
    }
    return s_instance;
}

bool OnlineStatusService::initialize()
{
    QMutexLocker locker(&_mutex);
    
    if (_initialized) {
        return true;
    }
    
    // 测试数据库连接 - 使用RAII包装器
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to initialize OnlineStatusService: database not available");
        return false;
    }
    
    // 启动清理定时器
    _cleanupTimer->start();
    
    _initialized = true;
    // OnlineStatusService初始化成功
    return true;
}

bool OnlineStatusService::userOnline(qint64 userId, const QString& clientId, const QString& deviceInfo, const QString& ipAddress)
{
    QMutexLocker locker(&_mutex);
    
    // 获取当前状态
    UserStatusInfo currentStatus = getUserStatus(userId);
    OnlineStatus oldStatus = currentStatus.status;
    
    // 更新状态为在线
    if (!updateStatusInDatabase(userId, Online, clientId, deviceInfo, ipAddress)) {
        LOG_ERROR(QString("Failed to update online status for user %1").arg(userId));
        return false;
    }
    
    // 更新缓存
    UserStatusInfo newStatus(userId, Online, QDateTime::currentDateTime());
    newStatus.clientId = clientId;
    newStatus.deviceInfo = deviceInfo;
    newStatus.ipAddress = ipAddress;
    _userStatusCache[userId] = newStatus;
    
    // 用户上线
    
    // 发送信号
    if (oldStatus != Online) {
        emit userStatusChanged(userId, oldStatus, Online);
        emit userWentOnline(userId, clientId);
        
        // 广播状态变化给好友
        broadcastStatusToFriends(userId, Online);
    }
    
    return true;
}

bool OnlineStatusService::userOffline(qint64 userId, const QString& clientId)
{
    QMutexLocker locker(&_mutex);
    
    // 获取当前状态
    UserStatusInfo currentStatus = getUserStatus(userId);
    OnlineStatus oldStatus = currentStatus.status;
    
    // 更新状态为离线
    if (!updateStatusInDatabase(userId, Offline, clientId)) {
        LOG_ERROR(QString("Failed to update offline status for user %1").arg(userId));
        return false;
    }
    
    // 更新缓存
    UserStatusInfo newStatus(userId, Offline, QDateTime::currentDateTime());
    newStatus.clientId = clientId;
    _userStatusCache[userId] = newStatus;
    
    // 用户下线
    
    // 发送信号
    if (oldStatus != Offline) {
        emit userStatusChanged(userId, oldStatus, Offline);
        emit userWentOffline(userId, clientId);
        
        // 广播状态变化给好友
        broadcastStatusToFriends(userId, Offline);
    }
    
    return true;
}

bool OnlineStatusService::updateUserStatus(qint64 userId, OnlineStatus status, const QString& clientId)
{
    QMutexLocker locker(&_mutex);
    
    // 获取当前状态
    UserStatusInfo currentStatus = getUserStatus(userId);
    OnlineStatus oldStatus = currentStatus.status;
    
    if (oldStatus == status) {
        return true; // 状态没有变化
    }
    
    // 更新状态
    if (!updateStatusInDatabase(userId, status, clientId)) {
        LOG_ERROR(QString("Failed to update status for user %1 to %2").arg(userId).arg(statusToString(status)));
        return false;
    }
    
    // 更新缓存
    UserStatusInfo newStatus(userId, status, QDateTime::currentDateTime());
    newStatus.clientId = clientId;
    _userStatusCache[userId] = newStatus;
    
    // 用户状态已更改
    
    // 发送信号
    emit userStatusChanged(userId, oldStatus, status);
    
    // 广播状态变化给好友
    broadcastStatusToFriends(userId, status);
    
    return true;
}

bool OnlineStatusService::updateHeartbeat(qint64 userId, const QString& clientId)
{
    QMutexLocker locker(&_mutex);
    
    // 更新最后在线时间 - 使用RAII包装器
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for heartbeat update");
        return false;
    }
    
    int result = dbConn.executeUpdate(
        "UPDATE user_online_status SET last_seen = NOW() WHERE user_id = ? AND client_id = ?",
        {userId, clientId}
    );
    
    if (result == -1) {
        LOG_ERROR(QString("Failed to update heartbeat for user %1").arg(userId));
        return false;
    }
    
    // 更新缓存
    if (_userStatusCache.contains(userId)) {
        _userStatusCache[userId].lastSeen = QDateTime::currentDateTime();
    }
    
    return true;
}

OnlineStatusService::UserStatusInfo OnlineStatusService::getUserStatus(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    // 先检查缓存
    if (_userStatusCache.contains(userId)) {
        UserStatusInfo cachedStatus = _userStatusCache[userId];
        
        // 检查缓存是否过期（超过心跳超时时间）
        if (cachedStatus.lastSeen.secsTo(QDateTime::currentDateTime()) < HEARTBEAT_TIMEOUT) {
            return cachedStatus;
        }
    }
    
    // 从数据库加载
    UserStatusInfo status = loadStatusFromDatabase(userId);
    
    // 更新缓存
    if (status.userId != -1) {
        _userStatusCache[userId] = status;
    }
    
    return status;
}

QMap<qint64, OnlineStatusService::UserStatusInfo> OnlineStatusService::getUsersStatus(const QList<qint64>& userIds)
{
    QMutexLocker locker(&_mutex);

    QMap<qint64, UserStatusInfo> statusMap;

    for (qint64 userId : userIds) {
        // 直接访问缓存，避免递归锁问题
        if (_userStatusCache.contains(userId)) {
            UserStatusInfo cachedStatus = _userStatusCache[userId];

            // 检查缓存是否过期
            if (cachedStatus.lastSeen.secsTo(QDateTime::currentDateTime()) < HEARTBEAT_TIMEOUT) {
                statusMap[userId] = cachedStatus;
                continue;
            }
        }

        // 需要从数据库加载，临时释放锁
        locker.unlock();
        UserStatusInfo status = loadStatusFromDatabase(userId);
        locker.relock();

        // 更新缓存
        if (status.userId != -1) {
            _userStatusCache[userId] = status;
            statusMap[userId] = status;
        }
    }

    return statusMap;
}

QJsonArray OnlineStatusService::getFriendsOnlineStatus(qint64 userId)
{
    QMutexLocker locker(&_mutex);

    QJsonArray friendsStatus;

    // 获取用户好友列表
    QList<qint64> friends = getUserFriends(userId);

    // 获取好友状态
    for (qint64 friendId : friends) {
        UserStatusInfo status = getUserStatus(friendId);

        QJsonObject friendStatus;
        friendStatus["user_id"] = friendId;
        friendStatus["status"] = statusToString(status.status);
        friendStatus["last_seen"] = status.lastSeen.toString(Qt::ISODate);

        friendsStatus.append(friendStatus);
    }

    return friendsStatus;
}

int OnlineStatusService::getOnlineUserCount()
{
    QMutexLocker locker(&_mutex);

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection");
        return 0;
    }

    QSqlQuery query = dbConn.executeQuery(
        "SELECT COUNT(DISTINCT user_id) FROM user_online_status WHERE "
        "status IN ('online', 'away', 'busy') AND "
        "last_seen > DATE_SUB(NOW(), INTERVAL ? SECOND)",
        {HEARTBEAT_TIMEOUT}
    );

    if (query.lastError().isValid() || !query.next()) {
        LOG_ERROR(QString("Failed to get online user count: %1").arg(query.lastError().text()));
        return 0;
    }

    return query.value(0).toInt();
}

QList<qint64> OnlineStatusService::getOnlineUsers()
{
    QMutexLocker locker(&_mutex);

    QList<qint64> onlineUsers;
    
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for online users");
        return onlineUsers;
    }

    QSqlQuery query = dbConn.executeQuery(
        "SELECT DISTINCT user_id FROM user_online_status WHERE "
        "status IN ('online', 'away', 'busy') AND "
        "last_seen > DATE_SUB(NOW(), INTERVAL ? SECOND)",
        {HEARTBEAT_TIMEOUT}
    );

    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get online users: %1").arg(query.lastError().text()));
        return onlineUsers;
    }

    while (query.next()) {
        onlineUsers.append(query.value("user_id").toLongLong());
    }

    return onlineUsers;
}

bool OnlineStatusService::isUserOnline(qint64 userId)
{
    UserStatusInfo status = getUserStatus(userId);

    // 检查状态和最后在线时间
    if (status.status == Offline || status.status == Invisible) {
        return false;
    }

    // 检查心跳超时
    return status.lastSeen.secsTo(QDateTime::currentDateTime()) < HEARTBEAT_TIMEOUT;
}

void OnlineStatusService::broadcastStatusToFriends(qint64 userId, OnlineStatus status)
{
    // 获取用户好友列表
    QList<qint64> friends = getUserFriends(userId);

    if (friends.isEmpty()) {
        return;
    }

    // 构建状态变化消息
    QJsonObject statusMessage;
    statusMessage["action"] = "friend_status_changed";
    statusMessage["user_id"] = userId;
    statusMessage["status"] = statusToString(status);
    statusMessage["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 广播给在线的好友
    ThreadPoolServer* server = ThreadPoolServer::instance();
    if (!server) {
        LOG_ERROR("ThreadPoolServer instance not available for status broadcast");
        return;
    }

    for (qint64 friendId : friends) {
        if (isUserOnline(friendId)) {
            server->sendMessageToUser(friendId, statusMessage);
        }
    }

    // 状态变更已广播
}

void OnlineStatusService::cleanupExpiredStatus()
{
    QMutexLocker locker(&_mutex);

    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for cleanup");
        return;
    }

    // 将超时的用户状态设为离线
    int affectedRows = dbConn.executeUpdate(
        "UPDATE user_online_status SET status = 'offline' WHERE "
        "status != 'offline' AND "
        "last_seen < DATE_SUB(NOW(), INTERVAL ? SECOND)",
        {HEARTBEAT_TIMEOUT}
    );

    if (affectedRows == -1) {
        LOG_ERROR("Failed to cleanup expired status");
        return;
    }

    if (affectedRows > 0) {
        // 清理过期状态记录完成

        // 清理缓存中的过期状态
        QDateTime now = QDateTime::currentDateTime();
        auto it = _userStatusCache.begin();
        while (it != _userStatusCache.end()) {
            if (it.value().lastSeen.secsTo(now) >= HEARTBEAT_TIMEOUT) {
                it = _userStatusCache.erase(it);
            } else {
                ++it;
            }
        }
    }
}

QString OnlineStatusService::statusToString(OnlineStatus status)
{
    switch (status) {
        case Online: return "online";
        case Offline: return "offline";
        case Away: return "away";
        case Busy: return "busy";
        case Invisible: return "invisible";
        default: return "offline";
    }
}

OnlineStatusService::OnlineStatus OnlineStatusService::stringToStatus(const QString& statusStr)
{
    if (statusStr == "online") return Online;
    if (statusStr == "away") return Away;
    if (statusStr == "busy") return Busy;
    if (statusStr == "invisible") return Invisible;
    return Offline;
}

void OnlineStatusService::onCleanupTimer()
{
    cleanupExpiredStatus();
}

// 移除getDatabase方法，改用RAII包装器

bool OnlineStatusService::updateStatusInDatabase(qint64 userId, OnlineStatus status, const QString& clientId,
                                                const QString& deviceInfo, const QString& ipAddress)
{
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for status update");
        return false;
    }

    // 使用UPSERT语法更新或插入状态
    int result = dbConn.executeUpdate(
        "INSERT INTO user_online_status (user_id, status, client_id, device_info, ip_address, last_seen) "
        "VALUES (?, ?, ?, ?, ?, NOW()) "
        "ON DUPLICATE KEY UPDATE "
        "status = VALUES(status), "
        "device_info = VALUES(device_info), "
        "ip_address = VALUES(ip_address), "
        "last_seen = NOW()",
        {userId, statusToString(status), clientId, deviceInfo, ipAddress}
    );

    if (result == -1) {
        LOG_ERROR(QString("Failed to update user status in database for user %1").arg(userId));
        return false;
    }

    return true;
}

OnlineStatusService::UserStatusInfo OnlineStatusService::loadStatusFromDatabase(qint64 userId)
{
    UserStatusInfo status;

    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for loading status");
        return status;
    }

    QSqlQuery query = dbConn.executeQuery(
        "SELECT status, last_seen, client_id, device_info, ip_address "
        "FROM user_online_status WHERE user_id = ? "
        "ORDER BY last_seen DESC LIMIT 1",
        {userId}
    );

    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to load user status from database: %1").arg(query.lastError().text()));
        return status;
    }

    if (query.next()) {
        status.userId = userId;
        status.status = stringToStatus(query.value("status").toString());
        status.lastSeen = query.value("last_seen").toDateTime();
        status.clientId = query.value("client_id").toString();
        status.deviceInfo = query.value("device_info").toString();
        status.ipAddress = query.value("ip_address").toString();

        // 检查是否超时
        if (status.lastSeen.secsTo(QDateTime::currentDateTime()) >= HEARTBEAT_TIMEOUT) {
            status.status = Offline;
        }
    } else {
        // 用户没有状态记录，默认为离线
        status.userId = userId;
        status.status = Offline;
        status.lastSeen = QDateTime::currentDateTime();
    }

    return status;
}

QList<qint64> OnlineStatusService::getUserFriends(qint64 userId)
{
    QList<qint64> friends;

    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for getting friends");
        return friends;
    }

    QSqlQuery query = dbConn.executeQuery(
        "SELECT friend_id FROM friendships WHERE user_id = ? AND status = 'accepted'",
        {userId}
    );

    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Failed to get user friends: %1").arg(query.lastError().text()));
        return friends;
    }

    while (query.next()) {
        friends.append(query.value("friend_id").toLongLong());
    }

    return friends;
}
