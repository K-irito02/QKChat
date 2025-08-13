#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <memory>

// 前向声明
class QSqlDatabase;

class CacheManager : public QObject
{
    Q_OBJECT

public:
    static CacheManager* instance();
    
    // 缓存操作
    bool setCache(const QString& key, const QJsonObject& data, int ttlSeconds = 300);
    QJsonObject getCache(const QString& key);
    bool removeCache(const QString& key);
    void clearCache();
    
    // 搜索缓存专用方法
    bool setSearchCache(const QString& keyword, qint64 userId, const QJsonArray& results, int ttlSeconds = 300);
    QJsonArray getSearchCache(const QString& keyword, qint64 userId);
    
    // L2缓存（数据库缓存）方法
    bool setL2Cache(const QString& key, const QJsonObject& data, int ttlSeconds = 1800);
    QJsonObject getL2Cache(const QString& key);
    bool removeL2Cache(const QString& key);
    void cleanupL2Cache();
    
    // 热点数据统计和识别
    void recordHotData(const QString& dataType, const QString& dataKey);
    QJsonArray getHotDataStats(const QString& dataType, int limit = 100);
    bool isHotData(const QString& dataType, const QString& dataKey, int threshold = 10);
    QJsonArray getHotDataList(const QString& dataType, int limit = 50);
    
    // 缓存统计
    QJsonObject getCacheStats();
    QJsonObject getL2CacheStats();

private:
    explicit CacheManager(QObject *parent = nullptr);
    ~CacheManager();
    
    static CacheManager* _instance;
    static QMutex _instanceMutex;
    
    // 内存缓存（L1缓存）
    QMap<QString, QJsonObject> _memoryCache;
    QMap<QString, qint64> _cacheExpiry;
    QMutex _cacheMutex;
    
    // 热点数据统计
    QMap<QString, int> _hotDataStats;
    QMap<QString, qint64> _hotDataLastAccess;
    QMutex _hotDataMutex;
    
    // 清理定时器
    QTimer* _cleanupTimer;
    QTimer* _l2CleanupTimer;
    QTimer* _hotDataTimer;
    
    // 私有方法
    void cleanupExpiredCache();
    QString generateCacheKey(const QString& keyword, qint64 userId);
    void logCacheOperation(const QString& operation, const QString& key, bool success);
    
    // L2缓存私有方法
    QSqlDatabase getDatabase();
    bool executeL2CacheQuery(const QString& sql, const QVariantList& params = QVariantList());
    QJsonObject executeL2CacheSelect(const QString& sql, const QVariantList& params = QVariantList());
    
    // 热点数据识别算法
    void updateHotDataInDatabase(const QString& dataType, const QString& dataKey);
    void loadHotDataFromDatabase();
    double calculateHotDataScore(const QString& dataType, const QString& dataKey);

private slots:
    void onCleanupTimer();
    void onL2CleanupTimer();
    void onHotDataTimer();
};

#endif // CACHEMANAGER_H
