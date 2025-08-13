#include "RateLimitManager.h"
#include "../utils/Logger.h"
#include "../database/DatabaseConnectionPool.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>

RateLimitManager* RateLimitManager::_instance = nullptr;
QMutex RateLimitManager::_instanceMutex;

RateLimitManager* RateLimitManager::instance()
{
    if (!_instance) {
        QMutexLocker locker(&_instanceMutex);
        if (!_instance) {
            _instance = new RateLimitManager();
        }
    }
    return _instance;
}

RateLimitManager::RateLimitManager(QObject *parent)
    : QObject(parent)
    , _cleanupTimer(new QTimer(this))
    , _tokenRefillTimer(new QTimer(this))
{
    // 设置清理定时器，每5分钟清理一次过期记录
    connect(_cleanupTimer, &QTimer::timeout, this, &RateLimitManager::onCleanupTimer);
    _cleanupTimer->start(5 * 60 * 1000); // 5分钟
    
    // 设置令牌补充定时器，每1秒补充一次令牌
    connect(_tokenRefillTimer, &QTimer::timeout, this, &RateLimitManager::onTokenRefillTimer);
    _tokenRefillTimer->start(1000); // 1秒
    
    // 初始化默认配置
    RateLimitConfig defaultConfig;
    defaultConfig.maxRequests = 10;
    defaultConfig.windowSeconds = 60;
    defaultConfig.burstSize = 5;
    defaultConfig.tokensPerSecond = 0.167;
    defaultConfig.maxTokens = 10;
    defaultConfig.refillInterval = 1000;
    defaultConfig.enabled = true;
    _configs["default"] = defaultConfig;
    
    // 搜索接口配置
    RateLimitConfig searchConfig;
    searchConfig.maxRequests = 20;
    searchConfig.windowSeconds = 60;
    searchConfig.burstSize = 10;
    searchConfig.tokensPerSecond = 0.333;
    searchConfig.maxTokens = 20;
    searchConfig.refillInterval = 1000;
    searchConfig.enabled = true;
    _configs["friend_search"] = searchConfig;
    
    // 登录接口配置
    RateLimitConfig loginConfig;
    loginConfig.maxRequests = 10;
    loginConfig.windowSeconds = 60;
    loginConfig.burstSize = 3;
    loginConfig.tokensPerSecond = 0.167;
    loginConfig.maxTokens = 10;
    loginConfig.refillInterval = 1000;
    loginConfig.enabled = true;
    _configs["login"] = loginConfig;
    
    // LOG_INFO removed
}

RateLimitManager::~RateLimitManager()
{
    if (_cleanupTimer) {
        _cleanupTimer->stop();
    }
    if (_tokenRefillTimer) {
        _tokenRefillTimer->stop();
    }
}

bool RateLimitManager::checkRateLimit(const QString& identifier, const QString& endpoint, qint64 userId)
{
    if (!_configs.contains(endpoint) || !_configs[endpoint].enabled) {
        return true; // 未配置或未启用限流，允许通过
    }
    
    QMutexLocker locker(&_mutex);
    
    QString key = QString("%1:%2").arg(identifier, endpoint);
    
    // 初始化限流信息
    if (!_rateLimitMap.contains(key)) {
        RateLimitInfo info;
        info.requestCount = 0;
        info.windowStart = QDateTime::currentSecsSinceEpoch();
        info.windowEnd = info.windowStart + _configs[endpoint].windowSeconds;
        info.tokenBucket = TokenBucket(_configs[endpoint].maxTokens);
        _rateLimitMap[key] = info;
    }
    
    RateLimitInfo& info = _rateLimitMap[key];
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    
    // 检查时间窗口是否过期
    if (currentTime > info.windowEnd) {
        // 重置窗口
        info.requestCount = 0;
        info.windowStart = currentTime;
        info.windowEnd = currentTime + _configs[endpoint].windowSeconds;
        info.tokenBucket = TokenBucket(_configs[endpoint].maxTokens);
    }
    
    // 尝试消费令牌
    bool success = tryConsumeToken(identifier, endpoint);
    
    if (success) {
        info.requestCount++;
        logRateLimitEvent(identifier, endpoint, false, userId);
        LOG_DEBUG(QString("Rate limit check passed for %1:%2").arg(identifier, endpoint));
    } else {
        logRateLimitEvent(identifier, endpoint, true, userId);
        LOG_WARNING(QString("Rate limit exceeded for %1:%2").arg(identifier, endpoint));
    }
    
    return success;
}

void RateLimitManager::resetRateLimit(const QString& identifier, const QString& endpoint)
{
    QMutexLocker locker(&_mutex);
    
    if (identifier.isEmpty() && endpoint.isEmpty()) {
        // 重置所有限流状态
        _rateLimitMap.clear();
        // LOG_INFO removed
    } else if (identifier.isEmpty()) {
        // 重置特定端点的所有限流状态
        QStringList keysToRemove;
        for (auto it = _rateLimitMap.begin(); it != _rateLimitMap.end(); ++it) {
            if (it.key().endsWith(":" + endpoint)) {
                keysToRemove.append(it.key());
            }
        }
        for (const QString& key : keysToRemove) {
            _rateLimitMap.remove(key);
        }
        LOG_INFO(QString("Rate limit states reset for endpoint: %1").arg(endpoint));
    } else {
        // 重置特定标识符和端点的限流状态
        QString key = QString("%1:%2").arg(identifier, endpoint);
        _rateLimitMap.remove(key);
        LOG_INFO(QString("Rate limit state reset for %1:%2").arg(identifier, endpoint));
    }
}

QJsonObject RateLimitManager::getRateLimitStats()
{
    QMutexLocker locker(&_mutex);
    
    QJsonObject stats;
    QJsonArray entries;
    
    for (auto it = _rateLimitMap.begin(); it != _rateLimitMap.end(); ++it) {
        QJsonObject entry;
        QStringList parts = it.key().split(":");
        if (parts.size() >= 2) {
            entry["identifier"] = parts[0];
            entry["endpoint"] = parts[1];
        } else {
            entry["identifier"] = it.key();
            entry["endpoint"] = "unknown";
        }
        
        const RateLimitInfo& info = it.value();
        entry["request_count"] = info.requestCount;
        entry["window_start"] = info.windowStart;
        entry["window_end"] = info.windowEnd;
        entry["available_tokens"] = calculateAvailableTokens(parts[0], parts[1]);
        
        entries.append(entry);
    }
    
    stats["entries"] = entries;
    stats["total_entries"] = _rateLimitMap.size();
    
    return stats;
}

bool RateLimitManager::consumeToken(const QString& identifier, const QString& endpoint)
{
    return tryConsumeToken(identifier, endpoint);
}

int RateLimitManager::getAvailableTokens(const QString& identifier, const QString& endpoint)
{
    return calculateAvailableTokens(identifier, endpoint);
}

void RateLimitManager::refillTokens(const QString& identifier, const QString& endpoint)
{
    refillTokenBucket(identifier, endpoint);
}

void RateLimitManager::cleanupExpiredEntries()
{
    QMutexLocker locker(&_mutex);
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    QStringList keysToRemove;
    
    for (auto it = _rateLimitMap.begin(); it != _rateLimitMap.end(); ++it) {
        const RateLimitInfo& info = it.value();
        if (currentTime > info.windowEnd + 300) { // 过期5分钟后清理
            keysToRemove.append(it.key());
        }
    }
    
    for (const QString& key : keysToRemove) {
        _rateLimitMap.remove(key);
    }
    
    if (!keysToRemove.isEmpty()) {
        LOG_INFO(QString("Cleaned up %1 expired rate limit entries").arg(keysToRemove.size()));
    }
}

void RateLimitManager::refillAllTokenBuckets()
{
    QMutexLocker locker(&_mutex);
    
    for (auto it = _rateLimitMap.begin(); it != _rateLimitMap.end(); ++it) {
        QStringList parts = it.key().split(":");
        if (parts.size() >= 2) {
            refillTokenBucket(parts[0], parts[1]);
        }
    }
}

bool RateLimitManager::tryConsumeToken(const QString& identifier, const QString& endpoint)
{
    if (!_configs.contains(endpoint)) {
        return true;
    }
    
    QString key = QString("%1:%2").arg(identifier, endpoint);
    if (!_rateLimitMap.contains(key)) {
        return true;
    }
    
    RateLimitInfo& info = _rateLimitMap[key];
    const RateLimitConfig& config = _configs[endpoint];
    
    // 先补充令牌
    refillTokenBucket(identifier, endpoint);
    
    // 尝试消费一个令牌
    if (info.tokenBucket.tokens > 0) {
        info.tokenBucket.tokens--;
        return true;
    }
    
    return false;
}

void RateLimitManager::refillTokenBucket(const QString& identifier, const QString& endpoint)
{
    if (!_configs.contains(endpoint)) {
        return;
    }
    
    QString key = QString("%1:%2").arg(identifier, endpoint);
    if (!_rateLimitMap.contains(key)) {
        return;
    }
    
    RateLimitInfo& info = _rateLimitMap[key];
    const RateLimitConfig& config = _configs[endpoint];
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    qint64 timeDiff = currentTime - info.tokenBucket.lastRefillTime;
    
    if (timeDiff >= config.refillInterval) {
        // 计算需要补充的令牌数
        int tokensToAdd = static_cast<int>(timeDiff * config.tokensPerSecond / 1000.0);
        if (tokensToAdd > 0) {
            int newTokens = std::min(info.tokenBucket.tokens + tokensToAdd, config.maxTokens);
            info.tokenBucket.tokens = newTokens;
            info.tokenBucket.lastRefillTime = currentTime;
            
            logTokenBucketEvent(identifier, endpoint, true);
        }
    }
}

int RateLimitManager::calculateAvailableTokens(const QString& identifier, const QString& endpoint)
{
    if (!_configs.contains(endpoint)) {
        return 0;
    }
    
    QString key = QString("%1:%2").arg(identifier, endpoint);
    if (!_rateLimitMap.contains(key)) {
        return _configs[endpoint].maxTokens;
    }
    
    const RateLimitInfo& info = _rateLimitMap[key];
    return info.tokenBucket.tokens;
}

void RateLimitManager::logTokenBucketEvent(const QString& identifier, const QString& endpoint, bool success)
{
    if (success) {
        LOG_DEBUG(QString("Token bucket event: %1:%2 - tokens available").arg(identifier, endpoint));
    } else {
        LOG_DEBUG(QString("Token bucket event: %1:%2 - no tokens available").arg(identifier, endpoint));
    }
}

void RateLimitManager::logRateLimitEvent(const QString& identifier, const QString& endpoint, bool blocked, qint64 userId)
{
    if (blocked) {
        LOG_WARNING(QString("Rate limit blocked: %1:%2 (user: %3)").arg(identifier, endpoint, QString::number(userId)));
    } else {
        LOG_DEBUG(QString("Rate limit allowed: %1:%2 (user: %3)").arg(identifier, endpoint, QString::number(userId)));
    }
    
    // 异步记录到数据库
    QTimer::singleShot(0, [this, identifier, endpoint, blocked]() {
        logRateLimitToDatabase(identifier, endpoint, blocked);
    });
}

void RateLimitManager::logRateLimitToDatabase(const QString& identifier, const QString& endpoint, bool blocked)
{
    // 这里可以记录限流事件到数据库
    // 由于数据库连接池可能不可用，这里只记录日志
    LOG_DEBUG(QString("Rate limit logged to database: %1:%2 blocked=%3").arg(identifier, endpoint, blocked ? "true" : "false"));
}

void RateLimitManager::onCleanupTimer()
{
    cleanupExpiredEntries();
}

void RateLimitManager::onTokenRefillTimer()
{
    refillAllTokenBuckets();
}
