#include "ServerManager.h"
#include "config/ConfigManager.h"
#include "security/CertificateManager.h"
#include "security/OpenSSLHelper.h"
#include "database/DatabaseManager.h"
#include "database/RedisClient.h"
#include "auth/EmailService.h"
#include "network/ThreadPoolServer.h"
#include "network/AsyncMessageQueue.h"
#include "network/ProtocolHandler.h"
#include "network/ClientHandler.h"
#include "utils/Logger.h"
#include "utils/Crypto.h"
#include "auth/UserService.h"
#include "chat/FriendService.h"
#include "chat/MessageService.h"
#include "chat/OnlineStatusService.h"
#include "cache/CacheManager.h"
#include "rate_limit/RateLimitManager.h"
#include "database/DatabaseConnectionPool.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimer>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QHostAddress>

// 静态成员初始化
ServerManager* ServerManager::s_instance = nullptr;

ServerManager::ServerManager(QObject *parent)
    : QObject(parent)
    , _serverState(Stopped)
    , _serverPort(8080)
    , _databaseManager(nullptr)
    , _redisClient(nullptr)
    , _emailService(nullptr)
    , _threadPoolServer(nullptr)
    , _messageQueue(nullptr)
    , _protocolHandler(nullptr)
    , _clientCount(0)
    , _totalConnections(0)
    , _totalRegistrations(0)
{
}

ServerManager::~ServerManager()
{
    stopServer();
}

ServerManager* ServerManager::instance()
{
    if (!s_instance) {
        s_instance = new ServerManager();
    }
    return s_instance;
}

bool ServerManager::initialize()
{
    setServerState(Starting);

    // 首先初始化基本日志系统
    QString basicLogDir = "D:/QT_Learn/Projects/QKChat/Server/logs";
    QDir().mkpath(basicLogDir);
    if (!Logger::initialize(basicLogDir, "Server")) {
        QString relativeLogDir = "logs";
        QDir().mkpath(relativeLogDir);
        Logger::initialize(relativeLogDir, "Server");
    }

    // LOG_INFO removed

    try {
        // 初始化OpenSSL库
        if (!OpenSSLHelper::initializeOpenSSL()) {
            LOG_ERROR("Failed to initialize OpenSSL library");
            setServerState(Error);
            return false;
        }

        // 初始化配置管理器
        // LOG_INFO removed
        ConfigManager* configManager = nullptr;
        try {
            configManager = ConfigManager::instance();
        
        } catch (const std::exception& e) {
            LOG_ERROR(QString("Exception creating ConfigManager: %1").arg(e.what()));
            setServerState(Error);
            return false;
        } catch (...) {
            LOG_ERROR("Unknown exception creating ConfigManager");
            setServerState(Error);
            return false;
        }

        // 加载配置文件
        QString appDir = QCoreApplication::applicationDirPath();
        QString configPath = appDir + "/config/server.json";

        

        QFileInfo configFile(configPath);
        if (!configFile.exists()) {
            LOG_WARNING(QString("Configuration file not found: %1").arg(configPath));
            LOG_WARNING("Using default configuration");

            QStringList alternatePaths = {
                "config/server.json",
                "../config/server.json",
                "../../config/server.json",
                "../../../config/server.json"
            };

            bool found = false;
            for (const QString &altPath : alternatePaths) {
                QFileInfo altFile(altPath);
                if (altFile.exists()) {
                    configPath = altFile.absoluteFilePath();
                
                    found = true;
                    break;
                }
            }

            if (!found) {
                LOG_WARNING("No configuration file found in any location, using defaults");
            }
        }

        // LOG_INFO removed
        if (!configManager->loadConfig(configPath)) {
            LOG_WARNING("Failed to load configuration file, using defaults");
        } else {
        
        }

        // 配置日志系统
        // LOG_INFO removed
        Logger::setLogLevel(static_cast<Logger::LogLevel>(
            configManager->getValue("logging.level", static_cast<int>(Logger::INFO)).toInt()));
        Logger::setConsoleOutput(configManager->getValue("logging.console_output", true).toBool());
    

    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during configuration initialization: %1").arg(e.what()));
        setServerState(Error);
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception during configuration initialization");
        setServerState(Error);
        return false;
    }

    // 初始化核心组件
    if (!initializeDatabasePool()) {
        LOG_ERROR("Failed to initialize database connection pool");
        setServerState(Error);
        return false;
    }

    if (!initializeRedis()) {
        LOG_WARNING("Failed to initialize Redis (optional)");
    }

    if (!initializeEmailService()) {
        LOG_ERROR("Failed to initialize email service - verification codes will not work");
        setServerState(Error);
        return false;
    }

    // 暂时禁用AsyncMessageQueue，避免重复发送消息
    // if (!initializeMessageQueue()) {
    //     LOG_ERROR("Failed to initialize async message queue");
    //     setServerState(Error);
    //     return false;
    // }
    // LOG_INFO removed

    if (!initializeThreadPoolServer()) {
        LOG_ERROR("Failed to initialize thread pool server");
        setServerState(Error);
        return false;
    }

    // 异步初始化证书
    QTimer::singleShot(200, this, &ServerManager::initializeCertificatesAsync);


    setServerState(Stopped);

    return true;
}

bool ServerManager::startServer(quint16 port)
{
    if (_serverState == Running) {
        LOG_WARNING("Server is already running");
        return true;
    }
    
    if (_serverState != Stopped && _serverState != Error) {
        LOG_ERROR("Cannot start server in current state");
        return false;
    }
    
    // 如果未指定端口，从配置文件读取
    ConfigManager* configManager = ConfigManager::instance();
    if (port == 0) {
        port = configManager->getValue("server.port", 8080).toUInt();
    }

    _serverPort = port;



    // 从配置文件读取TLS设置
    bool useTls = configManager->getValue("server.use_tls", true).toBool();
    
    // 启动线程池服务器
    if (!_threadPoolServer->startServer(port, QHostAddress::Any, useTls)) {
        LOG_ERROR("Failed to start thread pool server");
        setServerState(Error);
        return false;
    }
    
    _startTime = QDateTime::currentDateTime();
    setServerState(Running);
    

    // LOG_INFO removed
    // LOG_INFO removed
    // LOG_INFO removed
    // LOG_INFO removed
    // LOG_INFO removed
    // LOG_INFO removed
    
    return true;
}

void ServerManager::stopServer()
{
    if (_serverState == Stopped) {
        return;
    }

    // LOG_INFO removed
    setServerState(Stopping);

    // 停止线程池服务器
    if (_threadPoolServer) {
        _threadPoolServer->stopServer();
    }

    // 停止异步消息队列
    if (_messageQueue) {
        _messageQueue->shutdown();
    }

    // 关闭数据库连接池
    DatabaseConnectionPool::instance()->shutdown();
    if (_databaseManager) {
        _databaseManager->close();
    }

    // 关闭Redis连接
    if (_redisClient) {
        _redisClient->close();
    }

    // 清理OpenSSL
    OpenSSLHelper::cleanupOpenSSL();

    setServerState(Stopped);
    // LOG_INFO removed
}

QJsonObject ServerManager::getServerStatistics() const
{
    QJsonObject stats;
    
    stats["server_state"] = static_cast<int>(_serverState);
    stats["server_port"] = _serverPort;
    stats["client_count"] = _clientCount;
    stats["total_connections"] = _totalConnections;
    stats["total_registrations"] = _totalRegistrations;
    
    if (_startTime.isValid()) {
        stats["uptime_seconds"] = _startTime.secsTo(QDateTime::currentDateTime());
        stats["start_time"] = _startTime.toString(Qt::ISODate);
    }
    
    // 数据库连接池统计
    if (_databaseManager) {
        stats["database_pool"] = _databaseManager->getConnectionPoolStatistics();
    }
    
    // Redis状态
    if (_redisClient) {
        stats["redis_connected"] = _redisClient->isConnected();
    }
    
    // 线程池服务器统计
    if (_threadPoolServer) {
        QJsonObject serverStats = _threadPoolServer->getServerStatistics();
        stats["thread_pool_server"] = serverStats;
    }
    
    // 异步消息队列统计
    if (_messageQueue) {
        QJsonObject queueStats = _messageQueue->getStatistics();
        stats["message_queue"] = queueStats;
    }
    
    return stats;
}

QJsonObject ServerManager::getHighConcurrencyStatus()
{
    QJsonObject status;
    
    // 缓存状态
    status["cache"] = getCacheStatistics();
    
    // 限流状态
    status["rate_limit"] = getRateLimitStatistics();
    
    // 连接池状态
    status["connection_pool"] = getConnectionPoolStatistics();
    
    // 热点数据状态
    status["hot_data"] = getHotDataStatistics();
    
    // 整体性能指标
    QJsonObject performance;
    performance["uptime"] = _startTime.secsTo(QDateTime::currentDateTime());
    performance["total_requests"] = _totalRequests;
    performance["active_connections"] = _activeConnections;
    performance["online_users"] = getOnlineUserCount();
    status["performance"] = performance;
    
    return status;
}

QJsonObject ServerManager::getCacheStatistics()
{
    QJsonObject stats;
    
    // L1缓存统计
    QJsonObject l1Stats = CacheManager::instance()->getCacheStats();
    stats["l1_cache"] = l1Stats;
    
    // L2缓存统计
    QJsonObject l2Stats = CacheManager::instance()->getL2CacheStats();
    stats["l2_cache"] = l2Stats;
    
    // 热点数据统计
    QJsonArray hotSearchData = CacheManager::instance()->getHotDataList("user_search", 10);
    stats["hot_search_keywords"] = hotSearchData;
    
    return stats;
}

QJsonObject ServerManager::getRateLimitStatistics()
{
    QJsonObject stats = RateLimitManager::instance()->getRateLimitStats();
    
    // 添加限流配置信息
    QJsonObject configs;
    configs["friend_search"] = QJsonObject{
        {"max_requests", 20},
        {"window_seconds", 60},
        {"tokens_per_second", 0.333}
    };
    configs["login"] = QJsonObject{
        {"max_requests", 10},
        {"window_seconds", 60},
        {"tokens_per_second", 0.167}
    };
    
    stats["configurations"] = configs;
    
    return stats;
}

QJsonObject ServerManager::getConnectionPoolStatistics()
{
    QJsonObject stats = DatabaseConnectionPool::instance()->getStatistics();
    
    // 添加负载预测信息
    QJsonObject prediction = DatabaseConnectionPool::instance()->getLoadPrediction();
    stats["load_prediction"] = prediction;
    
    // 添加性能趋势分析
    QJsonObject trend = DatabaseConnectionPool::instance()->analyzePerformanceTrend();
    stats["performance_trend"] = trend;
    
    return stats;
}

QJsonObject ServerManager::getHotDataStatistics()
{
    QJsonObject stats;
    
    // 搜索热点数据
    QJsonArray searchHotData = CacheManager::instance()->getHotDataList("user_search", 20);
    stats["search_hot_data"] = searchHotData;
    
    // 热点数据阈值统计
    QJsonObject thresholds;
    thresholds["search_threshold"] = 5;
    thresholds["message_threshold"] = 10;
    thresholds["file_threshold"] = 3;
    stats["thresholds"] = thresholds;
    
    return stats;
}

bool ServerManager::clearCache(const QString& cacheType)
{
    if (cacheType == "all" || cacheType == "l1") {
        CacheManager::instance()->clearCache();
        // LOG_INFO removed
    }
    
    if (cacheType == "all" || cacheType == "l2") {
        CacheManager::instance()->cleanupL2Cache();
        // LOG_INFO removed
    }
    
    return true;
}

bool ServerManager::resetRateLimit(const QString& identifier)
{
    if (identifier == "all") {
        // 重置所有限流状态
        RateLimitManager::instance()->resetRateLimit("", "");
        // LOG_INFO removed
    } else {
        // 重置特定标识符的限流状态
        RateLimitManager::instance()->resetRateLimit(identifier, "friend_search");
        RateLimitManager::instance()->resetRateLimit(identifier, "login");
    
    }
    
    return true;
}

bool ServerManager::resizeConnectionPool(int minConnections, int maxConnections)
{
    if (minConnections < 1 || maxConnections < minConnections || maxConnections > 100) {
        LOG_ERROR("Invalid connection pool size parameters");
        return false;
    }
    
    DatabaseConnectionPool::instance()->resizePool(minConnections, maxConnections);

    
    return true;
}

bool ServerManager::setAutoResizeEnabled(bool enabled)
{
    DatabaseConnectionPool::instance()->setAutoResizeEnabled(enabled);

    
    return true;
}

int ServerManager::getClientCount() const
{
    return _clientCount;
}

int ServerManager::getOnlineUserCount() const
{
    return _clientCount; // 简化实现
}

void ServerManager::onThreadPoolClientConnected(ClientHandler* client)
{
    _clientCount++;
    _totalConnections++;
    
    QString clientId = client ? client->clientId() : "unknown";

    emit clientConnected(_clientCount);
}

void ServerManager::onThreadPoolClientDisconnected(ClientHandler* client)
{
    _clientCount--;
    
    QString clientId = client ? client->clientId() : "unknown";

    emit clientDisconnected(_clientCount);
}

void ServerManager::onThreadPoolUserLoggedIn(qint64 userId, ClientHandler* client)
{
    QString clientId = client ? client->clientId() : "unknown";

    emit userLoggedIn(userId, QString("User_%1").arg(userId));
}

void ServerManager::onThreadPoolUserLoggedOut(qint64 userId)
{

}

void ServerManager::onProtocolUserLoggedIn(qint64 userId, const QString &clientId, const QString &sessionToken)
{

    emit userLoggedIn(userId, QString("User_%1").arg(userId));
}

void ServerManager::onProtocolUserRegistered(qint64 userId, const QString &username, const QString &email)
{
    _totalRegistrations++;
    

    emit userRegistered(userId, username, email);
}

void ServerManager::onDatabaseConnectionChanged(bool connected)
{
    if (connected) {
        // LOG_INFO removed
    } else {
        LOG_WARNING("Database connection pool lost");
        emit serverError("Database connection pool lost");
    }
}

void ServerManager::onRedisConnectionChanged(bool connected)
{
    if (connected) {
        // LOG_INFO removed
    } else {
        LOG_WARNING("Redis connection lost");
    }
}

void ServerManager::onMessageQueueError(const QString &error)
{
    LOG_ERROR(QString("Message queue error: %1").arg(error));
    emit serverError(QString("Message queue error: %1").arg(error));
}

void ServerManager::setServerState(ServerState state)
{
    if (_serverState != state) {
        _serverState = state;
        emit serverStateChanged(state);
    }
}

bool ServerManager::initializeDatabasePool()
{
    _databaseManager = DatabaseManager::instance();
    
    connect(_databaseManager, &DatabaseManager::connectionStateChanged,
            this, &ServerManager::onDatabaseConnectionChanged);
    
    // 从配置文件读取数据库配置
    ConfigManager* configManager = ConfigManager::instance();
    QString host = configManager->getValue("database.host", "localhost").toString();
    int port = configManager->getValue("database.port", 3306).toInt();
    QString database = configManager->getValue("database.name", "qkchat").toString();
    QString username = configManager->getValue("database.username", "root").toString();
    QString password = configManager->getValue("database.password", "").toString();
    int minConnections = configManager->getValue("database.min_connections", 5).toInt();
    int maxConnections = configManager->getValue("database.max_connections", 20).toInt();
    
    
    
    bool result = _databaseManager->initialize(host, port, database, username, password, 
                                              minConnections, maxConnections);
    
    if (result) {
    
    }
    
    return result;
}

bool ServerManager::initializeRedis()
{
    _redisClient = RedisClient::instance();
    
    connect(_redisClient, &RedisClient::connectionStateChanged,
            this, &ServerManager::onRedisConnectionChanged);
    
    // 从配置文件读取Redis配置
    ConfigManager* configManager = ConfigManager::instance();
    QString host = configManager->getValue("redis.host", "localhost").toString();
    int port = configManager->getValue("redis.port", 6379).toInt();
    QString password = configManager->getValue("redis.password", "").toString();
    int database = configManager->getValue("redis.database", 0).toInt();
    

    
    return _redisClient->initialize(host, port, password, database);
}

bool ServerManager::initializeEmailService()
{
    _emailService = new EmailService(this);

    // 从配置文件读取SMTP配置
    ConfigManager* configManager = ConfigManager::instance();
    QString host = configManager->getValue("smtp.host", "smtp.qq.com").toString();
    int port = configManager->getValue("smtp.port", 587).toInt();
    QString username = configManager->getValue("smtp.username", "").toString();
    QString password = configManager->getValue("smtp.password", "").toString();
    bool useTls = configManager->getValue("smtp.use_tls", true).toBool();

    

    if (username.isEmpty() || password.isEmpty()) {
        LOG_ERROR("SMTP username or password is empty in configuration");
        return false;
    }

    return _emailService->initialize(host, port, username, password, useTls);
}

bool ServerManager::initializeThreadPoolServer()
{
    _threadPoolServer = new ThreadPoolServer(this);

    // 创建协议处理器
    _protocolHandler = new ProtocolHandler(_emailService, this);

    // 从配置文件读取服务器配置
    ConfigManager* configManager = ConfigManager::instance();
    
    ServerConfig serverConfig;
    serverConfig.minThreads = configManager->getValue("server.min_threads", 4).toInt();
    serverConfig.maxThreads = configManager->getValue("server.max_threads", 16).toInt();
    serverConfig.maxClients = configManager->getValue("server.max_clients", 5000).toInt();
    serverConfig.connectionTimeout = configManager->getValue("server.connection_timeout", 30000).toInt();
    serverConfig.heartbeatInterval = configManager->getValue("server.heartbeat_interval", 30000).toInt();
    serverConfig.enableLoadBalancing = configManager->getValue("server.enable_load_balancing", true).toBool();
    serverConfig.enableRateLimiting = configManager->getValue("server.enable_rate_limiting", true).toBool();
    serverConfig.maxConnectionsPerIP = configManager->getValue("server.max_connections_per_ip", 10).toInt();

    // 初始化线程池服务器
    if (!_threadPoolServer->initialize(serverConfig)) {
        LOG_ERROR("Failed to initialize thread pool server");
        return false;
    }

    // 设置协议处理器
    _threadPoolServer->setProtocolHandler(_protocolHandler);
    
    // 连接信号
    connect(_threadPoolServer, &ThreadPoolServer::clientConnected,
            this, &ServerManager::onThreadPoolClientConnected);
    connect(_threadPoolServer, &ThreadPoolServer::clientDisconnected,
            this, &ServerManager::onThreadPoolClientDisconnected);
    connect(_threadPoolServer, &ThreadPoolServer::userLoggedIn,
            this, &ServerManager::onThreadPoolUserLoggedIn);
    connect(_threadPoolServer, &ThreadPoolServer::userLoggedOut,
            this, &ServerManager::onThreadPoolUserLoggedOut);
    
    // 连接协议处理器信号
    connect(_protocolHandler, &ProtocolHandler::userLoggedIn,
            this, &ServerManager::onProtocolUserLoggedIn);
    connect(_protocolHandler, &ProtocolHandler::userRegistered,
            this, &ServerManager::onProtocolUserRegistered);
    

    return true;
}

bool ServerManager::initializeMessageQueue()
{
    _messageQueue = AsyncMessageQueue::instance();
    
    // 从配置文件读取消息队列配置
    ConfigManager* configManager = ConfigManager::instance();
    
    QueueConfig queueConfig;
    queueConfig.maxQueueSize = configManager->getValue("message_queue.max_queue_size", 10000).toInt();
    queueConfig.workerThreads = configManager->getValue("message_queue.worker_threads", 4).toInt();
    queueConfig.batchSize = configManager->getValue("message_queue.batch_size", 50).toInt();
    queueConfig.processingInterval = configManager->getValue("message_queue.processing_interval", 10).toInt();
    queueConfig.maxRetryCount = configManager->getValue("message_queue.max_retry_count", 3).toInt();
    queueConfig.retryDelay = configManager->getValue("message_queue.retry_delay", 1000).toInt();
    queueConfig.enableFlowControl = configManager->getValue("message_queue.enable_flow_control", true).toBool();
    queueConfig.flowControlThreshold = configManager->getValue("message_queue.flow_control_threshold", 8000).toInt();

    // 初始化消息队列
    if (!_messageQueue->initialize(queueConfig)) {
        LOG_ERROR("Failed to initialize async message queue");
        return false;
    }

    // 连接信号
    connect(_messageQueue, &AsyncMessageQueue::queueError,
            this, &ServerManager::onMessageQueueError);
    

    return true;
}

void ServerManager::initializeCertificatesAsync()
{
    try {
        CertificateManager* certManager = CertificateManager::instance();

        // LOG_INFO removed

        if (!certManager->generateSelfSignedCertificate("localhost", "QKChat", "CN", 365)) {
            LOG_ERROR("Failed to generate self-signed certificate");
        } else {
        
        }

    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during certificate initialization: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception during certificate initialization");
    }
}

void ServerManager::initializeOptionalComponentsAsync()
{

}
