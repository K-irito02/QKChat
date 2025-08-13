#include "CacheManager.h"
#include "../utils/Logger.h"
#include "../database/DatabaseConnectionPool.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonDocument>

CacheManager* CacheManager::_instance = nullptr;
QMutex CacheManager::_instanceMutex;

CacheManager* CacheManager::instance()
{
    if (!_instance) {
        QMutexLocker locker(&_instanceMutex);
        if (!_instance) {
            _instance = new CacheManager();
        }
    }
    return _instance;
}

CacheManager::CacheManager(QObject *parent)
    : QObject(parent)
    , _cleanupTimer(new QTimer(this))
    , _l2CleanupTimer(new QTimer(this))
    , _hotDataTimer(new QTimer(this))
{
    // 设置清理定时器，每5分钟清理一次过期缓存
    connect(_cleanupTimer, &QTimer::timeout, this, &CacheManager::onCleanupTimer);
    _cleanupTimer->start(5 * 60 * 1000); // 5分钟
    
    // 设置L2缓存清理定时器，每30分钟清理一次
    connect(_l2CleanupTimer, &QTimer::timeout, this, &CacheManager::onL2CleanupTimer);
    _l2CleanupTimer->start(30 * 60 * 1000); // 30分钟
    
    // 设置热点数据同步定时器，每10分钟同步一次
    connect(_hotDataTimer, &QTimer::timeout, this, &CacheManager::onHotDataTimer);
    _hotDataTimer->start(10 * 60 * 1000); // 10分钟
    
    // 初始化时加载热点数据
    loadHotDataFromDatabase();
    
    // LOG_INFO removed
}

CacheManager::~CacheManager()
{
    if (_cleanupTimer) {
        _cleanupTimer->stop();
    }
    if (_l2CleanupTimer) {
        _l2CleanupTimer->stop();
    }
    if (_hotDataTimer) {
        _hotDataTimer->stop();
    }
}

bool CacheManager::setCache(const QString& key, const QJsonObject& data, int ttlSeconds)
{
    QMutexLocker locker(&_cacheMutex);
    
    qint64 expiryTime = QDateTime::currentSecsSinceEpoch() + ttlSeconds;
    
    _memoryCache[key] = data;
    _cacheExpiry[key] = expiryTime;
    
    logCacheOperation("SET", key, true);
    return true;
}

QJsonObject CacheManager::getCache(const QString& key)
{
    QMutexLocker locker(&_cacheMutex);
    
    if (!_memoryCache.contains(key)) {
        logCacheOperation("GET", key, false);
        return QJsonObject();
    }
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    if (_cacheExpiry.contains(key) && _cacheExpiry[key] < currentTime) {
        // 缓存已过期，删除
        _memoryCache.remove(key);
        _cacheExpiry.remove(key);
        logCacheOperation("GET_EXPIRED", key, false);
        return QJsonObject();
    }
    
    logCacheOperation("GET", key, true);
    return _memoryCache[key];
}

bool CacheManager::removeCache(const QString& key)
{
    QMutexLocker locker(&_cacheMutex);
    
    bool removed = _memoryCache.remove(key) > 0;
    _cacheExpiry.remove(key);
    
    logCacheOperation("REMOVE", key, removed);
    return removed;
}

void CacheManager::clearCache()
{
    QMutexLocker locker(&_cacheMutex);
    
    _memoryCache.clear();
    _cacheExpiry.clear();
    
    // LOG_INFO removed
}

bool CacheManager::setSearchCache(const QString& keyword, qint64 userId, const QJsonArray& results, int ttlSeconds)
{
    QString cacheKey = generateCacheKey(keyword, userId);
    QJsonObject cacheData;
    cacheData["keyword"] = keyword;
    cacheData["userId"] = userId;
    cacheData["results"] = results;
    cacheData["timestamp"] = QDateTime::currentSecsSinceEpoch();
    
    // 记录热点数据
    recordHotData("user_search", keyword);
    
    return setCache(cacheKey, cacheData, ttlSeconds);
}

QJsonArray CacheManager::getSearchCache(const QString& keyword, qint64 userId)
{
    QString cacheKey = generateCacheKey(keyword, userId);
    QJsonObject cacheData = getCache(cacheKey);
    
    if (cacheData.isEmpty()) {
        return QJsonArray();
    }
    
    // 记录热点数据
    recordHotData("user_search", keyword);
    
    return cacheData["results"].toArray();
}

bool CacheManager::setL2Cache(const QString& key, const QJsonObject& data, int ttlSeconds)
{
    QJsonDocument doc(data);
    QString jsonData = doc.toJson(QJsonDocument::Compact);
    
    QString sql = "INSERT INTO search_cache (cache_key, search_keyword, cache_data, expires_at) "
                  "VALUES (?, ?, ?, DATE_ADD(NOW(), INTERVAL ? SECOND)) "
                  "ON DUPLICATE KEY UPDATE "
                  "cache_data = VALUES(cache_data), "
                  "hit_count = hit_count + 1, "
                  "expires_at = VALUES(expires_at)";
    
    QVariantList params;
    params << key << key << jsonData << ttlSeconds;
    
    bool success = executeL2CacheQuery(sql, params);
    if (success) {
        logCacheOperation("SET_L2", key, true);
    } else {
        logCacheOperation("SET_L2", key, false);
    }
    
    return success;
}

QJsonObject CacheManager::getL2Cache(const QString& key)
{
    QString sql = "SELECT cache_data, hit_count, expires_at FROM search_cache "
                  "WHERE cache_key = ? AND expires_at > NOW()";
    
    QVariantList params;
    params << key;
    
    QJsonObject result = executeL2CacheSelect(sql, params);
    
    if (!result.isEmpty()) {
        // 更新命中次数
        QString updateSql = "UPDATE search_cache SET hit_count = hit_count + 1 WHERE cache_key = ?";
        executeL2CacheQuery(updateSql, QVariantList() << key);
        
        logCacheOperation("GET_L2", key, true);
    } else {
        logCacheOperation("GET_L2", key, false);
    }
    
    return result;
}

bool CacheManager::removeL2Cache(const QString& key)
{
    QString sql = "DELETE FROM search_cache WHERE cache_key = ?";
    QVariantList params;
    params << key;
    
    bool success = executeL2CacheQuery(sql, params);
    logCacheOperation("REMOVE_L2", key, success);
    return success;
}

void CacheManager::cleanupL2Cache()
{
    QString sql = "DELETE FROM search_cache WHERE expires_at < NOW()";
    bool success = executeL2CacheQuery(sql);
    
    if (success) {
        // LOG_INFO removed
    } else {
        LOG_ERROR("L2 cache cleanup failed");
    }
}

bool CacheManager::isHotData(const QString& dataType, const QString& dataKey, int threshold)
{
    QMutexLocker locker(&_hotDataMutex);
    
    QString key = QString("%1:%2").arg(dataType, dataKey);
    
    // 检查内存中的热点数据
    if (_hotDataStats.contains(key)) {
        int count = _hotDataStats[key];
        qint64 lastAccess = _hotDataLastAccess.value(key, 0);
        qint64 currentTime = QDateTime::currentSecsSinceEpoch();
        
        // 计算热点数据分数（考虑访问频率和时间衰减）
        double score = calculateHotDataScore(dataType, dataKey);
        
        return score >= threshold;
    }
    
    return false;
}

QJsonArray CacheManager::getHotDataList(const QString& dataType, int limit)
{
    QMutexLocker locker(&_hotDataMutex);
    
    QJsonArray result;
    QList<QPair<QString, double>> scoredData;
    
    // 收集指定类型的数据并计算分数
    for (auto it = _hotDataStats.begin(); it != _hotDataStats.end(); ++it) {
        QString key = it.key();
        if (key.startsWith(dataType + ":")) {
            QString dataKey = key.mid(dataType.length() + 1);
            double score = calculateHotDataScore(dataType, dataKey);
            scoredData.append(qMakePair(dataKey, score));
        }
    }
    
    // 按分数排序
    std::sort(scoredData.begin(), scoredData.end(), 
              [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
                  return a.second > b.second;
              });
    
    // 返回前limit个
    int count = qMin(limit, scoredData.size());
    for (int i = 0; i < count; ++i) {
        QJsonObject item;
        item["key"] = scoredData[i].first;
        item["score"] = scoredData[i].second;
        item["count"] = _hotDataStats[QString("%1:%2").arg(dataType, scoredData[i].first)];
        result.append(item);
    }
    
    return result;
}

void CacheManager::recordHotData(const QString& dataType, const QString& dataKey)
{
    QMutexLocker locker(&_hotDataMutex);
    
    QString key = QString("%1:%2").arg(dataType, dataKey);
    _hotDataStats[key]++;
    _hotDataLastAccess[key] = QDateTime::currentSecsSinceEpoch();
    
    // 异步更新数据库
    QTimer::singleShot(0, [this, dataType, dataKey]() {
        updateHotDataInDatabase(dataType, dataKey);
    });
}

void CacheManager::updateHotDataInDatabase(const QString& dataType, const QString& dataKey)
{
    QString sql = "INSERT INTO hot_data_stats (data_type, data_key, access_count, last_access_at) "
                  "VALUES (?, ?, 1, NOW()) "
                  "ON DUPLICATE KEY UPDATE "
                  "access_count = access_count + 1, "
                  "last_access_at = NOW()";
    
    QVariantList params;
    params << dataType << dataKey;
    
    executeL2CacheQuery(sql, params);
}

void CacheManager::loadHotDataFromDatabase()
{
    QString sql = "SELECT data_type, data_key, access_count, last_access_at "
                  "FROM hot_data_stats "
                  "WHERE last_access_at >= DATE_SUB(NOW(), INTERVAL 1 DAY) "
                  "ORDER BY access_count DESC, last_access_at DESC "
                  "LIMIT 1000";
    
    QJsonObject result = executeL2CacheSelect(sql);
    if (!result.isEmpty()) {
        QMutexLocker locker(&_hotDataMutex);
        
        QJsonArray rows = result["rows"].toArray();
        for (const QJsonValue& row : rows) {
            QJsonObject rowObj = row.toObject();
            QString dataType = rowObj["data_type"].toString();
            QString dataKey = rowObj["data_key"].toString();
            int accessCount = rowObj["access_count"].toInt();
            
            QString key = QString("%1:%2").arg(dataType, dataKey);
            _hotDataStats[key] = accessCount;
            
            // 解析时间戳
            QString timeStr = rowObj["last_access_at"].toString();
            QDateTime lastAccess = QDateTime::fromString(timeStr, Qt::ISODate);
            _hotDataLastAccess[key] = lastAccess.toSecsSinceEpoch();
        }
        
        LOG_INFO(QString("Loaded %1 hot data records from database").arg(rows.size()));
    }
}

double CacheManager::calculateHotDataScore(const QString& dataType, const QString& dataKey)
{
    QString key = QString("%1:%2").arg(dataType, dataKey);
    
    if (!_hotDataStats.contains(key)) {
        return 0.0;
    }
    
    int accessCount = _hotDataStats[key];
    qint64 lastAccess = _hotDataLastAccess.value(key, 0);
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    
    // 时间衰减因子（1小时内访问权重最高，随时间递减）
    double timeDecay = 1.0;
    if (lastAccess > 0) {
        qint64 timeDiff = currentTime - lastAccess;
        if (timeDiff > 3600) { // 超过1小时
            timeDecay = 1.0 / (1.0 + (timeDiff - 3600) / 3600.0); // 每小时衰减50%
        }
    }
    
    // 计算最终分数：访问次数 * 时间衰减因子
    double score = accessCount * timeDecay;
    
    return score;
}

QJsonObject CacheManager::getL2CacheStats()
{
    QString sql = "SELECT "
                  "COUNT(*) as total_entries, "
                  "SUM(hit_count) as total_hits, "
                  "AVG(hit_count) as avg_hits, "
                  "MAX(hit_count) as max_hits, "
                  "COUNT(CASE WHEN expires_at > NOW() THEN 1 END) as active_entries, "
                  "COUNT(CASE WHEN expires_at <= NOW() THEN 1 END) as expired_entries "
                  "FROM search_cache";
    
    QJsonObject result = executeL2CacheSelect(sql);
    
    if (result.isEmpty()) {
        result["total_entries"] = 0;
        result["total_hits"] = 0;
        result["avg_hits"] = 0;
        result["max_hits"] = 0;
        result["active_entries"] = 0;
        result["expired_entries"] = 0;
    }
    
    return result;
}

QJsonArray CacheManager::getHotDataStats(const QString& dataType, int limit)
{
    QMutexLocker locker(&_hotDataMutex);
    
    QJsonArray result;
    QList<QPair<QString, int>> sortedData;
    
    // 收集指定类型的热点数据
    for (auto it = _hotDataStats.begin(); it != _hotDataStats.end(); ++it) {
        QString key = it.key();
        if (key.startsWith(dataType + ":")) {
            QString dataKey = key.mid(dataType.length() + 1);
            sortedData.append(qMakePair(dataKey, it.value()));
        }
    }
    
    // 按访问次数排序
    std::sort(sortedData.begin(), sortedData.end(), 
              [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
                  return a.second > b.second;
              });
    
    // 返回前limit个
    int count = qMin(limit, sortedData.size());
    for (int i = 0; i < count; ++i) {
        QJsonObject item;
        item["key"] = sortedData[i].first;
        item["count"] = sortedData[i].second;
        result.append(item);
    }
    
    return result;
}

QJsonObject CacheManager::getCacheStats()
{
    QMutexLocker locker(&_cacheMutex);
    
    QJsonObject stats;
    stats["total_entries"] = _memoryCache.size();
    stats["memory_usage"] = _memoryCache.size() * 1024; // 估算内存使用量
    
    // 计算过期条目数
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    int expiredCount = 0;
    for (auto it = _cacheExpiry.begin(); it != _cacheExpiry.end(); ++it) {
        if (it.value() < currentTime) {
            expiredCount++;
        }
    }
    stats["expired_entries"] = expiredCount;
    
    return stats;
}

void CacheManager::cleanupExpiredCache()
{
    QMutexLocker locker(&_cacheMutex);
    
    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
    QStringList expiredKeys;
    
    for (auto it = _cacheExpiry.begin(); it != _cacheExpiry.end(); ++it) {
        if (it.value() < currentTime) {
            expiredKeys.append(it.key());
        }
    }
    
    for (const QString& key : expiredKeys) {
        _memoryCache.remove(key);
        _cacheExpiry.remove(key);
    }
    
    if (!expiredKeys.isEmpty()) {
        LOG_INFO(QString("Cleaned up %1 expired cache entries").arg(expiredKeys.size()));
    }
}

QString CacheManager::generateCacheKey(const QString& keyword, qint64 userId)
{
    QString combined = QString("%1:%2:%3").arg(keyword, QString::number(userId), "search");
    QByteArray hash = QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Md5);
    return QString("search_cache:%1").arg(QString(hash.toHex()));
}

void CacheManager::logCacheOperation(const QString& operation, const QString& key, bool success)
{
    if (success) {
        LOG_DEBUG(QString("Cache %1: %2").arg(operation, key));
    } else {
        LOG_DEBUG(QString("Cache %1 failed: %2").arg(operation, key));
    }
}

void CacheManager::onCleanupTimer()
{
    cleanupExpiredCache();
}

void CacheManager::onL2CleanupTimer()
{
    cleanupL2Cache();
}

void CacheManager::onHotDataTimer()
{
    loadHotDataFromDatabase();
}

bool CacheManager::executeL2CacheQuery(const QString& sql, const QVariantList& params)
{
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for L2 cache");
        return false;
    }
    
    int result = dbConn.executeUpdate(sql, params);
    if (result == -1) {
        LOG_ERROR("L2 cache query failed");
        return false;
    }
    
    return true;
}

QJsonObject CacheManager::executeL2CacheSelect(const QString& sql, const QVariantList& params)
{
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for L2 cache select");
        return QJsonObject();
    }
    
    QSqlQuery query = dbConn.executeQuery(sql, params);
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("L2 cache select failed: %1").arg(query.lastError().text()));
        return QJsonObject();
    }
    
    QJsonObject result;
    QJsonArray rows;
    
    while (query.next()) {
        QJsonObject row;
        QSqlRecord record = query.record();
        
        for (int i = 0; i < record.count(); ++i) {
            QString fieldName = record.fieldName(i);
            QVariant value = query.value(i);
            
            if (value.type() == QVariant::String) {
                row[fieldName] = value.toString();
            } else if (value.type() == QVariant::Int) {
                row[fieldName] = value.toInt();
            } else if (value.type() == QVariant::LongLong) {
                row[fieldName] = value.toLongLong();
            } else if (value.type() == QVariant::Double) {
                row[fieldName] = value.toDouble();
            } else {
                row[fieldName] = value.toString();
            }
        }
        
        rows.append(row);
    }
    
    result["rows"] = rows;
    result["count"] = rows.size();
    
    return result;
}
