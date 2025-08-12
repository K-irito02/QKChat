#include "DatabaseConnectionPool.h"
#include "../utils/Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QJsonObject>
#include <QUuid>
#include <QCoreApplication>

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
{
    // 创建定时器
    _healthCheckTimer = new QTimer(this);
    _cleanupTimer = new QTimer(this);
    
    connect(_healthCheckTimer, &QTimer::timeout, this, &DatabaseConnectionPool::performHealthCheck);
    connect(_cleanupTimer, &QTimer::timeout, this, &DatabaseConnectionPool::cleanupIdleConnections);
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
    _shuttingDown = false;
    
    LOG_INFO(QString("Initializing database connection pool: min=%1, max=%2")
             .arg(_config.minConnections).arg(_config.maxConnections));
    
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
    
    _initialized = true;
    
    LOG_INFO(QString("Database connection pool initialized with %1 connections")
             .arg(_availableConnections.size()));
    
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
                break;
            }
        }
        
        // 检查超时
        if (startTime.msecsTo(QDateTime::currentDateTime()) >= timeoutMs) {
            _acquireTimeouts.fetchAndAddOrdered(1);
            LOG_WARNING("Connection acquire timeout");
            return QSqlDatabase();
        }
        
        // 等待连接可用
        _connectionAvailable.wait(&_poolMutex, 100);
    }
    
    if (_availableConnections.isEmpty()) {
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
        
        // 尝试重新创建连接
        locker.unlock();
        QSqlDatabase newConnection = createConnection();
        locker.relock();
        
        if (newConnection.isValid() && newConnection.isOpen()) {
            _usedConnections.insert(newConnection.connectionName());
            _activeConnections.fetchAndAddOrdered(1);
            return newConnection;
        }
        
        return QSqlDatabase();
    }
    
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
    
    LOG_INFO("Shutting down database connection pool...");
    
    _shuttingDown = true;
    _healthCheckTimer->stop();
    _cleanupTimer->stop();
    
    // 等待所有连接释放
    int waitCount = 0;
    while (!_usedConnections.isEmpty() && waitCount < 50) { // 最多等待5秒
        locker.unlock();
        QThread::msleep(100);
        locker.relock();
        waitCount++;
    }
    
    if (!_usedConnections.isEmpty()) {
        LOG_WARNING(QString("Force closing %1 active connections").arg(_usedConnections.size()));
    }
    
    // 关闭所有连接
    while (!_availableConnections.isEmpty()) {
        QSqlDatabase connection = _availableConnections.dequeue();
        if (connection.isOpen()) {
            connection.close();
        }
        QSqlDatabase::removeDatabase(connection.connectionName());
    }
    
    _connectionLastUsed.clear();
    _usedConnections.clear();
    _totalConnections.storeRelease(0);
    _activeConnections.storeRelease(0);
    
    _initialized = false;
    

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
    
    return stats;
}

bool DatabaseConnectionPool::isHealthy() const
{
    QMutexLocker locker(&_poolMutex);
    return _initialized && !_shuttingDown && _totalConnections.loadAcquire() >= _config.minConnections;
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
        QSqlDatabase::removeDatabase(connectionName);
        return QSqlDatabase();
    }
    
    
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
        DatabaseConnectionPool::instance()->releaseConnection(_connection);
    }
}

QSqlQuery DatabaseConnection::executeQuery(const QString& sql, const QVariantList& params)
{
    if (!isValid()) {
        LOG_ERROR("Cannot execute query: invalid database connection");
        return QSqlQuery();
    }
    
    QSqlQuery query(_connection);
    query.prepare(sql);
    
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    
    if (!query.exec()) {
        LOG_ERROR(QString("Query execution failed: %1").arg(query.lastError().text()));
    }
    
    return query;
}

int DatabaseConnection::executeUpdate(const QString& sql, const QVariantList& params)
{
    QSqlQuery query = executeQuery(sql, params);
    return query.lastError().isValid() ? -1 : query.numRowsAffected();
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