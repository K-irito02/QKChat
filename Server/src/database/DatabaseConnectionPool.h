#ifndef DATABASECONNECTIONPOOL_H
#define DATABASECONNECTIONPOOL_H

#include <QObject>
#include <QSqlDatabase>
#include <QQueue>
#include <QSet>
#include <QMap>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QThread>
#include <QAtomicInt>
#include <QDateTime>
#include <QJsonObject>

/**
 * @brief 数据库连接池类
 * 
 * 提供高效的数据库连接管理，支持连接复用、自动重连、连接健康检查等功能。
 * 解决单连接模式下的性能瓶颈问题。
 */
class DatabaseConnectionPool : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 连接池配置结构
     */
    struct PoolConfig {
        QString host = "localhost";
        int port = 3306;
        QString database = "qkchat";
        QString username = "root";
        QString password = "";
        int minConnections = 5;      // 最小连接数
        int maxConnections = 20;     // 最大连接数
        int acquireTimeout = 5000;   // 获取连接超时时间(ms)
        int idleTimeout = 300000;    // 连接空闲超时时间(ms)
        int healthCheckInterval = 60000; // 健康检查间隔(ms)
        // 动态调整参数
        bool enableAutoResize = true;    // 是否启用自动调整
        int targetUtilization = 70;      // 目标利用率百分比
        int resizeCheckInterval = 30000; // 调整检查间隔(ms)
        double loadPredictionWindow = 300.0; // 负载预测窗口(秒)
    };

    explicit DatabaseConnectionPool(QObject *parent = nullptr);
    ~DatabaseConnectionPool();
    
    /**
     * @brief 获取单例实例
     */
    static DatabaseConnectionPool* instance();
    
    /**
     * @brief 初始化连接池
     * @param config 连接池配置
     * @return 初始化是否成功
     */
    bool initialize(const PoolConfig& config);
    
    /**
     * @brief 获取数据库连接
     * @param timeoutMs 超时时间(毫秒)
     * @return 数据库连接，获取失败返回无效连接
     */
    QSqlDatabase acquireConnection(int timeoutMs = 5000);
    
    /**
     * @brief 释放数据库连接
     * @param connection 要释放的连接
     */
    void releaseConnection(const QSqlDatabase& connection);
    
    /**
     * @brief 关闭连接池
     */
    void shutdown();
    
    /**
     * @brief 获取连接池统计信息
     */
    QJsonObject getStatistics() const;
    
    /**
     * @brief 获取详细性能指标
     */
    QJsonObject getPerformanceMetrics() const;
    
    /**
     * @brief 获取连接池健康状态
     */
    QJsonObject getHealthStatus() const;
    
    /**
     * @brief 获取连接池告警信息
     */
    QJsonArray getAlerts() const;
    
    /**
     * @brief 检查连接池是否健康
     */
    bool isHealthy() const;
    
    /**
     * @brief 检查连接池是否正在关闭
     */
    bool isShuttingDown() const;
    
    /**
     * @brief 动态调整连接池大小
     * @param minConnections 最小连接数
     * @param maxConnections 最大连接数
     */
    void resizePool(int minConnections, int maxConnections);
    
    /**
     * @brief 获取连接池监控数据
     */
    QJsonObject getMonitoringData() const;
    
    /**
     * @brief 记录性能指标
     */
    void recordPerformanceMetric(const QString& metricName, double value);
    
    /**
     * @brief 启用/禁用自动调整
     */
    bool setAutoResizeEnabled(bool enabled);
    
    /**
     * @brief 分析性能趋势
     */
    QJsonObject analyzePerformanceTrend() const;
    
    /**
     * @brief 获取负载预测
     */
    QJsonObject getLoadPrediction() const;
    
    /**
     * @brief 手动触发调整检查
     */
    void triggerResizeCheck();

signals:
    void connectionPoolError(const QString& error);
    void connectionPoolWarning(const QString& warning);
    void poolResized(int oldMin, int oldMax, int newMin, int newMax);

private slots:
    void performHealthCheck();
    void cleanupIdleConnections();
    void performResizeCheck();
    void updatePerformanceMetrics();

private:
    /**
     * @brief 创建新的数据库连接
     */
    QSqlDatabase createConnection();
    
    /**
     * @brief 验证连接是否有效
     */
    bool validateConnection(const QSqlDatabase& connection);
    
    /**
     * @brief 生成唯一连接名
     */
    QString generateConnectionName();
    
    /**
     * @brief 扩展连接池
     */
    void expandPool();
    
    /**
     * @brief 收缩连接池
     */
    void shrinkPool();
    
    /**
     * @brief 智能动态调整算法
     */
    void performSmartResize();
    
    /**
     * @brief 计算当前利用率
     */
    double calculateUtilization() const;
    
    /**
     * @brief 预测未来负载
     */
    double predictFutureLoad() const;
    
    /**
     * @brief 计算最优连接数
     */
    int calculateOptimalConnections() const;
    
    /**
     * @brief 记录连接池状态到数据库
     */
    void recordPoolStatsToDatabase();
    
    /**
     * @brief 生成告警信息
     */
    void generateAlerts(double utilization, int waitingRequests);

private:
    static DatabaseConnectionPool* s_instance;
    static QMutex s_instanceMutex;
    
    PoolConfig _config;
    
    // 连接管理
    QQueue<QSqlDatabase> _availableConnections;  // 可用连接队列
    QSet<QString> _usedConnections;              // 已使用连接集合
    QMap<QString, QDateTime> _connectionLastUsed; // 连接最后使用时间
    
    // 线程同步
    mutable QMutex _poolMutex;
    QWaitCondition _connectionAvailable;
    
    // 统计信息
    QAtomicInt _totalConnections;
    QAtomicInt _activeConnections;
    QAtomicInt _totalAcquired;
    QAtomicInt _totalReleased;
    QAtomicInt _acquireTimeouts;
    
    // 定时器
    QTimer* _healthCheckTimer;
    QTimer* _cleanupTimer;
    QTimer* _monitoringTimer;
    QTimer* _resizeTimer;
    QTimer* _metricsTimer;
    
    // 性能监控
    mutable QMutex _metricsMutex;
    QMap<QString, QList<double>> _performanceMetrics;
    QMap<QString, QDateTime> _lastMetricTime;
    QList<double> _utilizationHistory;
    QList<double> _responseTimeHistory;
    QList<int> _waitingRequestsHistory;
    QList<double> _connectionAcquireTimes;
    QList<double> _queryExecutionTimes;
    QMap<QString, int> _errorCounts;
    QList<QJsonObject> _alerts;
    
    // 动态调整
    mutable QMutex _resizeMutex;
    bool _autoResizeEnabled;
    int _targetUtilization; // 目标利用率百分比
    QDateTime _lastResizeTime;
    int _resizeCount;
    int _excessConnections; // 多余的连接数（用于清理）
    
    // 负载预测
    mutable QMutex _predictionMutex;
    QList<QPair<QDateTime, double>> _loadHistory;
    double _loadPredictionWindow;
    
    bool _initialized;
    bool _shuttingDown;
};

/**
 * @brief 数据库连接RAII包装器
 * 
 * 自动管理数据库连接的获取和释放，确保连接不会泄漏
 */
class DatabaseConnection
{
public:
    explicit DatabaseConnection(int timeoutMs = 5000);
    ~DatabaseConnection();
    
    /**
     * @brief 获取数据库连接
     */
    QSqlDatabase& database() { return _connection; }
    
    /**
     * @brief 检查连接是否有效
     */
    bool isValid() const { return _connection.isValid() && _connection.isOpen(); }
    
    /**
     * @brief 执行查询
     */
    QSqlQuery executeQuery(const QString& sql, const QVariantList& params = QVariantList());
    
    /**
     * @brief 执行更新
     */
    int executeUpdate(const QString& sql, const QVariantList& params = QVariantList());
    
    /**
     * @brief 执行返回单个值的查询
     */
    QVariant executeScalar(const QString& sql, const QVariantList& params = QVariantList());
    
    /**
     * @brief 批量执行查询
     */
    bool executeBatch(const QStringList& sqlList, const QList<QVariantList>& paramsList = QList<QVariantList>());
    
    /**
     * @brief 带重试机制的查询执行
     */
    QSqlQuery executeQueryWithRetry(const QString& sql, const QVariantList& params = QVariantList(), int maxRetries = 3);
    
    /**
     * @brief 带重试机制的更新执行
     */
    int executeUpdateWithRetry(const QString& sql, const QVariantList& params = QVariantList(), int maxRetries = 3);
    
    /**
     * @brief 开始事务
     */
    bool beginTransaction();
    
    /**
     * @brief 提交事务
     */
    bool commitTransaction();
    
    /**
     * @brief 回滚事务
     */
    bool rollbackTransaction();
    
    /**
     * @brief 获取最后执行的错误信息
     */
    QString getLastError() const;
    
    /**
     * @brief 检查连接是否健康
     */
    bool isConnectionHealthy() const;

private:
    QSqlDatabase _connection;
    bool _acquired;
    QString _lastError;
    mutable QMutex _errorMutex;
};

#endif // DATABASECONNECTIONPOOL_H