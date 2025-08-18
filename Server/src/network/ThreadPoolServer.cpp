#include "ThreadPoolServer.h"
#include "../network/ProtocolHandler.h"
#include "../utils/Logger.h"
#include "AsyncMessageQueue.h"
#include <QSslSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>
#include <QApplication>

// 静态成员初始化
ThreadPoolServer* ThreadPoolServer::s_instance = nullptr;
QMutex ThreadPoolServer::s_instanceMutex;

ThreadPoolServer::ThreadPoolServer(QObject *parent)
    : QTcpServer(parent)
    , _protocolHandler(nullptr)
    , _currentPoolIndex(0)
    , _totalConnections(0)
    , _activeConnections(0)
    , _rejectedConnections(0)
    , _useTLS(true)
    , _initialized(false)
    , _running(false)
{
    // 创建定时器
    _healthCheckTimer = new QTimer(this);
    _loadBalanceTimer = new QTimer(this);
    
    connect(_healthCheckTimer, &QTimer::timeout, this, &ThreadPoolServer::performHealthCheck);
    connect(_loadBalanceTimer, &QTimer::timeout, this, &ThreadPoolServer::balanceLoad);
}

ThreadPoolServer::~ThreadPoolServer()
{
    stopServer();
}

ThreadPoolServer* ThreadPoolServer::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new ThreadPoolServer();
        }
    }
    return s_instance;
}

bool ThreadPoolServer::initialize(const ServerConfig& config)
{
    if (_initialized) {
        LOG_WARNING("Thread pool server already initialized");
        return true;
    }
    
    _config = config;
    
    // 初始化线程池服务器
    
    // 创建线程池
    int poolCount = qMax(1, _config.maxThreads / 4); // 每个池最多4个线程
    for (int i = 0; i < poolCount; ++i) {
        QThreadPool* pool = new QThreadPool(this);
        pool->setMaxThreadCount(_config.maxThreads / poolCount);
        pool->setExpiryTimeout(30000); // 30秒空闲超时
        _threadPools.append(pool);
    }
    
    // 启动定时器
    _healthCheckTimer->start(30000); // 30秒健康检查
    if (_config.enableLoadBalancing) {
        _loadBalanceTimer->start(10000); // 10秒负载均衡
    }
    
    _initialized = true;
    _startTime = QDateTime::currentDateTime();
    
    // 线程池服务器初始化完成
    
    return true;
}

bool ThreadPoolServer::startServer(quint16 port, const QHostAddress &address, bool useTLS)
{
    if (!_initialized) {
        LOG_ERROR("Server not initialized");
        return false;
    }
    
    if (_running) {
        LOG_WARNING("Server is already running");
        return true;
    }
    
    _useTLS = useTLS;
    
    if (!listen(address, port)) {
        QString error = QString("Failed to start server: %1").arg(errorString());
        LOG_ERROR(error);
        emit serverError(error);
        return false;
    }
    
    _running = true;
    
    // 线程池服务器已启动
    
    return true;
}

void ThreadPoolServer::stopServer()
{
    if (!_running) {
        return;
    }
    
    // 停止线程池服务器
    
    _running = false;
    
    // 停止定时器
    _healthCheckTimer->stop();
    _loadBalanceTimer->stop();
    
    // 断开所有客户端连接
    QMutexLocker locker(&_clientsMutex);
    for (auto it = _clients.begin(); it != _clients.end(); ++it) {
        ClientHandler* client = it.value();
        client->disconnect("Server shutting down");
        client->deleteLater();
    }
    _clients.clear();
    _userClients.clear();
    locker.unlock();
    
    // 等待所有线程池完成
    for (QThreadPool* pool : _threadPools) {
        pool->waitForDone(5000);
    }
    
    close();
    
    // 线程池服务器已停止
}

void ThreadPoolServer::setProtocolHandler(ProtocolHandler *protocolHandler)
{
    _protocolHandler = protocolHandler;
}

QJsonObject ThreadPoolServer::getServerStatistics() const
{
    QMutexLocker locker(&_clientsMutex);
    
    QJsonObject stats;
    stats["listening"] = isListening();
    stats["server_address"] = serverAddress().toString();
    stats["server_port"] = serverPort();
    stats["active_connections"] = _activeConnections.loadAcquire();
    stats["total_connections"] = _totalConnections.loadAcquire();
    stats["rejected_connections"] = _rejectedConnections.loadAcquire();
    stats["authenticated_clients"] = _userClients.size();
    stats["max_clients"] = _config.maxClients;
    stats["use_tls"] = _useTLS;
    
    // 线程池统计
    QJsonArray poolStats;
    for (int i = 0; i < _threadPools.size(); ++i) {
        QThreadPool* pool = _threadPools[i];
        QJsonObject poolStat;
        poolStat["pool_id"] = i;
        poolStat["active_threads"] = pool->activeThreadCount();
        poolStat["max_threads"] = pool->maxThreadCount();
        poolStats.append(poolStat);
    }
    stats["thread_pools"] = poolStats;
    
    if (_startTime.isValid()) {
        stats["uptime_seconds"] = _startTime.secsTo(QDateTime::currentDateTime());
        stats["start_time"] = _startTime.toString(Qt::ISODate);
    }
    
    return stats;
}

int ThreadPoolServer::clientCount() const
{
    return _activeConnections.loadAcquire();
}

void ThreadPoolServer::broadcastMessage(const QJsonObject &message)
{
    // 直接发送广播消息，避免AsyncMessageQueue重复发送
    QMutexLocker locker(&_clientsMutex);

    for (auto it = _clients.begin(); it != _clients.end(); ++it) {
        ClientHandler* client = it.value();
        if (client && client->isAuthenticated()) {
            client->sendMessage(message);
        }
    }

    // 消息已广播给所有认证客户端
}

bool ThreadPoolServer::sendMessageToUser(qint64 userId, const QJsonObject &message)
{
    // 直接发送消息给指定用户，避免AsyncMessageQueue重复发送
    QMutexLocker locker(&_clientsMutex);

    ClientHandler* client = _userClients.value(userId, nullptr);
    if (client && client->isAuthenticated()) {
        bool success = client->sendMessage(message);
        if (success) {
            // 消息已发送给指定用户
        } else {
            LOG_WARNING(QString("Failed to send message to user %1").arg(userId));
        }
        return success;
    }

    LOG_WARNING(QString("Cannot send message to user %1: not connected or not authenticated").arg(userId));
    return false;
}

void ThreadPoolServer::incomingConnection(qintptr socketDescriptor)
{
    // 检查连接数限制
    if (_activeConnections.loadAcquire() >= _config.maxClients) {
        LOG_WARNING(QString("Rejected connection: maximum clients reached (%1)").arg(_config.maxClients));
        _rejectedConnections.fetchAndAddOrdered(1);
        
        // 直接关闭连接，不消耗描述符
        QTcpSocket tempSocket;
        tempSocket.setSocketDescriptor(socketDescriptor);
        tempSocket.disconnectFromHost();
        
        return;
    }
    
    // 选择最佳线程池
    QThreadPool* selectedPool = selectBestThreadPool();
    
    // 创建客户端处理任务
    ClientTask* task = new ClientTask(socketDescriptor, this, _protocolHandler, _useTLS);
    task->setAutoDelete(true);
    
    // 提交任务到线程池
    selectedPool->start(task);
    
    _totalConnections.fetchAndAddOrdered(1);
    _activeConnections.fetchAndAddOrdered(1);
    

}

void ThreadPoolServer::onClientConnected(ClientHandler* client)
{
    QMutexLocker locker(&_clientsMutex);
    _clients[client->clientId()] = client;
    
    // 客户端已连接
    
    emit clientConnected(client);
}

void ThreadPoolServer::onClientDisconnected(ClientHandler* client)
{
    if (!client) {
        LOG_WARNING("Received disconnect signal from null client");
        return;
    }
    
    QString clientId = client->clientId();
    qint64 userId = client->userId();
    QHostAddress clientAddress = client->peerAddress();
    
    {
        QMutexLocker locker(&_clientsMutex);
        
        // 从客户端列表中移除
        if (_clients.contains(clientId)) {
            _clients.remove(clientId);
        }
        
        // 从用户客户端映射中移除
        if (userId > 0 && _userClients.contains(userId)) {
            _userClients.remove(userId);
        }
    }
    
    // 更新IP连接计数
    updateIPCount(clientAddress, -1);
    
    _activeConnections.fetchAndSubOrdered(1);
    
    // 客户端已断开连接
    
    emit clientDisconnected(client);
    
    if (userId > 0) {
        emit userLoggedOut(userId);
    }
    
    // 使用更安全的删除策略
    // 1. 先断开所有信号连接，防止在删除过程中触发信号
    client->disconnect();
    
    // 2. 使用更长的延迟确保所有信号处理完成
    // 3. 使用QTimer::singleShot确保在主线程中删除
    QTimer::singleShot(1000, [client]() {
        if (client) {
            // 再次断开连接，确保安全
            client->disconnect();
            client->deleteLater();
        }
    });
}

void ThreadPoolServer::onClientAuthenticated(qint64 userId, ClientHandler* client)
{
    QMutexLocker locker(&_clientsMutex);
    
    // 检查是否已有该用户的连接
    if (_userClients.contains(userId)) {
        ClientHandler* existingClient = _userClients[userId];
        LOG_WARNING(QString("User %1 already connected, disconnecting previous session").arg(userId));
        existingClient->disconnect("New session started");
    }
    
    // 添加到用户客户端映射
    _userClients[userId] = client;
    
    // 客户端认证成功
    emit userLoggedIn(userId, client);
}

void ThreadPoolServer::onClientMessageReceived(ClientHandler* client, const QJsonObject &message)
{
    // Processing client message
    
    if (!client) {
        LOG_WARNING("Received message from null client");
        return;
    }
    
    // 处理客户端消息
    
    // 线程安全检查：确保client对象仍然有效
    QString clientId;
    try {
        clientId = client->clientId();
    } catch (...) {
        LOG_ERROR("Exception while getting client ID");
        return;
    }
    
    if (clientId.isEmpty()) {
        LOG_WARNING("Client ID is empty, client may be invalid");
        return;
    }
    
    QString action = message["action"].toString();
    
    // 处理聊天消息
    if (action.startsWith("friend_") || action.startsWith("message_") || 
        action.startsWith("status_") || action == "heartbeat" ||
        action == "send_message" || action == "get_chat_history" || action == "get_chat_sessions") {
        
        // 路由聊天消息到协议处理器
        
        // 使用已存在的ProtocolHandler实例
        if (!_protocolHandler) {
            LOG_ERROR("ProtocolHandler instance not available");
            return;
        }
        
        // 调用协议处理器
        
        // 获取客户端IP地址
        QString clientIP;
        try {
            clientIP = client->peerAddress().toString();
        } catch (...) {
            LOG_ERROR("Exception while getting client IP");
            return;
        }
        
        // 处理消息并获取响应
        QJsonObject response = _protocolHandler->handleMessage(message, clientId, clientIP);
        
        // 协议处理器调用成功
        
        // 发送响应给客户端
        client->sendMessage(response);
        
        // 聊天消息处理完成
    } else {
        LOG_WARNING(QString("Unknown message type: %1").arg(action));
    }
    
    // Client message processed
}

void ThreadPoolServer::performHealthCheck()
{

    
    // 检查线程池状态
    for (int i = 0; i < _threadPools.size(); ++i) {
        QThreadPool* pool = _threadPools[i];
        int activeThreads = pool->activeThreadCount();
        int maxThreads = pool->maxThreadCount();
        
        if (activeThreads > maxThreads * 0.8) {
            LOG_WARNING(QString("Thread pool %1 is under high load: %2/%3 threads active")
                       .arg(i).arg(activeThreads).arg(maxThreads));
        }
    }
    
    // 记录统计信息

}

void ThreadPoolServer::balanceLoad()
{
    if (!_config.enableLoadBalancing) {
        return;
    }
    
    // 动态调整线程池大小
    for (QThreadPool* pool : _threadPools) {
        int activeThreads = pool->activeThreadCount();
        int maxThreads = pool->maxThreadCount();
        
        // 如果负载高，增加线程数
        if (activeThreads > maxThreads * 0.8 && maxThreads < _config.maxThreads / _threadPools.size()) {
            pool->setMaxThreadCount(maxThreads + 1);
    
        }
        // 如果负载低，减少线程数
        else if (activeThreads < maxThreads * 0.3 && maxThreads > _config.minThreads / _threadPools.size()) {
            pool->setMaxThreadCount(maxThreads - 1);
    
        }
    }
}

bool ThreadPoolServer::checkIPLimit(const QHostAddress& address)
{
    QMutexLocker locker(&_ipMutex);
    
    QString addressString = address.toString();
    int currentConnections = _ipConnections.value(addressString, 0);
    return currentConnections < _config.maxConnectionsPerIP;
}

void ThreadPoolServer::updateIPCount(const QHostAddress& address, int increment)
{
    QMutexLocker locker(&_ipMutex);
    
    QString addressString = address.toString();
    int currentCount = _ipConnections.value(addressString, 0);
    int newCount = currentCount + increment;
    
    if (newCount <= 0) {
        _ipConnections.remove(addressString);
    } else {
        _ipConnections[addressString] = newCount;
    }
}

QThreadPool* ThreadPoolServer::selectBestThreadPool()
{
    if (_threadPools.isEmpty()) {
        return QThreadPool::globalInstance();
    }
    
    if (!_config.enableLoadBalancing) {
        // 简单轮询
        int index = _currentPoolIndex.fetchAndAddOrdered(1) % _threadPools.size();
        return _threadPools[index];
    }
    
    // 选择负载最低的线程池
    QThreadPool* bestPool = _threadPools[0];
    int minLoad = bestPool->activeThreadCount();
    
    for (int i = 1; i < _threadPools.size(); ++i) {
        QThreadPool* pool = _threadPools[i];
        int load = pool->activeThreadCount();
        if (load < minLoad) {
            minLoad = load;
            bestPool = pool;
        }
    }
    
    return bestPool;
}

// ClientTask 实现
ThreadPoolServer::ClientTask::ClientTask(qintptr socketDescriptor, ThreadPoolServer* server, 
                                        ProtocolHandler* protocolHandler, bool useTLS)
    : _socketDescriptor(socketDescriptor)
    , _server(server)
    , _protocolHandler(protocolHandler)
    , _useTLS(useTLS)
{
}

void ThreadPoolServer::ClientTask::run()
{
    // Starting client task
    
    // 获取主线程，确保线程安全
    QThread* mainThread = nullptr;
    if (QApplication::instance()) {
        mainThread = QApplication::instance()->thread();
    } else {
        // 如果没有QApplication实例，使用当前线程
        mainThread = QThread::currentThread();
        LOG_WARNING("QApplication instance not available, using current thread");
    }
    
    // 检查线程环境
    
    // 在当前线程中创建客户端处理器，确保套接字描述符有效
    // 创建客户端处理器
    ClientHandler* client = new ClientHandler(_socketDescriptor, _protocolHandler, _useTLS);
    
    // 客户端处理器创建完成
    
    // 将客户端移动到主线程，确保线程安全
    if (mainThread && mainThread != client->thread()) {
        // 移动到主线程
        client->moveToThread(mainThread);
        client->setParent(_server);
    }
    
    // 在主线程中连接信号
    QMetaObject::invokeMethod(_server, [this, client]() {
        // 连接信号
        
        // 连接信号到服务器
        QObject::connect(client, &ClientHandler::connected, _server, 
                        [client]() {
            // 客户端连接信号
        }, Qt::QueuedConnection);
        
        QObject::connect(client, &ClientHandler::disconnected, _server, 
                        [client]() {
            // 客户端断开信号
        }, Qt::QueuedConnection);
        
        QObject::connect(client, &ClientHandler::authenticated, _server, 
                        [client](qint64 userId) {
            // 客户端认证信号
        }, Qt::QueuedConnection);
        
        QObject::connect(client, &ClientHandler::clientError, _server, 
                        [client](const QString &error) {
            LOG_ERROR(QString("Client error: %1, Client: %2").arg(error).arg(client->clientId()));
            if (client) {
                QMetaObject::invokeMethod(client, "emitDisconnected", Qt::QueuedConnection);
            }
        }, Qt::QueuedConnection);
        
        QObject::connect(client, &ClientHandler::messageReceived, _server, 
                        [client](const QJsonObject &message) {
            // 检查对象有效性
            if (!client) {
                LOG_ERROR("Client pointer is null in messageReceived lambda");
                return;
            }
            
            // 使用传入的服务器实例，而不是单例
            ThreadPoolServer* server = qobject_cast<ThreadPoolServer*>(client->parent());
            if (!server) {
                LOG_ERROR("Server instance is null in messageReceived lambda");
                return;
            }
            
            try {
                server->onClientMessageReceived(client, message);
            } catch (...) {
                LOG_ERROR("Exception occurred in server->onClientMessageReceived");
            }
        }, Qt::QueuedConnection);
        
        // 启动客户端处理器
        QMetaObject::invokeMethod(client, "startProcessing", Qt::QueuedConnection);
        
    }, Qt::QueuedConnection);
    
    // Client task completed
}
