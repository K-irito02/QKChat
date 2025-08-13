#ifndef RATELIMITMANAGER_H
#define RATELIMITMANAGER_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include <QAtomicInt>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

struct TokenBucket {
    int tokens;
    int maxTokens;
    qint64 lastRefillTime;
    
    TokenBucket() : tokens(0), maxTokens(0), lastRefillTime(0) {}
    TokenBucket(int max) : tokens(max), maxTokens(max), lastRefillTime(QDateTime::currentSecsSinceEpoch()) {}
    
    // 允许复制构造函数和赋值操作符
    TokenBucket(const TokenBucket& other) = default;
    TokenBucket& operator=(const TokenBucket& other) = default;
};

struct RateLimitInfo {
    int requestCount;
    qint64 windowStart;
    qint64 windowEnd;
    TokenBucket tokenBucket;
    
    RateLimitInfo() : requestCount(0), windowStart(0), windowEnd(0) {}
    
    // 允许复制构造函数和赋值操作符
    RateLimitInfo(const RateLimitInfo& other) = default;
    RateLimitInfo& operator=(const RateLimitInfo& other) = default;
};

struct RateLimitConfig {
    int maxRequests;
    int windowSeconds;
    int burstSize;
    double tokensPerSecond;
    int maxTokens;
    int refillInterval;
    bool enabled;
    
    RateLimitConfig() : maxRequests(10), windowSeconds(60), burstSize(5), 
                       tokensPerSecond(0.167), maxTokens(10), refillInterval(1000), enabled(true) {}
};

class RateLimitManager : public QObject
{
    Q_OBJECT

public:
    static RateLimitManager* instance();
    
    bool checkRateLimit(const QString& identifier, const QString& endpoint, qint64 userId = 0);
    void resetRateLimit(const QString& identifier, const QString& endpoint);
    QJsonObject getRateLimitStats();
    
    // 令牌桶相关方法
    bool consumeToken(const QString& identifier, const QString& endpoint);
    int getAvailableTokens(const QString& identifier, const QString& endpoint);
    void refillTokens(const QString& identifier, const QString& endpoint);

private:
    explicit RateLimitManager(QObject *parent = nullptr);
    ~RateLimitManager();
    
    static RateLimitManager* _instance;
    static QMutex _instanceMutex;
    
    QMap<QString, RateLimitInfo> _rateLimitMap;
    QMap<QString, RateLimitConfig> _configs;
    QMutex _mutex;
    QTimer* _cleanupTimer;
    QTimer* _tokenRefillTimer;
    
    void cleanupExpiredEntries();
    void refillAllTokenBuckets();
    bool tryConsumeToken(const QString& identifier, const QString& endpoint);
    void refillTokenBucket(const QString& identifier, const QString& endpoint);
    int calculateAvailableTokens(const QString& identifier, const QString& endpoint);
    void logTokenBucketEvent(const QString& identifier, const QString& endpoint, bool success);
    void logRateLimitEvent(const QString& identifier, const QString& endpoint, bool blocked, qint64 userId);
    void logRateLimitToDatabase(const QString& identifier, const QString& endpoint, bool blocked);

private slots:
    void onCleanupTimer();
    void onTokenRefillTimer();
};

#endif // RATELIMITMANAGER_H
