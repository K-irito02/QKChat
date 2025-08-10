#include "DatabaseManager.h"
#include "../utils/Logger.h"
#include <QSqlDriver>
#include <QThread>
#include <QCoreApplication>
#include <QTimer>


// 静态成员初始化
DatabaseManager* DatabaseManager::s_instance = nullptr;
QMutex DatabaseManager::s_mutex;

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , _port(3306)
    , _isConnected(false)
{
    _connectionName = QString("QKChat_DB_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

DatabaseManager::~DatabaseManager()
{
    close();
}

DatabaseManager* DatabaseManager::instance()
{
    // 双重检查锁定模式，提高性能
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new DatabaseManager();
        }
    }
    return s_instance;
}

bool DatabaseManager::initialize(const QString &host, int port, const QString &database,
                               const QString &username, const QString &password)
{
    QMutexLocker locker(&_queryMutex);

    _host = host;
    _port = port;
    _databaseName = database;
    _username = username;
    _password = password;

    // 关闭现有连接
    if (_database.isOpen()) {
        _database.close();
    }

    // 移除现有连接
    if (QSqlDatabase::contains(_connectionName)) {
        QSqlDatabase::removeDatabase(_connectionName);
    }

    // 创建新的数据库连接
    _database = QSqlDatabase::addDatabase("QMYSQL", _connectionName);

    if (!_database.isValid()) {
        _lastError = "MySQL driver not available";
        logError(_lastError);
        return false;
    }

    setupConnection();

    // 尝试连接
    if (!_database.open()) {
        _lastError = _database.lastError().text();
        logError(QString("Failed to connect to database: %1").arg(_lastError));
        _isConnected = false;
        emit connectionStateChanged(false);
        return false;
    }

    _isConnected = true;
    _lastError.clear();

    LOG_INFO(QString("Connected to MySQL database: %1@%2:%3/%4")
             .arg(_username).arg(_host).arg(_port).arg(_databaseName));

    emit connectionStateChanged(true);

    // 创建表结构
    if (!createTables()) {
        LOG_WARNING("Failed to create database tables");
    }

    return true;
}

void DatabaseManager::close()
{
    QMutexLocker locker(&_queryMutex);
    
    if (_database.isOpen()) {
        _database.close();
        LOG_INFO("Database connection closed");
    }
    
    if (QSqlDatabase::contains(_connectionName)) {
        QSqlDatabase::removeDatabase(_connectionName);
    }
    
    _isConnected = false;
    emit connectionStateChanged(false);
}

bool DatabaseManager::isConnected() const
{
    // 对于只读操作，使用更轻量级的检查
    return _isConnected && _database.isOpen();
}

QSqlQuery DatabaseManager::executeQuery(const QString &sql, const QVariantList &params)
{
    QMutexLocker locker(&_queryMutex);
    
    if (!isConnected()) {
        LOG_WARNING("Database not connected, attempting to reconnect...");
        if (!reconnect()) {
            LOG_ERROR("Failed to reconnect to database");
            QSqlQuery invalidQuery;
            return invalidQuery;
        }
    }
    
    QSqlQuery query(_database);
    query.prepare(sql);
    
    // 绑定参数
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    
    if (!query.exec()) {
        _lastError = query.lastError().text();
        LOG_ERROR(QString("Query execution failed: %1").arg(_lastError));
        emit databaseError(_lastError);
    }
    
    return query;
}

int DatabaseManager::executeUpdate(const QString &sql, const QVariantList &params)
{
    QMutexLocker locker(&_queryMutex);
    
    if (!isConnected()) {
        LOG_WARNING("Database not connected, attempting to reconnect...");
        if (!reconnect()) {
            LOG_ERROR("Failed to reconnect to database");
            return -1;
        }
    }
    
    QSqlQuery query(_database);
    query.prepare(sql);
    
    // 绑定参数
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }
    
    if (!query.exec()) {
        _lastError = query.lastError().text();
        LOG_ERROR(QString("SQL update failed: %1").arg(_lastError));
        emit databaseError(_lastError);
        return -1;
    }
    
    return query.numRowsAffected();
}

bool DatabaseManager::beginTransaction()
{
    QMutexLocker locker(&_queryMutex);
    
    if (!isConnected()) {
        return false;
    }
    
    bool success = _database.transaction();
    if (!success) {
        _lastError = _database.lastError().text();
        logError(QString("Failed to begin transaction: %1").arg(_lastError));
    }
    
    return success;
}

bool DatabaseManager::commitTransaction()
{
    QMutexLocker locker(&_queryMutex);
    
    if (!isConnected()) {
        return false;
    }
    
    bool success = _database.commit();
    if (!success) {
        _lastError = _database.lastError().text();
        logError(QString("Failed to commit transaction: %1").arg(_lastError));
    }
    
    return success;
}

bool DatabaseManager::rollbackTransaction()
{
    QMutexLocker locker(&_queryMutex);
    
    if (!isConnected()) {
        return false;
    }
    
    bool success = _database.rollback();
    if (!success) {
        _lastError = _database.lastError().text();
        logError(QString("Failed to rollback transaction: %1").arg(_lastError));
    }
    
    return success;
}

qint64 DatabaseManager::lastInsertId() const
{
    QMutexLocker locker(&_queryMutex);
    
    QSqlQuery query("SELECT LAST_INSERT_ID()", _database);
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    
    return -1;
}

QString DatabaseManager::lastError() const
{
    return _lastError;
}

bool DatabaseManager::reconnect()
{
    LOG_INFO("Attempting to reconnect to database...");
    
    if (_database.isOpen()) {
        _database.close();
    }
    
    setupConnection();
    
    if (!_database.open()) {
        _lastError = _database.lastError().text();
        logError(QString("Reconnection failed: %1").arg(_lastError));
        _isConnected = false;
        emit connectionStateChanged(false);
        return false;
    }
    
    _isConnected = true;
    _lastError.clear();
    
    LOG_INFO("Database reconnection successful");
    emit connectionStateChanged(true);
    
    return true;
}

void DatabaseManager::setupConnection()
{
    _database.setHostName(_host);
    _database.setPort(_port);
    _database.setDatabaseName(_databaseName);
    _database.setUserName(_username);
    _database.setPassword(_password);
    
    // 设置连接选项，使用较短的超时时间避免阻塞
    _database.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3;MYSQL_OPT_READ_TIMEOUT=5;MYSQL_OPT_WRITE_TIMEOUT=5");
}

void DatabaseManager::logError(const QString &error)
{
    // 使用QTimer延迟记录日志，避免在持有锁时调用LOG宏
    QTimer::singleShot(0, [error]() {
        LOG_ERROR(error);
    });
    _lastError = error;
}

bool DatabaseManager::createTables()
{
    if (!isConnected()) {
        LOG_ERROR("Database not connected, cannot create tables");
        return false;
    }

    LOG_INFO("Creating database tables...");

    // 创建用户表
    QString createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) NOT NULL UNIQUE COMMENT '用户名',
            email VARCHAR(100) NOT NULL UNIQUE COMMENT '邮箱',
            password_hash VARCHAR(255) NOT NULL COMMENT '密码哈希',
            salt VARCHAR(64) NOT NULL COMMENT '盐值',
            display_name VARCHAR(200) DEFAULT NULL COMMENT '显示名称',
            avatar_url VARCHAR(512) DEFAULT NULL COMMENT '头像URL',
            bio TEXT DEFAULT NULL COMMENT '个人简介',
            status ENUM('active', 'inactive', 'banned', 'deleted') DEFAULT 'inactive' COMMENT '账户状态',
            email_verified BOOLEAN DEFAULT FALSE COMMENT '邮箱是否已验证',
            verification_code VARCHAR(10) DEFAULT NULL COMMENT '验证码',
            verification_expires TIMESTAMP NULL DEFAULT NULL COMMENT '验证码过期时间',
            last_online TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP COMMENT '最后在线时间',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
            UNIQUE INDEX idx_username (username),
            UNIQUE INDEX idx_email (email),
            INDEX idx_status (status),
            INDEX idx_last_online (last_online),
            INDEX idx_email_verified (email_verified),
            INDEX idx_verification_expires (verification_expires),
            INDEX idx_created_at (created_at),
            INDEX idx_updated_at (updated_at)
        ) ENGINE=InnoDB COMMENT='用户表'
    )";

    if (executeUpdate(createUsersTable) == -1) {
        LOG_ERROR("Failed to create users table");
        return false;
    }

    // 创建验证码表
    QString createVerificationCodesTable = R"(
        CREATE TABLE IF NOT EXISTS verification_codes (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            email VARCHAR(100) NOT NULL COMMENT '邮箱地址',
            code VARCHAR(10) NOT NULL COMMENT '验证码',
            type ENUM('registration', 'password_reset', 'email_change') DEFAULT 'registration' COMMENT '验证码类型',
            expires_at TIMESTAMP NOT NULL COMMENT '过期时间',
            used_at TIMESTAMP NULL DEFAULT NULL COMMENT '使用时间',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
            INDEX idx_email (email),
            INDEX idx_code (code),
            INDEX idx_type (type),
            INDEX idx_expires_at (expires_at),
            INDEX idx_used_at (used_at),
            INDEX idx_email_type_expires (email, type, expires_at)
        ) ENGINE=InnoDB COMMENT='验证码表'
    )";

    if (executeUpdate(createVerificationCodesTable) == -1) {
        LOG_ERROR("Failed to create verification_codes table");
        return false;
    }

    // 创建用户会话表
    QString createSessionsTable = R"(
        CREATE TABLE IF NOT EXISTS user_sessions (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            user_id BIGINT UNSIGNED NOT NULL,
            session_token VARCHAR(128) NOT NULL UNIQUE COMMENT '会话令牌',
            refresh_token VARCHAR(128) DEFAULT NULL COMMENT '刷新令牌',
            device_info VARCHAR(500) DEFAULT NULL COMMENT '设备信息',
            ip_address VARCHAR(45) DEFAULT NULL COMMENT 'IP地址',
            user_agent TEXT DEFAULT NULL COMMENT '用户代理',
            expires_at TIMESTAMP NOT NULL COMMENT '过期时间',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
            last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后活动时间',
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            INDEX idx_user_id (user_id),
            INDEX idx_session_token (session_token),
            INDEX idx_expires_at (expires_at),
            INDEX idx_user_expires (user_id, expires_at)
        ) ENGINE=InnoDB COMMENT='用户会话表'
    )";

    LOG_INFO("Creating sessions table...");
    if (executeUpdate(createSessionsTable) == -1) {
        LOG_ERROR("Failed to create sessions table");
        return false;
    }

    // 创建登录日志表
    QString createLoginLogsTable = R"(
        CREATE TABLE IF NOT EXISTS login_logs (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            user_id BIGINT UNSIGNED,
            username VARCHAR(50),
            email VARCHAR(100),
            success BOOLEAN NOT NULL,
            ip_address VARCHAR(45),
            user_agent TEXT,
            error_message TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_user_id (user_id),
            INDEX idx_success (success),
            INDEX idx_created_at (created_at),
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL
        ) ENGINE=InnoDB COMMENT='登录日志表'
    )";

    if (executeUpdate(createLoginLogsTable) == -1) {
        LOG_ERROR("Failed to create login_logs table");
        return false;
    }

    return true;
}



bool DatabaseManager::tableExists(const QString &tableName)
{
    QString sql = "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = ? AND table_name = ?";
    QSqlQuery query = executeQuery(sql, {_databaseName, tableName});

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QString DatabaseManager::getDatabaseVersion()
{
    QSqlQuery query = executeQuery("SELECT VERSION()");
    if (query.next()) {
        return query.value(0).toString();
    }
    return "Unknown";
}

bool DatabaseManager::testConnection()
{
    QSqlQuery query = executeQuery("SELECT 1");
    return !query.lastError().isValid();
}

bool DatabaseManager::optimizeDatabase()
{
    LOG_INFO("Optimizing database...");

    QStringList tables = {"users", "verification_codes", "sessions", "login_logs"};

    for (const QString &table : tables) {
        if (tableExists(table)) {
            QString sql = QString("OPTIMIZE TABLE %1").arg(table);
            if (executeUpdate(sql) == -1) {
                LOG_WARNING(QString("Failed to optimize table: %1").arg(table));
            }
        }
    }

    // 清理过期的验证码
    QString cleanupCodes = "DELETE FROM verification_codes WHERE expires_at < NOW()";
    executeUpdate(cleanupCodes);

    // 清理过期的会话
    QString cleanupSessions = "DELETE FROM sessions WHERE expires_at < NOW()";
    executeUpdate(cleanupSessions);

    // 清理旧的登录日志（保留30天）
    QString cleanupLogs = "DELETE FROM login_logs WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 DAY)";
    executeUpdate(cleanupLogs);

    LOG_INFO("Database optimization completed");
    return true;
}
