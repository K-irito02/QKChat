#include "ServerManager.h"
#include "config/ConfigManager.h"
#include "security/CertificateManager.h"
#include "security/OpenSSLHelper.h"
#include "network/ThreadPoolServer.h"
#include "network/AsyncMessageQueue.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QHostAddress>
#include <QFileInfo>
#include <QTimer>

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

    LOG_INFO("Initializing QKChat Server with high-performance architecture...");

    try {
        // 初始化OpenSSL库
        if (!OpenSSLHelper::initializeOpenSSL()) {
            LOG_ERROR("Failed to initialize OpenSSL library");
            setServerState(Error);
            return false;
        }

        // 初始化配置管理器
        LOG_INFO("Creating ConfigManager instance...");
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

        LOG_INFO(QString("Application directory: %1").arg(appDir));
        LOG_INFO(QString("Looking for config file at: %1").arg(configPath));

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
                    LOG_INFO(QString("Found config file at alternate path: %1").arg(configPath));
                    found = true;
                    break;
                }
            }

            if (!found) {
                LOG_WARNING("No configuration file found in any location, using defaults");
            }
        }

        LOG_INFO("Attempting to load configuration...");
        if (!configManager->loadConfig(configPath)) {
            LOG_WARNING("Failed to load configuration file, using defaults");
        } else {
        
        }

        // 配置日志系统
        LOG_INFO("Configuring logger settings...");
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
    LOG_INFO("AsyncMessageQueue disabled to prevent duplicate message sending");

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

    LOG_INFO(QString("Starting QKChat High-Performance Server on port %1...").arg(port));

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
    

    LOG_INFO("Server features enabled:");
    LOG_INFO("  - Database connection pool with automatic scaling");
    LOG_INFO("  - Multi-threaded client handling");
    LOG_INFO("  - Async message queue with priority support");
    LOG_INFO("  - Redis caching for session management");
    LOG_INFO("  - Load balancing and rate limiting");
    
    return true;
}

void ServerManager::stopServer()
{
    if (_serverState == Stopped) {
        return;
    }

    LOG_INFO("Stopping QKChat High-Performance Server...");
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
    LOG_INFO("QKChat High-Performance Server stopped");
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
    
    LOG_INFO(QString("Client connected: %1 (Total: %2)").arg(client->clientId()).arg(_clientCount));
    emit clientConnected(_clientCount);
}

void ServerManager::onThreadPoolClientDisconnected(ClientHandler* client)
{
    _clientCount--;
    
    LOG_INFO(QString("Client disconnected: %1 (Total: %2)").arg(client->clientId()).arg(_clientCount));
    emit clientDisconnected(_clientCount);
}

void ServerManager::onThreadPoolUserLoggedIn(qint64 userId, ClientHandler* client)
{
    LOG_INFO(QString("User logged in: ID=%1, Client=%2").arg(userId).arg(client->clientId()));
    emit userLoggedIn(userId, QString("User_%1").arg(userId));
}

void ServerManager::onThreadPoolUserLoggedOut(qint64 userId)
{
    LOG_INFO(QString("User logged out: ID=%1").arg(userId));
}

void ServerManager::onProtocolUserLoggedIn(qint64 userId, const QString &clientId, const QString &sessionToken)
{
    LOG_INFO(QString("Protocol: User logged in: ID=%1, Client=%2").arg(userId).arg(clientId));
    emit userLoggedIn(userId, QString("User_%1").arg(userId));
}

void ServerManager::onProtocolUserRegistered(qint64 userId, const QString &username, const QString &email)
{
    _totalRegistrations++;
    
    LOG_INFO(QString("User registered: %1 (%2)").arg(username).arg(email));
    emit userRegistered(userId, username, email);
}

void ServerManager::onDatabaseConnectionChanged(bool connected)
{
    if (connected) {
        LOG_INFO("Database connection pool established");
    } else {
        LOG_WARNING("Database connection pool lost");
        emit serverError("Database connection pool lost");
    }
}

void ServerManager::onRedisConnectionChanged(bool connected)
{
    if (connected) {
        LOG_INFO("Redis connection established");
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
    
    LOG_INFO(QString("Initializing database connection pool: %1@%2:%3/%4 (pool: %5-%6)")
             .arg(username).arg(host).arg(port).arg(database).arg(minConnections).arg(maxConnections));
    
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
    
    LOG_INFO(QString("Initializing Redis connection: %1:%2").arg(host).arg(port));
    
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

    LOG_INFO(QString("Initializing EmailService: host=%1, port=%2, username=%3, useTls=%4")
             .arg(host).arg(port).arg(username).arg(useTls ? "true" : "false"));

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

        LOG_INFO("Initializing TLS certificates using auto-generated self-signed certificate");

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