#include "DatabaseConnectionPool.h"
#include "../utils/Logger.h"
#include "../utils/DatabaseErrorHandler.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <numeric>
#include <algorithm>
#include <QJsonObject>
#include <QUuid>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <cmath>

// 静态成员初始化
DatabaseConnectionPool* DatabaseConnectionPool::s_instance = nullptr;
QMutex DatabaseConnectionPool::s_instanceMutex;

DatabaseConnectionPool::DatabaseConnectionPool(QObject *parent)
    : QObject(parent)
    , _totalConnections(0)
    , _activeConnections(0)
    , _totalAcquired(0)
    , _totalReleased(0)
    , _acquireTimeouts(0)
    , _initialized(false)
    , _shuttingDown(false)
    , _autoResizeEnabled(true)
    , _targetUtilization(70)
    , _resizeCount(0)
    , _excessConnections(0)
    , _loadPredictionWindow(300.0)
{
    // 创建定时器
    _healthCheckTimer = new QTimer(this);
    _cleanupTimer = new QTimer(this);
    _resizeTimer = new QTimer(this);
    _metricsTimer = new QTimer(this);
    
    connect(_healthCheckTimer, &QTimer::timeout, this, &DatabaseConnectionPool::performHealthCheck);
    connect(_cleanupTimer, &QTimer::timeout, this, &DatabaseConnectionPool::cleanupIdleConnections);
    connect(_resizeTimer, &QTimer::timeout, this, &DatabaseConnectionPool::performResizeCheck);
    connect(_metricsTimer, &QTimer::timeout, this, &DatabaseConnectionPool::updatePerformanceMetrics);
}

DatabaseConnectionPool::~DatabaseConnectionPool()
{
    shutdown();
}

DatabaseConnectionPool* DatabaseConnectionPool::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new DatabaseConnectionPool();
        }
    }
    return s_instance;
}

bool DatabaseConnectionPool::initialize(const PoolConfig& config)
{
    QMutexLocker locker(&_poolMutex);
    
    if (_initialized) {
        LOG_WARNING("Database connection pool already initialized");
        return true;
    }
    
    _config = config;
    _autoResizeEnabled = config.enableAutoResize;
    _targetUtilization = config.targetUtilization;
    _loadPredictionWindow = config.loadPredictionWindow;
    _shuttingDown = false;
    
    // 初始化数据库连接池
    
    // 创建最小数量的连接
    for (int i = 0; i < _config.minConnections; ++i) {
        QSqlDatabase connection = createConnection();
        if (connection.isValid() && connection.isOpen()) {
            _availableConnections.enqueue(connection);
            _connectionLastUsed[connection.connectionName()] = QDateTime::currentDateTime();
            _totalConnections.fetchAndAddOrdered(1);
        } else {
            LOG_ERROR(QString("Failed to create initial connection %1").arg(i + 1));
            return false;
        }
    }
    
    // 启动定时器
    _healthCheckTimer->start(_config.healthCheckInterval);
    _cleanupTimer->start(_config.idleTimeout / 2); // 清理频率为空闲超时的一半
    
    if (_autoResizeEnabled) {
        _resizeTimer->start(_config.resizeCheckInterval);
        // 自动调整大小已启用
    }
    
    _metricsTimer->start(10000); // 每10秒更新性能指标
    
    _initialized = true;
    
    // 数据库连接池初始化完成
    
    return true;
}

QSqlDatabase DatabaseConnectionPool::acquireConnection(int timeoutMs)
{
    QMutexLocker locker(&_poolMutex);
    
    if (_shuttingDown) {
        LOG_WARNING("Connection pool is shutting down");
        return QSqlDatabase();
    }
    
    QDateTime startTime = QDateTime::currentDateTime();
    
    // 等待可用连接
    while (_availableConnections.isEmpty() && !_shuttingDown) {
        // 尝试扩展连接池
        if (_totalConnections.loadAcquire() < _config.maxConnections) {
            locker.unlock();
            QSqlDatabase newConnection = createConnection();
            locker.relock();
            
            if (newConnection.isValid() && newConnection.isOpen()) {
                _availableConnections.enqueue(newConnection);
                _connectionLastUsed[newConnection.connectionName()] = QDateTime::currentDateTime();
                _totalConnections.fetchAndAddOrdered(1);
                LOG_DEBUG(QString("Created new connection, pool size: %1").arg(_availableConnections.size()));
                break;
            } else {
                LOG_WARNING("Failed to create new database connection");
            }
        }
        
        // 检查超时
        if (startTime.msecsTo(QDateTime::currentDateTime()) >= timeoutMs) {
            _acquireTimeouts.fetchAndAddOrdered(1);
            LOG_WARNING(QString("Connection acquire timeout after %1ms").arg(timeoutMs));
            LOG_WARNING(QString("Pool status - Available: %1, Total: %2, Active: %3")
                       .arg(_availableConnections.size())
                       .arg(_totalConnections.loadAcquire())
                       .arg(_activeConnections.loadAcquire()));
            return QSqlDatabase();
        }
        
        // 等待连接可用
        _connectionAvailable.wait(&_poolMutex, 100);
    }
    
    if (_availableConnections.isEmpty()) {
        LOG_ERROR("No available connections in pool");
        return QSqlDatabase();
    }
    
    // 获取连接
    QSqlDatabase connection = _availableConnections.dequeue();
    _usedConnections.insert(connection.connectionName());
    _activeConnections.fetchAndAddOrdered(1);
    _totalAcquired.fetchAndAddOrdered(1);
    
    // 验证连接有效性
    if (!validateConnection(connection)) {
        LOG_WARNING(QString("Invalid connection detected: %1").arg(connection.connectionName()));
        _usedConnections.remove(connection.connectionName());
        _activeConnections.fetchAndSubOrdered(1);
        
        // 关闭无效连接
        connection.close();
        QSqlDatabase::removeDatabase(connection.connectionName());
        
        // 尝试重新创建连接
        locker.unlock();
        QSqlDatabase newConnection = createConnection();
        locker.relock();
        
        if (newConnection.isValid() && newConnection.isOpen()) {
            _usedConnections.insert(newConnection.connectionName());
            _activeConnections.fetchAndAddOrdered(1);
            LOG_DEBUG(QString("Replaced invalid connection with new one: %1").arg(newConnection.connectionName()));
            return newConnection;
        }
        
        LOG_ERROR("Failed to create replacement connection");
        return QSqlDatabase();
    }
    
    LOG_DEBUG(QString("Acquired connection: %1").arg(connection.connectionName()));
    return connection;
}

void DatabaseConnectionPool::releaseConnection(const QSqlDatabase& connection)
{
    if (!connection.isValid()) {
        return;
    }
    
    QMutexLocker locker(&_poolMutex);
    
    QString connectionName = connection.connectionName();
    
    if (!_usedConnections.contains(connectionName)) {
        LOG_WARNING(QString("Attempting to release unknown connection: %1").arg(connectionName));
        return;
    }
    
    _usedConnections.remove(connectionName);
    _activeConnections.fetchAndSubOrdered(1);
    _totalReleased.fetchAndAddOrdered(1);
    
    // 更新最后使用时间
    _connectionLastUsed[connectionName] = QDateTime::currentDateTime();
    
    // 验证连接状态
    if (validateConnection(connection)) {
        _availableConnections.enqueue(connection);
        _connectionAvailable.wakeOne();
    } else {
        // 连接无效，从池中移除
        LOG_WARNING(QString("Removing invalid connection from pool: %1").arg(connectionName));
        _connectionLastUsed.remove(connectionName);
        _totalConnections.fetchAndSubOrdered(1);
        
        // 如果连接数低于最小值，创建新连接
        if (_totalConnections.loadAcquire() < _config.minConnections) {
            locker.unlock();
            QSqlDatabase newConnection = createConnection();
            locker.relock();
            
            if (newConnection.isValid() && newConnection.isOpen()) {
                _availableConnections.enqueue(newConnection);
                _connectionLastUsed[newConnection.connectionName()] = QDateTime::currentDateTime();
                _totalConnections.fetchAndAddOrdered(1);
                _connectionAvailable.wakeOne();
            }
        }
    }
}

void DatabaseConnectionPool::shutdown()
{
    QMutexLocker locker(&_poolMutex);
    
    if (!_initialized || _shuttingDown) {
        return;
    }
    
    // 关闭数据库连接池
    // LOG_INFO removed
    
    _shuttingDown = true;
    
    // 停止所有定时器
    if (_healthCheckTimer) _healthCheckTimer->stop();
    if (_cleanupTimer) _cleanupTimer->stop();
    if (_resizeTimer) _resizeTimer->stop();
    if (_metricsTimer) _metricsTimer->stop();
    
    // 等待所有连接释放
    int waitCount = 0;
    while (!_usedConnections.isEmpty() && waitCount < 50) { // 最多等待5秒
        locker.unlock();
        QThread::msleep(100);
        locker.relock();
        waitCount++;
    }
    
    // 记录仍在使用的连接
    if (!_usedConnections.isEmpty()) {
        LOG_WARNING(QString("仍有 %1 个活跃连接未释放").arg(_usedConnections.size()));
        
        // 不要在这里调用removeDatabase，这会导致警告
        // 只关闭连接，让Qt在应用程序退出时自行清理
        QSet<QString> connectionNames = _usedConnections;
        for (const QString& connectionName : connectionNames) {
            QSqlDatabase db = QSqlDatabase::database(connectionName);
            if (db.isOpen()) {
                db.close();
            }
        }
    }

    // 关闭所有可用连接
    while (!_availableConnections.isEmpty()) {
        QSqlDatabase connection = _availableConnections.dequeue();
        if (connection.isOpen()) {
            connection.close();
        }
        // 不要在这里调用removeDatabase，这会导致警告
    }

    _connectionLastUsed.clear();
    _usedConnections.clear();
    _totalConnections.storeRelease(0);
    _activeConnections.storeRelease(0);
    
    _initialized = false;
    
    // LOG_INFO removed
}

QJsonObject DatabaseConnectionPool::getStatistics() const
{
    QMutexLocker locker(&_poolMutex);
    
    QJsonObject stats;
    stats["initialized"] = _initialized;
    stats["total_connections"] = _totalConnections.loadAcquire();
    stats["active_connections"] = _activeConnections.loadAcquire();
    stats["available_connections"] = _availableConnections.size();
    stats["total_acquired"] = _totalAcquired.loadAcquire();
    stats["total_released"] = _totalReleased.loadAcquire();
    stats["acquire_timeouts"] = _acquireTimeouts.loadAcquire();
    stats["min_connections"] = _config.minConnections;
    stats["max_connections"] = _config.maxConnections;
    stats["utilization_percent"] = calculateUtilization();
    stats["auto_resize_enabled"] = _autoResizeEnabled;
    stats["target_utilization"] = _targetUtilization;
    
    return stats;
}

QJsonObject DatabaseConnectionPool::getPerformanceMetrics() const
{
    QMutexLocker locker(&_metricsMutex);
    
    QJsonObject metrics;
    
    // 连接获取时间统计
    if (!_connectionAcquireTimes.isEmpty()) {
        QList<double> sortedTimes = _connectionAcquireTimes;
        std::sort(sortedTimes.begin(), sortedTimes.end());
        
        double avgAcquireTime = std::accumulate(_connectionAcquireTimes.begin(), _connectionAcquireTimes.end(), 0.0) / _connectionAcquireTimes.size();
        metrics["acquire_time_avg"] = avgAcquireTime;
        metrics["acquire_time_min"] = sortedTimes.first();
        metrics["acquire_time_max"] = sortedTimes.last();
        metrics["acquire_time_median"] = sortedTimes[sortedTimes.size() / 2];
        metrics["acquire_time_95th"] = sortedTimes[static_cast<int>(sortedTimes.size() * 0.95)];
        metrics["connection_acquire_count"] = _connectionAcquireTimes.size();
    }
    
    // 查询执行时间统计
    if (!_queryExecutionTimes.isEmpty()) {
        QList<double> sortedTimes = _queryExecutionTimes;
        std::sort(sortedTimes.begin(), sortedTimes.end());
        
        double avgQueryTime = std::accumulate(_queryExecutionTimes.begin(), _queryExecutionTimes.end(), 0.0) / _queryExecutionTimes.size();
        metrics["query_time_avg"] = avgQueryTime;
        metrics["query_time_min"] = sortedTimes.first();
        metrics["query_time_max"] = sortedTimes.last();
        metrics["query_time_median"] = sortedTimes[sortedTimes.size() / 2];
        metrics["query_time_95th"] = sortedTimes[static_cast<int>(sortedTimes.size() * 0.95)];
        metrics["query_execution_count"] = _queryExecutionTimes.size();
    }
    
    // 利用率统计
    if (!_utilizationHistory.isEmpty()) {
        QList<double> sortedUtilization = _utilizationHistory;
        std::sort(sortedUtilization.begin(), sortedUtilization.end());
        
        double avgUtilization = std::accumulate(_utilizationHistory.begin(), _utilizationHistory.end(), 0.0) / _utilizationHistory.size();
        metrics["utilization_avg"] = avgUtilization;
        metrics["utilization_min"] = sortedUtilization.first();
        metrics["utilization_max"] = sortedUtilization.last();
        metrics["utilization_median"] = sortedUtilization[sortedUtilization.size() / 2];
        metrics["utilization_95th"] = sortedUtilization[static_cast<int>(sortedUtilization.size() * 0.95)];
    }
    
    // 响应时间历史
    if (!_responseTimeHistory.isEmpty()) {
        QList<double> sortedResponseTimes = _responseTimeHistory;
        std::sort(sortedResponseTimes.begin(), sortedResponseTimes.end());
        
        double avgResponseTime = std::accumulate(_responseTimeHistory.begin(), _responseTimeHistory.end(), 0.0) / _responseTimeHistory.size();
        metrics["response_time_avg"] = avgResponseTime;
        metrics["response_time_min"] = sortedResponseTimes.first();
        metrics["response_time_max"] = sortedResponseTimes.last();
        metrics["response_time_median"] = sortedResponseTimes[sortedResponseTimes.size() / 2];
    }
    
    // 错误统计
    QJsonObject errorStats;
    for (auto it = _errorCounts.begin(); it != _errorCounts.end(); ++it) {
        errorStats[it.key()] = it.value();
    }
    metrics["error_counts"] = errorStats;
    
    // 等待请求历史
    if (!_waitingRequestsHistory.isEmpty()) {
        QList<int> sortedWaiting = _waitingRequestsHistory;
        std::sort(sortedWaiting.begin(), sortedWaiting.end());
        
        double avgWaiting = std::accumulate(_waitingRequestsHistory.begin(), _waitingRequestsHistory.end(), 0) / (double)_waitingRequestsHistory.size();
        metrics["waiting_requests_avg"] = avgWaiting;
        metrics["waiting_requests_max"] = sortedWaiting.last();
        metrics["waiting_requests_median"] = sortedWaiting[sortedWaiting.size() / 2];
    }
    
    return metrics;
}

QJsonObject DatabaseConnectionPool::getHealthStatus() const
{
    QMutexLocker locker(&_poolMutex);
    
    QJsonObject health;
    health["is_healthy"] = isHealthy();
    health["is_shutting_down"] = _shuttingDown;
    health["initialized"] = _initialized;
    
    // 连接池状态
    int totalConnections = _totalConnections.loadAcquire();
    int activeConnections = _activeConnections.loadAcquire();
    int availableConnections = _availableConnections.size();
    
    health["total_connections"] = totalConnections;
    health["active_connections"] = activeConnections;
    health["available_connections"] = availableConnections;
    
    // 健康检查
    health["min_connections_met"] = totalConnections >= _config.minConnections;
    health["max_connections_ok"] = totalConnections <= _config.maxConnections;
    health["utilization_ok"] = totalConnections > 0 ? (activeConnections * 100.0 / totalConnections) < 90 : true;
    
    // 连接质量
    health["connection_quality"] = "good";
    if (totalConnections == 0) {
        health["connection_quality"] = "critical";
    } else if (availableConnections == 0) {
        health["connection_quality"] = "warning";
    }
    
    return health;
}

QJsonArray DatabaseConnectionPool::getAlerts() const
{
    QMutexLocker locker(&_metricsMutex);
    QJsonArray result;
    for (const QJsonObject& alert : _alerts) {
        result.append(alert);
    }
    return result;
}

bool DatabaseConnectionPool::isHealthy() const
{
    QMutexLocker locker(&_poolMutex);
    return _initialized && !_shuttingDown && _totalConnections.loadAcquire() >= _config.minConnections;
}

bool DatabaseConnectionPool::isShuttingDown() const
{
    return _shuttingDown;
}

void DatabaseConnectionPool::performHealthCheck()
{
    QMutexLocker locker(&_poolMutex);
    
    if (_shuttingDown) {
        return;
    }
    

    
    // 检查可用连接的健康状态
    QQueue<QSqlDatabase> healthyConnections;
    int removedConnections = 0;
    
    while (!_availableConnections.isEmpty()) {
        QSqlDatabase connection = _availableConnections.dequeue();
        
        locker.unlock();
        bool isHealthy = validateConnection(connection);
        locker.relock();
        
        if (isHealthy) {
            healthyConnections.enqueue(connection);
        } else {
            LOG_WARNING(QString("Removing unhealthy connection: %1").arg(connection.connectionName()));
            _connectionLastUsed.remove(connection.connectionName());
            removedConnections++;
        }
    }
    
    _availableConnections = healthyConnections;
    _totalConnections.fetchAndSubOrdered(removedConnections);
    
    // 如果连接数不足，创建新连接
    int currentTotal = _totalConnections.loadAcquire();
    if (currentTotal < _config.minConnections) {
        int needed = _config.minConnections - currentTotal;
        LOG_INFO(QString("Creating %1 new connections to maintain minimum pool size").arg(needed));
        
        for (int i = 0; i < needed; ++i) {
            locker.unlock();
            QSqlDatabase newConnection = createConnection();
            locker.relock();
            
            if (newConnection.isValid() && newConnection.isOpen()) {
                _availableConnections.enqueue(newConnection);
                _connectionLastUsed[newConnection.connectionName()] = QDateTime::currentDateTime();
                _totalConnections.fetchAndAddOrdered(1);
            }
        }
    }
}

void DatabaseConnectionPool::cleanupIdleConnections()
{
    QMutexLocker locker(&_poolMutex);
    
    if (_shuttingDown || _totalConnections.loadAcquire() <= _config.minConnections) {
        return;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    QQueue<QSqlDatabase> activeConnections;
    int removedConnections = 0;
    
    // 检查空闲连接
    while (!_availableConnections.isEmpty()) {
        QSqlDatabase connection = _availableConnections.dequeue();
        QString connectionName = connection.connectionName();
        
        QDateTime lastUsed = _connectionLastUsed.value(connectionName, now);
        if (lastUsed.msecsTo(now) > _config.idleTimeout && 
            _totalConnections.loadAcquire() > _config.minConnections) {
            
    
            _connectionLastUsed.remove(connectionName);
            connection.close();
            QSqlDatabase::removeDatabase(connectionName);
            removedConnections++;
        } else {
            activeConnections.enqueue(connection);
        }
    }
    
    _availableConnections = activeConnections;
    _totalConnections.fetchAndSubOrdered(removedConnections);
    
    if (removedConnections > 0) {
    
    }
}

QSqlDatabase DatabaseConnectionPool::createConnection()
{
    QString connectionName = generateConnectionName();
    
    // 检查MySQL驱动是否可用
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        LOG_ERROR("MySQL driver (QMYSQL) is not available");
        LOG_ERROR("Available drivers: " + QSqlDatabase::drivers().join(", "));
        return QSqlDatabase();
    }
    
    QSqlDatabase connection = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    connection.setHostName(_config.host);
    connection.setPort(_config.port);
    connection.setDatabaseName(_config.database);
    connection.setUserName(_config.username);
    connection.setPassword(_config.password);
    
    // 设置连接选项
    // 注意：MYSQL_OPT_RECONNECT已被弃用，使用连接池的健康检查和重连机制替代
    connection.setConnectOptions(
        "MYSQL_OPT_CONNECT_TIMEOUT=5;"
        "MYSQL_OPT_READ_TIMEOUT=10;"
        "MYSQL_OPT_WRITE_TIMEOUT=10"
    );
    
    if (!connection.open()) {
        QString error = connection.lastError().text();
        LOG_ERROR(QString("Failed to create database connection: %1").arg(error));
        LOG_ERROR(QString("Connection details: %1@%2:%3/%4")
                  .arg(_config.username).arg(_config.host).arg(_config.port).arg(_config.database));
        QSqlDatabase::removeDatabase(connectionName);
        return QSqlDatabase();
    }
    
    LOG_DEBUG(QString("Database connection created successfully: %1").arg(connectionName));
    return connection;
}

bool DatabaseConnectionPool::validateConnection(const QSqlDatabase& connection)
{
    if (!connection.isValid() || !connection.isOpen()) {
        return false;
    }
    
    // 执行简单查询测试连接
    QSqlQuery query("SELECT 1", connection);
    return query.exec() && !query.lastError().isValid();
}

QString DatabaseConnectionPool::generateConnectionName()
{
    return QString("QKChat_Pool_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

// DatabaseConnection RAII包装器实现
DatabaseConnection::DatabaseConnection(int timeoutMs)
    : _acquired(false)
{
    _connection = DatabaseConnectionPool::instance()->acquireConnection(timeoutMs);
    _acquired = _connection.isValid() && _connection.isOpen();
}

DatabaseConnection::~DatabaseConnection()
{
    if (_acquired) {
        // 检查连接池是否仍然有效，避免程序退出时的警告
        DatabaseConnectionPool* pool = DatabaseConnectionPool::instance();
        if (pool && !pool->isShuttingDown()) {
            pool->releaseConnection(_connection);
        } else {
            // 连接池正在关闭或已关闭，直接关闭连接
            if (_connection.isOpen()) {
                _connection.close();
            }
            
            // 不要在析构函数中调用removeDatabase，这会导致警告
            // 因为QSqlDatabase可能在其他地方仍被引用
            // 让Qt在应用程序退出时自行清理
        }
    }
}

QSqlQuery DatabaseConnection::executeQuery(const QString& sql, const QVariantList& params)
{
    if (!isValid()) {
        QMutexLocker locker(&_errorMutex);
        _lastError = "Database connection is not valid";
        LOG_ERROR("Cannot execute query: invalid database connection");
        return QSqlQuery();
    }
    
    QSqlQuery query(_connection);
    query.prepare(sql);
    
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    
    if (!query.exec()) {
        QMutexLocker locker(&_errorMutex);
        _lastError = query.lastError().text();
        
        // 使用错误处理器处理错误
        DatabaseErrorHandler* errorHandler = DatabaseErrorHandler::instance();
        if (errorHandler) {
            errorHandler->handleError(query.lastError(), "DatabaseConnection::executeQuery");
        }
        
        LOG_ERROR(QString("Query execution failed: %1").arg(_lastError));
    }
    
    return query;
}

int DatabaseConnection::executeUpdate(const QString& sql, const QVariantList& params)
{
    QSqlQuery query = executeQuery(sql, params);
    if (query.lastError().isValid()) {
        QMutexLocker locker(&_errorMutex);
        _lastError = query.lastError().text();
        return -1;
    }
    return query.numRowsAffected();
}

QVariant DatabaseConnection::executeScalar(const QString& sql, const QVariantList& params)
{
    QSqlQuery query = executeQuery(sql, params);
    if (query.lastError().isValid() || !query.next()) {
        QMutexLocker locker(&_errorMutex);
        _lastError = query.lastError().text();
        return QVariant();
    }
    return query.value(0);
}

bool DatabaseConnection::executeBatch(const QStringList& sqlList, const QList<QVariantList>& paramsList)
{
    if (!isValid()) {
        LOG_ERROR("Cannot execute batch: invalid database connection");
        return false;
    }
    
    if (!beginTransaction()) {
        LOG_ERROR("Failed to begin transaction for batch execution");
        return false;
    }
    
    try {
        for (int i = 0; i < sqlList.size(); ++i) {
            QVariantList params = (i < paramsList.size()) ? paramsList[i] : QVariantList();
            QSqlQuery query = executeQuery(sqlList[i], params);
            
            if (query.lastError().isValid()) {
                QMutexLocker locker(&_errorMutex);
                _lastError = query.lastError().text();
                rollbackTransaction();
                return false;
            }
        }
        
        if (!commitTransaction()) {
            rollbackTransaction();
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        QMutexLocker locker(&_errorMutex);
        _lastError = QString("Batch execution failed: %1").arg(e.what());
        rollbackTransaction();
        return false;
    }
}

QSqlQuery DatabaseConnection::executeQueryWithRetry(const QString& sql, const QVariantList& params, int maxRetries)
{
    QSqlQuery query;
    int retryCount = 0;
    
    while (retryCount < maxRetries) {
        query = executeQuery(sql, params);
        
        if (!query.lastError().isValid()) {
            return query; // 成功执行
        }
        
        // 检查是否为可重试的错误
        QString errorText = query.lastError().text().toLower();
        if (errorText.contains("connection") || errorText.contains("timeout") || 
            errorText.contains("deadlock") || errorText.contains("lock")) {
            
            retryCount++;
            if (retryCount < maxRetries) {
                LOG_WARNING(QString("Database query failed, retrying (%1/%2): %3")
                           .arg(retryCount).arg(maxRetries).arg(errorText));
                QThread::msleep(100 * retryCount); // 指数退避
                continue;
            }
        }
        
        // 不可重试的错误或已达到最大重试次数
        break;
    }
    
    QMutexLocker locker(&_errorMutex);
    _lastError = query.lastError().text();
    return query;
}

int DatabaseConnection::executeUpdateWithRetry(const QString& sql, const QVariantList& params, int maxRetries)
{
    QSqlQuery query = executeQueryWithRetry(sql, params, maxRetries);
    if (query.lastError().isValid()) {
        return -1;
    }
    return query.numRowsAffected();
}

bool DatabaseConnection::beginTransaction()
{
    if (!isValid()) {
        return false;
    }
    return _connection.transaction();
}

bool DatabaseConnection::commitTransaction()
{
    if (!isValid()) {
        return false;
    }
    return _connection.commit();
}

bool DatabaseConnection::rollbackTransaction()
{
    if (!isValid()) {
        return false;
    }
    return _connection.rollback();
}

QString DatabaseConnection::getLastError() const
{
    QMutexLocker locker(&_errorMutex);
    return _lastError;
}

bool DatabaseConnection::isConnectionHealthy() const
{
    if (!isValid()) {
        return false;
    }
    
    // 执行简单查询测试连接
    QSqlQuery query("SELECT 1", _connection);
    return query.exec() && !query.lastError().isValid();
}

QJsonObject DatabaseConnectionPool::getLoadPrediction() const
{
    QMutexLocker locker(&_predictionMutex);
    
    QJsonObject prediction;
    prediction["current_load"] = calculateUtilization();
    prediction["predicted_load"] = predictFutureLoad();
    prediction["optimal_connections"] = calculateOptimalConnections();
    prediction["prediction_window"] = _loadPredictionWindow;
    
    // 添加历史数据
    QJsonArray historyArray;
    for (const auto& entry : _loadHistory) {
        QJsonObject historyEntry;
        historyEntry["timestamp"] = entry.first.toString(Qt::ISODate);
        historyEntry["load"] = entry.second;
        historyArray.append(historyEntry);
    }
    prediction["load_history"] = historyArray;
    
    return prediction;
}

void DatabaseConnectionPool::triggerResizeCheck()
{
    if (_autoResizeEnabled) {
        performResizeCheck();
    }
}

void DatabaseConnectionPool::performResizeCheck()
{
    if (!_autoResizeEnabled || _shuttingDown) {
        return;
    }
    
    QMutexLocker locker(&_resizeMutex);
    
    // 检查是否频繁调整（避免过于频繁的调整）
    QDateTime now = QDateTime::currentDateTime();
    if (_lastResizeTime.isValid() && _lastResizeTime.msecsTo(now) < 60000) { // 至少间隔1分钟
        return;
    }
    
    performSmartResize();
}

void DatabaseConnectionPool::performSmartResize()
{
    double currentUtilization = calculateUtilization();
    double predictedLoad = predictFutureLoad();
    int optimalConnections = calculateOptimalConnections();
    
    int currentTotal = _totalConnections.loadAcquire();
    int currentMin = _config.minConnections;
    int currentMax = _config.maxConnections;
    
    // 智能调整分析
    
    // 计算新的连接池大小
    int newMin = qMax(5, qMin(optimalConnections - 2, currentMax - 5));
    int newMax = qMax(newMin + 5, qMin(optimalConnections + 5, 50)); // 最大50个连接
    
    // 检查是否需要调整
    bool shouldResize = false;
    QString resizeReason;
    
    if (currentUtilization > _targetUtilization + 10) {
        // 利用率过高，需要扩容
        shouldResize = true;
        resizeReason = "High utilization";
    } else if (currentUtilization < _targetUtilization - 20 && currentTotal > _config.minConnections) {
        // 利用率过低，可以缩容
        shouldResize = true;
        resizeReason = "Low utilization";
    } else if (predictedLoad > currentMax * 0.8) {
        // 预测负载较高，提前扩容
        shouldResize = true;
        resizeReason = "High predicted load";
    }
    
    if (shouldResize && (newMin != currentMin || newMax != currentMax)) {
        // 调整连接池大小
        
        // 执行调整
        _config.minConnections = newMin;
        _config.maxConnections = newMax;
        
        // 如果当前连接数不足，立即创建新连接
        if (currentTotal < newMin) {
            int needed = newMin - currentTotal;
            for (int i = 0; i < needed; ++i) {
                QSqlDatabase newConnection = createConnection();
                if (newConnection.isValid() && newConnection.isOpen()) {
                    _availableConnections.enqueue(newConnection);
                    _connectionLastUsed[newConnection.connectionName()] = QDateTime::currentDateTime();
                    _totalConnections.fetchAndAddOrdered(1);
                }
            }
        }
        
        _lastResizeTime = QDateTime::currentDateTime();
        _resizeCount++;
        
        // 记录到数据库
        recordPoolStatsToDatabase();
        
        // 发送信号
        emit poolResized(currentMin, currentMax, newMin, newMax);
    }
}

double DatabaseConnectionPool::calculateUtilization() const
{
    int total = _totalConnections.loadAcquire();
    int active = _activeConnections.loadAcquire();
    
    if (total == 0) return 0.0;
    return (double)active / total * 100.0;
}

double DatabaseConnectionPool::predictFutureLoad() const
{
    QMutexLocker locker(&_predictionMutex);
    
    if (_loadHistory.size() < 3) {
        return calculateUtilization(); // 数据不足，返回当前利用率
    }
    
    // 简单的线性回归预测
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = _loadHistory.size();
    
    for (int i = 0; i < n; ++i) {
        double x = i; // 时间点
        double y = _loadHistory[i].second; // 负载值
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }
    
    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double intercept = (sumY - slope * sumX) / n;
    
    // 预测下一个时间点的负载
    double predictedLoad = slope * n + intercept;
    
    // 确保预测值在合理范围内
    return qMax(0.0, qMin(100.0, predictedLoad));
}

int DatabaseConnectionPool::calculateOptimalConnections() const
{
    double currentLoad = calculateUtilization();
    double predictedLoad = predictFutureLoad();
    
    // 基于当前和预测负载计算最优连接数
    double targetLoad = qMax(currentLoad, predictedLoad);
    
    // 基础连接数
    int baseConnections = 10;
    
    // 根据负载调整
    if (targetLoad > 80) {
        baseConnections = 25; // 高负载
    } else if (targetLoad > 60) {
        baseConnections = 20; // 中等负载
    } else if (targetLoad > 40) {
        baseConnections = 15; // 低负载
    }
    
    // 考虑历史趋势
    QMutexLocker locker(&_metricsMutex);
    if (_utilizationHistory.size() >= 5) {
        double avgUtilization = 0;
        for (int i = _utilizationHistory.size() - 5; i < _utilizationHistory.size(); ++i) {
            avgUtilization += _utilizationHistory[i];
        }
        avgUtilization /= 5;
        
        if (avgUtilization > 70) {
            baseConnections += 5; // 历史利用率高，增加连接数
        } else if (avgUtilization < 30) {
            baseConnections = qMax(5, baseConnections - 3); // 历史利用率低，减少连接数
        }
    }
    
    return baseConnections;
}

void DatabaseConnectionPool::recordPoolStatsToDatabase()
{
    // 使用RAII包装器自动管理数据库连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        LOG_ERROR("Failed to acquire database connection for pool stats recording");
        return;
    }
    
    int result = dbConn.executeUpdate(
        "INSERT INTO connection_pool_stats "
        "(pool_name, total_connections, active_connections, idle_connections, "
        "waiting_requests, connection_timeout_count, connection_error_count, "
        "avg_connection_time, max_connection_time, recorded_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, NOW())",
        {
            "main_pool",
            _totalConnections.loadAcquire(),
            _activeConnections.loadAcquire(),
            _availableConnections.size(),
            0, // 等待请求数（需要额外统计）
            _acquireTimeouts.loadAcquire(),
            0, // 连接错误数（需要额外统计）
            0.0, // 平均连接时间（需要额外统计）
            0.0  // 最大连接时间（需要额外统计）
        }
    );
    
    if (result == -1) {
        LOG_ERROR(QString("Failed to record pool stats: %1").arg(dbConn.getLastError()));
    }
}

QJsonObject DatabaseConnectionPool::analyzePerformanceTrend() const
{
    QMutexLocker locker(&_metricsMutex);
    
    QJsonObject analysis;
    
    // 分析利用率趋势
    if (_utilizationHistory.size() >= 10) {
        double recentAvg = 0, olderAvg = 0;
        int halfSize = _utilizationHistory.size() / 2;
        
        for (int i = 0; i < halfSize; ++i) {
            olderAvg += _utilizationHistory[i];
        }
        for (int i = halfSize; i < _utilizationHistory.size(); ++i) {
            recentAvg += _utilizationHistory[i];
        }
        
        olderAvg /= halfSize;
        recentAvg /= (_utilizationHistory.size() - halfSize);
        
        analysis["utilization_trend"] = recentAvg > olderAvg ? "increasing" : "decreasing";
        analysis["utilization_change"] = recentAvg - olderAvg;
    }
    
    // 分析响应时间趋势
    if (_responseTimeHistory.size() >= 5) {
        double avgResponseTime = 0;
        for (double time : _responseTimeHistory) {
            avgResponseTime += time;
        }
        avgResponseTime /= _responseTimeHistory.size();
        
        analysis["avg_response_time"] = avgResponseTime;
        analysis["response_time_trend"] = avgResponseTime > 1000 ? "slow" : "normal";
    }
    
    return analysis;
}

void DatabaseConnectionPool::updatePerformanceMetrics()
{
    QMutexLocker locker(&_metricsMutex);
    
    // 记录当前利用率
    double currentUtilization = calculateUtilization();
    _utilizationHistory.append(currentUtilization);
    
    // 保持历史记录在合理范围内
    if (_utilizationHistory.size() > 100) {
        _utilizationHistory.removeFirst();
    }
    
    // 记录响应时间（这里简化处理，实际应该从具体的查询中获取）
    _responseTimeHistory.append(0.0); // 占位符
    if (_responseTimeHistory.size() > 100) {
        _responseTimeHistory.removeFirst();
    }
    
    // 记录等待请求数
    int waitingRequests = _totalAcquired.loadAcquire() - _totalReleased.loadAcquire();
    _waitingRequestsHistory.append(waitingRequests);
    if (_waitingRequestsHistory.size() > 100) {
        _waitingRequestsHistory.removeFirst();
    }
    
    // 记录负载历史
    QMutexLocker predLocker(&_predictionMutex);
    _loadHistory.append(qMakePair(QDateTime::currentDateTime(), currentUtilization));
    
    // 清理过期的负载历史
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-_loadPredictionWindow);
    while (!_loadHistory.isEmpty() && _loadHistory.first().first < cutoff) {
        _loadHistory.removeFirst();
    }
    
    // 生成告警
    generateAlerts(currentUtilization, waitingRequests);
    
    // 记录性能指标到数据库
    QTimer::singleShot(0, [this]() {
        recordPoolStatsToDatabase();
    });
}

void DatabaseConnectionPool::generateAlerts(double utilization, int waitingRequests)
{
    QDateTime now = QDateTime::currentDateTime();
    
    // 高利用率告警
    if (utilization > 90.0) {
        QJsonObject alert;
        alert["type"] = "high_utilization";
        alert["severity"] = "critical";
        alert["message"] = QString("连接池利用率过高: %1%").arg(utilization, 0, 'f', 2);
        alert["timestamp"] = now.toString(Qt::ISODate);
        alert["value"] = utilization;
        alert["threshold"] = 90.0;
        _alerts.append(alert);
    }
    
    // 连接获取超时告警
    if (_acquireTimeouts.loadAcquire() > 10) {
        QJsonObject alert;
        alert["type"] = "acquire_timeout";
        alert["severity"] = "warning";
        alert["message"] = QString("连接获取超时次数过多: %1").arg(_acquireTimeouts.loadAcquire());
        alert["timestamp"] = now.toString(Qt::ISODate);
        alert["value"] = _acquireTimeouts.loadAcquire();
        alert["threshold"] = 10;
        _alerts.append(alert);
    }
    
    // 等待请求过多告警
    if (waitingRequests > 50) {
        QJsonObject alert;
        alert["type"] = "high_waiting_requests";
        alert["severity"] = "warning";
        alert["message"] = QString("等待请求数量过多: %1").arg(waitingRequests);
        alert["timestamp"] = now.toString(Qt::ISODate);
        alert["value"] = waitingRequests;
        alert["threshold"] = 50;
        _alerts.append(alert);
    }
    
    // 连接池耗尽告警
    if (_availableConnections.size() == 0 && _activeConnections.loadAcquire() >= _config.maxConnections) {
        QJsonObject alert;
        alert["type"] = "pool_exhausted";
        alert["severity"] = "critical";
        alert["message"] = "连接池已耗尽，无法提供新连接";
        alert["timestamp"] = now.toString(Qt::ISODate);
        alert["available_connections"] = _availableConnections.size();
        alert["active_connections"] = _activeConnections.loadAcquire();
        alert["max_connections"] = _config.maxConnections;
        _alerts.append(alert);
    }
    
    // 保持告警数量在合理范围内
    if (_alerts.size() > 50) {
        _alerts.removeFirst();
    }
}

void DatabaseConnectionPool::resizePool(int minConnections, int maxConnections)
{
    QMutexLocker locker(&_poolMutex);
    
    if (minConnections < 1 || maxConnections < minConnections || maxConnections > 100) {
        LOG_ERROR(QString("Invalid pool size parameters: min=%1, max=%2").arg(minConnections).arg(maxConnections));
        return;
    }
    
    LOG_INFO(QString("Resizing connection pool from %1-%2 to %3-%4")
             .arg(_config.minConnections).arg(_config.maxConnections)
             .arg(minConnections).arg(maxConnections));
    
    // 更新配置
    _config.minConnections = minConnections;
    _config.maxConnections = maxConnections;
    
    // 如果当前连接数小于新的最小值，创建更多连接
    int currentTotal = _totalConnections.loadAcquire();
    if (currentTotal < minConnections) {
        int needed = minConnections - currentTotal;
        LOG_INFO(QString("Creating %1 additional connections to meet minimum requirement").arg(needed));
        
        for (int i = 0; i < needed; ++i) {
            QSqlDatabase connection = createConnection();
            if (connection.isValid() && connection.isOpen()) {
                _availableConnections.enqueue(connection);
                _connectionLastUsed[connection.connectionName()] = QDateTime::currentDateTime();
                _totalConnections.fetchAndAddOrdered(1);
            } else {
                LOG_ERROR(QString("Failed to create connection %1 during resize").arg(i + 1));
            }
        }
    }
    
    // 如果当前连接数大于新的最大值，标记多余连接为待清理
    if (currentTotal > maxConnections) {
        int excess = currentTotal - maxConnections;
        LOG_INFO(QString("Marking %1 excess connections for cleanup").arg(excess));
        
        // 将多余的连接标记为待清理（在下次清理时移除）
        _excessConnections = excess;
    }
    
    _resizeCount++;
    _lastResizeTime = QDateTime::currentDateTime();
    
    LOG_INFO(QString("Connection pool resized successfully. Current: %1 total, %2 active, %3 available")
             .arg(_totalConnections.loadAcquire())
             .arg(_activeConnections.loadAcquire())
             .arg(_availableConnections.size()));
}

bool DatabaseConnectionPool::setAutoResizeEnabled(bool enabled)
{
    QMutexLocker locker(&_resizeMutex);
    
    if (_autoResizeEnabled == enabled) {
        return true; // 已经是目标状态
    }
    
    _autoResizeEnabled = enabled;
    
    if (enabled) {
        if (_resizeTimer && !_resizeTimer->isActive()) {
            _resizeTimer->start(_config.resizeCheckInterval);
            LOG_INFO(QString("Auto-resize enabled with %1ms check interval").arg(_config.resizeCheckInterval));
        }
    } else {
        if (_resizeTimer && _resizeTimer->isActive()) {
            _resizeTimer->stop();
            // LOG_INFO removed
        }
    }
    
    return true;
}
