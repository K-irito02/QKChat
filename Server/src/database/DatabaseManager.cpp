#include "DatabaseManager.h"
#include "../utils/Logger.h"
#include <QSqlDriver>
#include <QThread>
#include <QCoreApplication>
#include <QTimer>
#include <QJsonObject>
#include <functional>

// 静态成员初始化
DatabaseManager* DatabaseManager::s_instance = nullptr;
QMutex DatabaseManager::s_mutex;

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , _connectionPool(nullptr)
    , _isConnected(false)
{
    // 注册应用程序退出时的清理函数
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, 
            this, &DatabaseManager::close);
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
                               const QString &username, const QString &password,
                               int minConnections, int maxConnections)
{
    if (_isConnected) {
        LOG_WARNING("Database manager already initialized");
        return true;
    }

    // 初始化数据库管理器

    // 获取连接池实例
    _connectionPool = DatabaseConnectionPool::instance();

    // 配置连接池
    DatabaseConnectionPool::PoolConfig config;
    config.host = host;
    config.port = port;
    config.database = database;
    config.username = username;
    config.password = password;
    config.minConnections = minConnections;
    config.maxConnections = maxConnections;

    // 初始化连接池
    if (!_connectionPool->initialize(config)) {
        _lastError = "Failed to initialize database connection pool";
        logError(_lastError);
        emit connectionStateChanged(false);
        return false;
    }

    _isConnected = true;
    _lastError.clear();


    emit connectionStateChanged(true);

    // 创建表结构
    if (!createTables()) {
        LOG_WARNING("Failed to create database tables");
    }

    return true;
}

void DatabaseManager::close()
{
    if (!_isConnected) {
        return;
    }

    // 关闭数据库管理器
    // LOG_INFO removed

    // 确保所有数据库操作完成
    QThread::msleep(100);

    if (_connectionPool) {
        _connectionPool->shutdown();
        
        // 等待连接池完全关闭
        QThread::msleep(200);
    }

    _isConnected = false;
    emit connectionStateChanged(false);

    // LOG_INFO removed
}

bool DatabaseManager::isConnected() const
{
    return _isConnected && _connectionPool && _connectionPool->isHealthy();
}

QSqlQuery DatabaseManager::executeQuery(const QString &sql, const QVariantList &params)
{
    if (!isConnected()) {
        LOG_ERROR("Database not connected");
        return QSqlQuery();
    }

    // 使用RAII包装器自动管理连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        _lastError = "Failed to acquire database connection";
        logError(_lastError);
        return QSqlQuery();
    }

    return dbConn.executeQuery(sql, params);
}

int DatabaseManager::executeUpdate(const QString &sql, const QVariantList &params)
{
    if (!isConnected()) {
        LOG_ERROR("Database not connected");
        return -1;
    }

    // 使用RAII包装器自动管理连接
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        _lastError = "Failed to acquire database connection";
        logError(_lastError);
        return -1;
    }

    return dbConn.executeUpdate(sql, params);
}

bool DatabaseManager::executeTransaction(std::function<bool(DatabaseConnection&)> operations)
{
    if (!isConnected()) {
        LOG_ERROR("Database not connected");
        return false;
    }

    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        _lastError = "Failed to acquire database connection";
        logError(_lastError);
        return false;
    }

    // 开始事务
    if (!dbConn.beginTransaction()) {
        _lastError = "Failed to begin transaction";
        logError(_lastError);
        return false;
    }

    // 执行事务操作
    bool success = false;
    try {
        success = operations(dbConn);
    } catch (const std::exception& e) {
        _lastError = QString("Transaction operation failed: %1").arg(e.what());
        logError(_lastError);
        success = false;
    } catch (...) {
        _lastError = "Unknown exception in transaction operation";
        logError(_lastError);
        success = false;
    }

    // 提交或回滚事务
    if (success) {
        if (dbConn.commitTransaction()) {
            return true;
        } else {
            _lastError = "Failed to commit transaction";
            logError(_lastError);
        }
    }

    // 回滚事务
    if (!dbConn.rollbackTransaction()) {
        LOG_ERROR("Failed to rollback transaction");
    }

    return false;
}

qint64 DatabaseManager::lastInsertId(const QSqlDatabase& connection) const
{
    QSqlQuery query("SELECT LAST_INSERT_ID()", connection);
    if (query.exec() && query.next()) {
        return query.value(0).toLongLong();
    }
    return -1;
}

QString DatabaseManager::lastError() const
{
    QMutexLocker locker(&_errorMutex);
    return _lastError;
}

void DatabaseManager::logError(const QString &error)
{
    // 使用QTimer延迟记录日志，避免在持有锁时调用LOG宏
    QTimer::singleShot(0, [error]() {
        LOG_ERROR(error);
    });
    
    QMutexLocker locker(&_errorMutex);
    _lastError = error;
}

QJsonObject DatabaseManager::getConnectionPoolStatistics() const
{
    if (_connectionPool) {
        return _connectionPool->getStatistics();
    }
    return QJsonObject();
}

bool DatabaseManager::createTables()
{
    if (!isConnected()) {
        LOG_ERROR("Database not connected, cannot create tables");
        return false;
    }

    // LOG_INFO removed

    // 使用事务确保表创建的原子性
    return executeTransaction([this](DatabaseConnection& dbConn) -> bool {
        
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

        if (dbConn.executeUpdate(createUsersTable) == -1) {
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

        if (dbConn.executeUpdate(createVerificationCodesTable) == -1) {
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

        if (dbConn.executeUpdate(createSessionsTable) == -1) {
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

        if (dbConn.executeUpdate(createLoginLogsTable) == -1) {
            LOG_ERROR("Failed to create login_logs table");
            return false;
        }

    
        return true;
    });
}

bool DatabaseManager::tableExists(const QString &tableName)
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        return false;
    }

    QString sql = "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = ?";
    QSqlQuery query = dbConn.executeQuery(sql, {tableName});

    if (query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QString DatabaseManager::getDatabaseVersion()
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        return "Unknown";
    }

    QSqlQuery query = dbConn.executeQuery("SELECT VERSION()");
    if (query.next()) {
        return query.value(0).toString();
    }
    return "Unknown";
}

bool DatabaseManager::testConnection()
{
    DatabaseConnection dbConn;
    if (!dbConn.isValid()) {
        return false;
    }

    QSqlQuery query = dbConn.executeQuery("SELECT 1");
    return !query.lastError().isValid();
}

QSqlDatabase DatabaseManager::getConnection()
{
    if (_connectionPool) {
        return _connectionPool->acquireConnection();
    }
    return QSqlDatabase();
}

bool DatabaseManager::optimizeDatabase()
{
    // LOG_INFO removed

    return executeTransaction([this](DatabaseConnection& dbConn) -> bool {
        QStringList tables = {"users", "verification_codes", "user_sessions", "login_logs"};

        for (const QString &table : tables) {
            if (tableExists(table)) {
                QString sql = QString("OPTIMIZE TABLE %1").arg(table);
                if (dbConn.executeUpdate(sql) == -1) {
                    LOG_WARNING(QString("Failed to optimize table: %1").arg(table));
                }
            }
        }

        // 清理过期的验证码
        QString cleanupCodes = "DELETE FROM verification_codes WHERE expires_at < NOW()";
        dbConn.executeUpdate(cleanupCodes);

        // 清理过期的会话
        QString cleanupSessions = "DELETE FROM user_sessions WHERE expires_at < NOW()";
        dbConn.executeUpdate(cleanupSessions);

        // 清理旧的登录日志（保留30天）
        QString cleanupLogs = "DELETE FROM login_logs WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 DAY)";
        dbConn.executeUpdate(cleanupLogs);

    
        return true;
    });
}
