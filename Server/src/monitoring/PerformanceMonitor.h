#ifndef PERFORMANCEMONITOR_H
#define PERFORMANCEMONITOR_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QDateTime>
#include <QJsonObject>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QQueue>

/**
 * @brief 性能监控类
 * 
 * 提供实时的服务器性能监控，包括CPU使用率、内存使用、
 * 网络连接数、请求响应时间、数据库性能等关键指标。
 */
class PerformanceMonitor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 性能指标结构
     */
    struct PerformanceMetrics {
        // 系统资源
        double cpuUsagePercent;
        qint64 memoryUsedMB;
        qint64 memoryTotalMB;
        double memoryUsagePercent;
        
        // 网络连接
        int activeConnections;
        int totalConnections;
        qint64 bytesReceived;
        qint64 bytesSent;
        
        // 请求统计
        qint64 totalRequests;
        qint64 successfulRequests;
        qint64 failedRequests;
        double averageResponseTime;
        double requestsPerSecond;
        
        // 数据库性能
        int activeDatabaseConnections;
        qint64 totalDatabaseQueries;
        double averageQueryTime;
        qint64 slowQueries;
        
        // 缓存性能
        qint64 cacheHits;
        qint64 cacheMisses;
        double cacheHitRate;
        
        // 消息队列
        int queueSize;
        qint64 processedMessages;
        qint64 failedMessages;
        
        QDateTime timestamp;
        
        PerformanceMetrics() {
            cpuUsagePercent = 0.0;
            memoryUsedMB = 0;
            memoryTotalMB = 0;
            memoryUsagePercent = 0.0;
            activeConnections = 0;
            totalConnections = 0;
            bytesReceived = 0;
            bytesSent = 0;
            totalRequests = 0;
            successfulRequests = 0;
            failedRequests = 0;
            averageResponseTime = 0.0;
            requestsPerSecond = 0.0;
            activeDatabaseConnections = 0;
            totalDatabaseQueries = 0;
            averageQueryTime = 0.0;
            slowQueries = 0;
            cacheHits = 0;
            cacheMisses = 0;
            cacheHitRate = 0.0;
            queueSize = 0;
            processedMessages = 0;
            failedMessages = 0;
            timestamp = QDateTime::currentDateTime();
        }
    };

    explicit PerformanceMonitor(QObject *parent = nullptr);
    ~PerformanceMonitor();
    
    /**
     * @brief 获取单例实例
     * @return PerformanceMonitor实例
     */
    static PerformanceMonitor* instance();
    
    /**
     * @brief 初始化性能监控
     * @param monitoringIntervalMs 监控间隔（毫秒）
     * @param historySize 历史数据保存数量
     * @return 初始化是否成功
     */
    bool initialize(int monitoringIntervalMs = 5000, int historySize = 720);
    
    /**
     * @brief 开始监控
     */
    void startMonitoring();
    
    /**
     * @brief 停止监控
     */
    void stopMonitoring();
    
    /**
     * @brief 获取当前性能指标
     * @return 性能指标
     */
    PerformanceMetrics getCurrentMetrics() const;
    
    /**
     * @brief 获取历史性能数据
     * @param minutes 获取最近多少分钟的数据
     * @return 历史数据列表
     */
    QList<PerformanceMetrics> getHistoryMetrics(int minutes = 60) const;
    
    /**
     * @brief 获取性能统计JSON
     * @return 性能统计JSON对象
     */
    QJsonObject getPerformanceStatistics() const;
    
    // 请求统计方法
    /**
     * @brief 记录请求开始
     * @return 请求ID
     */
    qint64 recordRequestStart();
    
    /**
     * @brief 记录请求完成
     * @param requestId 请求ID
     * @param success 是否成功
     */
    void recordRequestEnd(qint64 requestId, bool success = true);
    
    /**
     * @brief 记录数据库查询
     * @param queryTimeMs 查询时间（毫秒）
     * @param isSlow 是否为慢查询
     */
    void recordDatabaseQuery(qint64 queryTimeMs, bool isSlow = false);
    
    /**
     * @brief 记录缓存访问
     * @param hit 是否命中
     */
    void recordCacheAccess(bool hit);
    
    /**
     * @brief 记录网络流量
     * @param bytesReceived 接收字节数
     * @param bytesSent 发送字节数
     */
    void recordNetworkTraffic(qint64 bytesReceived, qint64 bytesSent);
    
    /**
     * @brief 更新连接数
     * @param activeConnections 当前活跃连接数
     */
    void updateConnectionCount(int activeConnections);
    
    /**
     * @brief 更新队列大小
     * @param queueSize 当前队列大小
     */
    void updateQueueSize(int queueSize);
    
    /**
     * @brief 记录消息处理
     * @param success 是否成功
     */
    void recordMessageProcessed(bool success = true);

signals:
    /**
     * @brief 性能警告信号
     * @param metric 指标名称
     * @param value 当前值
     * @param threshold 阈值
     */
    void performanceWarning(const QString& metric, double value, double threshold);
    
    /**
     * @brief 性能指标更新信号
     * @param metrics 最新性能指标
     */
    void metricsUpdated(const PerformanceMetrics& metrics);

private slots:
    /**
     * @brief 收集性能数据
     */
    void collectMetrics();

private:
    /**
     * @brief 获取CPU使用率
     * @return CPU使用率百分比
     */
    double getCpuUsage();
    
    /**
     * @brief 获取内存使用情况
     * @param usedMB 已使用内存（MB）
     * @param totalMB 总内存（MB）
     * @return 内存使用率百分比
     */
    double getMemoryUsage(qint64& usedMB, qint64& totalMB);
    
    /**
     * @brief 计算平均响应时间
     * @return 平均响应时间（毫秒）
     */
    double calculateAverageResponseTime();
    
    /**
     * @brief 计算每秒请求数
     * @return 每秒请求数
     */
    double calculateRequestsPerSecond();
    
    /**
     * @brief 检查性能阈值
     * @param metrics 性能指标
     */
    void checkPerformanceThresholds(const PerformanceMetrics& metrics);

private:
    static PerformanceMonitor* s_instance;
    
    // 监控配置
    int _monitoringInterval;
    int _historySize;
    bool _isMonitoring;
    
    // 定时器
    QTimer* _monitoringTimer;
    
    // 性能数据
    mutable QMutex _metricsMutex;
    PerformanceMetrics _currentMetrics;
    QQueue<PerformanceMetrics> _historyMetrics;
    
    // 请求统计
    mutable QMutex _requestMutex;
    QAtomicInt _nextRequestId;
    QHash<qint64, QElapsedTimer> _activeRequests;
    QQueue<qint64> _recentResponseTimes;
    QQueue<QPair<QDateTime, int>> _requestHistory;
    
    // 数据库统计
    mutable QMutex _dbMutex;
    QQueue<qint64> _recentQueryTimes;
    QAtomicInt _totalQueries;
    QAtomicInt _slowQueryCount;
    
    // 缓存统计
    QAtomicInt _cacheHitCount;
    QAtomicInt _cacheMissCount;
    
    // 网络统计
    QAtomicInt _totalBytesReceived;
    QAtomicInt _totalBytesSent;
    QAtomicInt _currentConnections;
    QAtomicInt _totalConnectionCount;
    
    // 消息队列统计
    QAtomicInt _currentQueueSize;
    QAtomicInt _processedMessageCount;
    QAtomicInt _failedMessageCount;
    
    // 性能阈值
    double _cpuWarningThreshold;
    double _memoryWarningThreshold;
    int _connectionWarningThreshold;
    double _responseTimeWarningThreshold;
    
    // 上次CPU检查时间（用于CPU使用率计算）
    QDateTime _lastCpuCheck;
    qint64 _lastCpuTime;
};

#endif // PERFORMANCEMONITOR_H