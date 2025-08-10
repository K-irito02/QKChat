#include "ServerManager.h"
#include "config/ConfigManager.h"
#include "security/CertificateManager.h"
#include "security/OpenSSLHelper.h"
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
    , _tcpServer(nullptr)
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

    // 首先初始化基本日志系统，避免其他组件初始化时日志调用失败
    QString basicLogDir = "D:/QT_Learn/Projects/QKChat/Server/logs";
    QDir().mkpath(basicLogDir);
    if (!Logger::initialize(basicLogDir, "Server")) {
        QString relativeLogDir = "logs";
        QDir().mkpath(relativeLogDir);
        Logger::initialize(relativeLogDir, "Server");
    }

    LOG_INFO("Initializing QKChat Server...");

    // 简化初始化流程，先确保基本功能正常
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
            LOG_INFO("ConfigManager instance created successfully");
        } catch (const std::exception& e) {
            LOG_ERROR(QString("Exception creating ConfigManager: %1").arg(e.what()));
            setServerState(Error);
            return false;
        } catch (...) {
            LOG_ERROR("Unknown exception creating ConfigManager");
            setServerState(Error);
            return false;
        }

        QString appDir = QCoreApplication::applicationDirPath();
        QString configPath = appDir + "/config/server.json";

        LOG_INFO(QString("Application directory: %1").arg(appDir));
        LOG_INFO(QString("Looking for config file at: %1").arg(configPath));

        // 检查配置文件是否存在
        QFileInfo configFile(configPath);
        if (!configFile.exists()) {
            LOG_WARNING(QString("Configuration file not found: %1").arg(configPath));
            LOG_WARNING("Using default configuration");

            // 尝试其他可能的路径
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
            LOG_INFO("Configuration loaded successfully");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during configuration initialization: %1").arg(e.what()));
        setServerState(Error);
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception during configuration initialization");
        setServerState(Error);
        return false;
    }

    // 配置日志系统（从配置文件读取）
    LOG_INFO("Configuring logger settings...");
    try {
        ConfigManager* configManager = ConfigManager::instance();
        // 配置日志系统
        Logger::setLogLevel(static_cast<Logger::LogLevel>(
            configManager->getValue("logging.level", static_cast<int>(Logger::INFO)).toInt()));
        Logger::setConsoleOutput(configManager->getValue("logging.console_output", true).toBool());
        LOG_INFO("Logger configuration completed");

    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during logger configuration: %1").arg(e.what()));
        // 继续执行，不因为日志问题而停止服务器
    } catch (...) {
        LOG_ERROR("Unknown exception during logger configuration");
        // 继续执行，不因为日志问题而停止服务器
    }

    // 同步初始化关键组件，确保服务器启动前完成
    if (!initializeDatabase()) {
        LOG_WARNING("Failed to initialize database (optional)");
    } else {
        // 执行用户状态迁移
        if (_protocolHandler && _protocolHandler->userService()) {
            if (_protocolHandler->userService()->migrateUserStatuses()) {
                LOG_INFO("User status migration completed successfully");
            } else {
                LOG_WARNING("User status migration failed");
            }
        }
    }

    if (!initializeRedis()) {
        LOG_WARNING("Failed to initialize Redis (optional)");
    }

    // 邮件服务是关键组件，必须同步初始化
    if (!initializeEmailService()) {
        LOG_ERROR("Failed to initialize email service - verification codes will not work");
    }

    // 简化证书初始化，异步处理避免阻塞
    QTimer::singleShot(200, this, &ServerManager::initializeCertificatesAsync);
    
    // 初始化TCP服务器
    try {
        if (!initializeTcpServer()) {
            LOG_ERROR("Failed to initialize TCP server");
            setServerState(Error);
            return false;
        }

        LOG_INFO("QKChat Server initialized successfully");

        // 初始化成功后，将状态设置为Stopped，以便可以启动服务器
        setServerState(Stopped);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during TCP server initialization: %1").arg(e.what()));
        setServerState(Error);
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception during TCP server initialization");
        setServerState(Error);
        return false;
    }
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

    LOG_INFO(QString("Starting QKChat Server on port %1...").arg(port));

    // 从配置文件读取TLS设置
    bool useTls = configManager->getValue("server.use_tls", true).toBool();
    
    // 启动TCP服务器
    if (!_tcpServer->startServer(port, QHostAddress::Any, useTls)) {
        LOG_ERROR("Failed to start TCP server");
        setServerState(Error);
        return false;
    }
    
    _startTime = QDateTime::currentDateTime();
    setServerState(Running);
    
    LOG_INFO(QString("QKChat Server started successfully on port %1").arg(port));
    return true;
}

void ServerManager::stopServer()
{
    if (_serverState == Stopped) {
        return;
    }

    LOG_INFO("Stopping QKChat Server...");
    setServerState(Stopping);

    // 停止TCP服务器
    if (_tcpServer) {
        _tcpServer->stopServer();
    }

    // 关闭数据库连接
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
    LOG_INFO("QKChat Server stopped");
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
    
    // 数据库状态
    if (_databaseManager) {
        stats["database_connected"] = _databaseManager->isConnected();
    }
    
    // Redis状态
    if (_redisClient) {
        stats["redis_connected"] = _redisClient->isConnected();
    }
    
    // TCP服务器统计
    if (_tcpServer) {
        QJsonObject tcpStats = _tcpServer->getServerStatistics();
        stats["tcp_server"] = tcpStats;
    }
    
    return stats;
}

int ServerManager::getClientCount() const
{
    return _clientCount;
}

int ServerManager::getOnlineUserCount() const
{
    // 这里可以从数据库查询在线用户数量
    // 简化实现，返回客户端数量
    return _clientCount;
}

void ServerManager::onTcpClientConnected(ClientHandler* client)
{
    _clientCount++;
    _totalConnections++;
    
    LOG_INFO(QString("Client connected: %1 (Total: %2)").arg(client->clientId()).arg(_clientCount));
    emit clientConnected(_clientCount);
}

void ServerManager::onTcpClientDisconnected(ClientHandler* client)
{
    _clientCount--;
    
    LOG_INFO(QString("Client disconnected: %1 (Total: %2)").arg(client->clientId()).arg(_clientCount));
    emit clientDisconnected(_clientCount);
}

void ServerManager::onProtocolUserLoggedIn(qint64 userId, const QString &clientId, const QString &sessionToken)
{
    // 可以从数据库获取用户名
    LOG_INFO(QString("User logged in: ID=%1, Client=%2").arg(userId).arg(clientId));
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
        LOG_INFO("Database connection established");
    } else {
        LOG_WARNING("Database connection lost");
        emit serverError("Database connection lost");
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

void ServerManager::setServerState(ServerState state)
{
    if (_serverState != state) {
        _serverState = state;
        emit serverStateChanged(state);
    }
}

bool ServerManager::initializeDatabase()
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
    
    bool result = _databaseManager->initialize(host, port, database, username, password);
    
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

    LOG_INFO(QString("Initializing EmailService with config: host=%1, port=%2, username=%3, useTls=%4")
             .arg(host).arg(port).arg(username).arg(useTls ? "true" : "false"));

    if (username.isEmpty() || password.isEmpty()) {
        LOG_ERROR("SMTP username or password is empty in configuration");
        return false;
    }

    return _emailService->initialize(host, port, username, password, useTls);
}

bool ServerManager::initializeTcpServer()
{
    _tcpServer = new TcpServer(this);

    _protocolHandler = new ProtocolHandler(_emailService, this);

    // 从配置文件读取TCP服务器配置
    ConfigManager* configManager = ConfigManager::instance();
    int maxClients = configManager->getValue("server.max_clients", 1000).toInt();
    int heartbeatInterval = configManager->getValue("server.heartbeat_interval", 30000).toInt();

    // 配置TCP服务器
    _tcpServer->setMaxClients(maxClients);
    _tcpServer->setHeartbeatInterval(heartbeatInterval);
    _tcpServer->setProtocolHandler(_protocolHandler);
    
    // 连接TCP服务器信号
    connect(_tcpServer, &TcpServer::clientConnected,
            this, &ServerManager::onTcpClientConnected);
    connect(_tcpServer, &TcpServer::clientDisconnected,
            this, &ServerManager::onTcpClientDisconnected);
    
    // 连接协议处理器信号
    connect(_protocolHandler, &ProtocolHandler::userLoggedIn,
            this, &ServerManager::onProtocolUserLoggedIn);
    connect(_protocolHandler, &ProtocolHandler::userRegistered,
            this, &ServerManager::onProtocolUserRegistered);
    
    return true;
}

void ServerManager::initializeCertificatesAsync()
{
    try {
        CertificateManager* certManager = CertificateManager::instance();

        LOG_INFO("Initializing TLS certificates using auto-generated self-signed certificate");

        // 直接生成自签名证书，不依赖外部证书文件
        if (!certManager->generateSelfSignedCertificate("localhost", "QKChat", "CN", 365)) {
            LOG_ERROR("Failed to generate self-signed certificate");
        } else {
            LOG_INFO("Self-signed certificate generated successfully");
        }

    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during certificate initialization: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception during certificate initialization");
    }
}

void ServerManager::initializeOptionalComponentsAsync()
{
    // 这个方法现在为空，因为关键组件已经在主初始化流程中同步初始化了
    LOG_INFO("Async optional components initialization completed (no additional components)");
}
