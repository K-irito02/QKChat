#include "AuthCache.h"
#include "../utils/Logger.h"
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>

// 静态成员初始化
AuthCache* AuthCache::s_instance = nullptr;
QMutex AuthCache::s_instanceMutex;

AuthCache::AuthCache(QObject *parent)
    : QObject(parent)
    , _sessionTimeoutMinutes(30)
    , _userCacheTimeoutMinutes(5)
    , _cleanupIntervalMinutes(1)
    , _cleanupTimer(nullptr)
    , _totalSessionsCreated(0)
    , _totalSessionsDestroyed(0)
    , _totalSessionsExpired(0)
    , _totalCacheHits(0)
    , _totalCacheMisses(0)
{
}

AuthCache::~AuthCache()
{
    if (_cleanupTimer) {
        _cleanupTimer->stop();
    }
}

AuthCache* AuthCache::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new AuthCache();
        }
    }
    return s_instance;
}

bool AuthCache::initialize(int sessionTimeoutMinutes, int userCacheTimeoutMinutes, int cleanupIntervalMinutes)
{
    _sessionTimeoutMinutes = sessionTimeoutMinutes;
    _userCacheTimeoutMinutes = userCacheTimeoutMinutes;
    _cleanupIntervalMinutes = cleanupIntervalMinutes;
    
    // 初始化清理定时器
    _cleanupTimer = new QTimer(this);
    connect(_cleanupTimer, &QTimer::timeout, this, &AuthCache::performCleanup);
    _cleanupTimer->start(_cleanupIntervalMinutes * 60 * 1000); // 转换为毫秒
    

    
    return true;
}

bool AuthCache::createSession(const QString& sessionToken, qint64 userId, 
                             const QString& username, const QString& clientId,
                             const QString& ipAddress, int timeoutMinutes)
{
    if (sessionToken.isEmpty() || userId <= 0) {
        return false;
    }
    
    QWriteLocker locker(&_sessionsLock);
    
    SessionInfo session;
    session.userId = userId;
    session.username = username;
    session.clientId = clientId;
    session.loginTime = QDateTime::currentDateTime();
    session.lastActivity = session.loginTime;
    session.expiryTime = session.loginTime.addSecs(timeoutMinutes * 60);
    session.ipAddress = ipAddress;
    session.isValid = true;
    
    _sessions[sessionToken] = session;
    
    QMutexLocker statsLocker(&_statsMutex);
    _totalSessionsCreated++;
    

    
    emit sessionCreated(sessionToken, userId);
    return true;
}

AuthCache::SessionInfo AuthCache::validateSession(const QString& sessionToken)
{
    if (sessionToken.isEmpty()) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        return SessionInfo();
    }
    
    QReadLocker locker(&_sessionsLock);
    
    auto it = _sessions.find(sessionToken);
    if (it == _sessions.end()) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        return SessionInfo();
    }
    
    SessionInfo session = it.value();
    
    // 检查会话是否过期
    if (session.isExpired()) {
        locker.unlock();
        
        // 需要写锁来移除过期会话
        QWriteLocker writeLocker(&_sessionsLock);
        _sessions.remove(sessionToken);
        
        QMutexLocker statsLocker(&_statsMutex);
        _totalSessionsExpired++;
        _totalCacheMisses++;
        

        
        emit sessionExpired(sessionToken, session.userId);
        return SessionInfo();
    }
    
    QMutexLocker statsLocker(&_statsMutex);
    _totalCacheHits++;
    
    return session;
}

bool AuthCache::updateSessionActivity(const QString& sessionToken)
{
    if (sessionToken.isEmpty()) {
        return false;
    }
    
    QWriteLocker locker(&_sessionsLock);
    
    auto it = _sessions.find(sessionToken);
    if (it == _sessions.end()) {
        return false;
    }
    
    // 检查会话是否过期
    if (it.value().isExpired()) {
        _sessions.erase(it);
        
        QMutexLocker statsLocker(&_statsMutex);
        _totalSessionsExpired++;
        
        return false;
    }
    
    // 更新活动时间
    it.value().updateActivity();
    
    return true;
}

bool AuthCache::destroySession(const QString& sessionToken)
{
    if (sessionToken.isEmpty()) {
        return false;
    }
    
    QWriteLocker locker(&_sessionsLock);
    
    auto it = _sessions.find(sessionToken);
    if (it == _sessions.end()) {
        return false;
    }
    
    qint64 userId = it.value().userId;
    _sessions.erase(it);
    
    QMutexLocker statsLocker(&_statsMutex);
    _totalSessionsDestroyed++;
    

    
    emit sessionDestroyed(sessionToken, userId);
    return true;
}

int AuthCache::destroyUserSessions(qint64 userId)
{
    if (userId <= 0) {
        return 0;
    }
    
    QWriteLocker locker(&_sessionsLock);
    
    QStringList tokensToRemove;
    for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
        if (it.value().userId == userId) {
            tokensToRemove.append(it.key());
        }
    }
    
    int removedCount = 0;
    for (const QString& token : tokensToRemove) {
        _sessions.remove(token);
        removedCount++;
        
        emit sessionDestroyed(token, userId);
    }
    
    if (removedCount > 0) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalSessionsDestroyed += removedCount;
        

    }
    
    return removedCount;
}

void AuthCache::cacheUserInfo(qint64 userId, const QString& username, 
                             const QString& email, const QString& passwordHash, 
                             bool isActive)
{
    if (userId <= 0 || username.isEmpty()) {
        return;
    }
    
    QWriteLocker locker(&_userCacheLock);
    
    UserInfo userInfo;
    userInfo.userId = userId;
    userInfo.username = username;
    userInfo.email = email;
    userInfo.passwordHash = passwordHash;
    userInfo.isActive = isActive;
    userInfo.cacheTime = QDateTime::currentDateTime();
    
    _userInfoCache[userId] = userInfo;
    _usernameToIdMap[username] = userId;
    

}

AuthCache::UserInfo AuthCache::getCachedUserInfo(qint64 userId)
{
    if (userId <= 0) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        return UserInfo();
    }
    
    QReadLocker locker(&_userCacheLock);
    
    auto it = _userInfoCache.find(userId);
    if (it == _userInfoCache.end()) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        return UserInfo();
    }
    
    UserInfo userInfo = it.value();
    
    // 检查缓存是否过期
    if (userInfo.isExpired(_userCacheTimeoutMinutes * 60)) {
        locker.unlock();
        
        // 需要写锁来移除过期缓存
        QWriteLocker writeLocker(&_userCacheLock);
        _userInfoCache.remove(userId);
        _usernameToIdMap.remove(userInfo.username);
        
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        
        return UserInfo();
    }
    
    QMutexLocker statsLocker(&_statsMutex);
    _totalCacheHits++;
    
    return userInfo;
}

AuthCache::UserInfo AuthCache::getCachedUserInfoByUsername(const QString& username)
{
    if (username.isEmpty()) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        return UserInfo();
    }
    
    QReadLocker locker(&_userCacheLock);
    
    auto it = _usernameToIdMap.find(username);
    if (it == _usernameToIdMap.end()) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalCacheMisses++;
        return UserInfo();
    }
    
    qint64 userId = it.value();
    locker.unlock();
    
    return getCachedUserInfo(userId);
}

void AuthCache::removeUserInfoCache(qint64 userId)
{
    if (userId <= 0) {
        return;
    }
    
    QWriteLocker locker(&_userCacheLock);
    
    auto it = _userInfoCache.find(userId);
    if (it != _userInfoCache.end()) {
        QString username = it.value().username;
        _userInfoCache.erase(it);
        _usernameToIdMap.remove(username);
        
    
    }
}

int AuthCache::getActiveSessionCount() const
{
    QReadLocker locker(&_sessionsLock);
    return _sessions.size();
}

int AuthCache::getCachedUserCount() const
{
    QReadLocker locker(&_userCacheLock);
    return _userInfoCache.size();
}

QJsonObject AuthCache::getCacheStatistics() const
{
    QJsonObject stats;
    
    QMutexLocker statsLocker(&_statsMutex);
    
    stats["active_sessions"] = getActiveSessionCount();
    stats["cached_users"] = getCachedUserCount();
    stats["total_sessions_created"] = _totalSessionsCreated;
    stats["total_sessions_destroyed"] = _totalSessionsDestroyed;
    stats["total_sessions_expired"] = _totalSessionsExpired;
    stats["total_cache_hits"] = _totalCacheHits;
    stats["total_cache_misses"] = _totalCacheMisses;
    
    // 计算缓存命中率
    qint64 totalRequests = _totalCacheHits + _totalCacheMisses;
    if (totalRequests > 0) {
        stats["cache_hit_rate"] = static_cast<double>(_totalCacheHits) / totalRequests;
    } else {
        stats["cache_hit_rate"] = 0.0;
    }
    
    stats["session_timeout_minutes"] = _sessionTimeoutMinutes;
    stats["user_cache_timeout_minutes"] = _userCacheTimeoutMinutes;
    stats["cleanup_interval_minutes"] = _cleanupIntervalMinutes;
    
    return stats;
}

void AuthCache::cleanup()
{
    performCleanup();
}

void AuthCache::performCleanup()
{
    QDateTime now = QDateTime::currentDateTime();
    int expiredSessions = 0;
    int expiredUsers = 0;
    
    // 清理过期会话
    {
        QWriteLocker locker(&_sessionsLock);
        
        auto it = _sessions.begin();
        while (it != _sessions.end()) {
            if (it.value().isExpired()) {
                qint64 userId = it.value().userId;
                QString token = it.key();
                it = _sessions.erase(it);
                expiredSessions++;
                
                emit sessionExpired(token, userId);
            } else {
                ++it;
            }
        }
    }
    
    // 清理过期用户信息缓存
    {
        QWriteLocker locker(&_userCacheLock);
        
        auto it = _userInfoCache.begin();
        while (it != _userInfoCache.end()) {
            if (it.value().isExpired(_userCacheTimeoutMinutes * 60)) {
                QString username = it.value().username;
                it = _userInfoCache.erase(it);
                _usernameToIdMap.remove(username);
                expiredUsers++;
            } else {
                ++it;
            }
        }
    }
    
    // 更新统计信息
    if (expiredSessions > 0 || expiredUsers > 0) {
        QMutexLocker statsLocker(&_statsMutex);
        _totalSessionsExpired += expiredSessions;
        
        
    }
}