#include "TcpServer.h"
#include "../utils/Logger.h"
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QMutexLocker>
#include <QJsonDocument>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
    , _useTLS(true)
    , _heartbeatInterval(30000) // 30秒
    , _maxClients(1000)
    , _clientIdCounter(0)
    , _totalConnections(0)
    , _totalMessages(0)
{
    // 创建心跳定时器
    _heartbeatTimer = new QTimer(this);
    _heartbeatTimer->setInterval(_heartbeatInterval);
    connect(_heartbeatTimer, &QTimer::timeout, this, &TcpServer::onHeartbeatTimer);
}

TcpServer::~TcpServer()
{
    stopServer();
}

bool TcpServer::startServer(quint16 port, const QHostAddress &address, bool useTLS)
{
    _useTLS = useTLS;
    
    if (isListening()) {
        LOG_WARNING("TCP Server is already listening");
        return true;
    }
    
    if (!listen(address, port)) {
        QString error = QString("Failed to start TCP server: %1").arg(errorString());
        LOG_ERROR(error);
        emit serverError(error);
        return false;
    }
    
    _startTime = QDateTime::currentDateTime();
    _heartbeatTimer->start();
    
    LOG_INFO(QString("TCP Server started on %1:%2 (TLS: %3)")
             .arg(address.toString()).arg(port).arg(_useTLS ? "Yes" : "No"));
    
    return true;
}

void TcpServer::stopServer()
{
    if (!isListening()) {
        return;
    }
    
    LOG_INFO("Stopping TCP Server...");
    
    _heartbeatTimer->stop();
    
    // 断开所有客户端连接
    QMutexLocker locker(&_clientsMutex);
    for (auto it = _clients.begin(); it != _clients.end(); ++it) {
        ClientHandler* client = it.value();
        client->disconnect("Server shutting down");
        client->deleteLater();
    }
    _clients.clear();
    _userClients.clear();
    
    close();
    
    LOG_INFO("TCP Server stopped");
}

int TcpServer::clientCount() const
{
    QMutexLocker locker(&_clientsMutex);
    return _clients.size();
}

QList<ClientHandler*> TcpServer::getClients() const
{
    QMutexLocker locker(&_clientsMutex);
    return _clients.values();
}

ClientHandler* TcpServer::getClientByUserId(qint64 userId)
{
    QMutexLocker locker(&_clientsMutex);
    return _userClients.value(userId, nullptr);
}

void TcpServer::broadcastMessage(const QJsonObject &message)
{
    QMutexLocker locker(&_clientsMutex);
    
    for (auto it = _clients.begin(); it != _clients.end(); ++it) {
        ClientHandler* client = it.value();
        if (client->isAuthenticated()) {
            client->sendMessage(message);
        }
    }
    
    _totalMessages += _clients.size();
    

}

bool TcpServer::sendMessageToUser(qint64 userId, const QJsonObject &message)
{
    QMutexLocker locker(&_clientsMutex);
    
    ClientHandler* client = _userClients.value(userId, nullptr);
    if (client && client->isAuthenticated()) {
        bool success = client->sendMessage(message);
        if (success) {
            _totalMessages++;
        }
        return success;
    }
    
    LOG_WARNING(QString("Cannot send message to user %1: not connected").arg(userId));
    return false;
}

bool TcpServer::disconnectUser(qint64 userId)
{
    QMutexLocker locker(&_clientsMutex);
    
    ClientHandler* client = _userClients.value(userId, nullptr);
    if (client) {
        client->disconnect("Disconnected by server");
        return true;
    }
    
    return false;
}

bool TcpServer::setTlsCertificate(const QString &certFile, const QString &keyFile)
{
    _certFile = certFile;
    _keyFile = keyFile;
    
    // 验证证书文件
    if (!QFile::exists(certFile)) {
        LOG_ERROR(QString("Certificate file not found: %1").arg(certFile));
        return false;
    }
    
    if (!QFile::exists(keyFile)) {
        LOG_ERROR(QString("Private key file not found: %1").arg(keyFile));
        return false;
    }
    
    LOG_INFO(QString("TLS certificate configured: %1").arg(certFile));
    return true;
}

void TcpServer::setHeartbeatInterval(int interval)
{
    _heartbeatInterval = interval;
    _heartbeatTimer->setInterval(interval);
    
    LOG_INFO(QString("Heartbeat interval set to %1ms").arg(interval));
}

void TcpServer::setMaxClients(int maxClients)
{
    _maxClients = maxClients;
    
    LOG_INFO(QString("Maximum clients set to %1").arg(maxClients));
}

QJsonObject TcpServer::getServerStatistics() const
{
    QMutexLocker locker(&_clientsMutex);
    
    QJsonObject stats;
    stats["listening"] = isListening();
    stats["server_address"] = serverAddress().toString();
    stats["server_port"] = serverPort();
    stats["client_count"] = _clients.size();
    stats["authenticated_clients"] = _userClients.size();
    stats["total_connections"] = _totalConnections;
    stats["total_messages"] = _totalMessages;
    stats["max_clients"] = _maxClients;
    stats["heartbeat_interval"] = _heartbeatInterval;
    stats["use_tls"] = _useTLS;
    
    if (_startTime.isValid()) {
        stats["uptime_seconds"] = _startTime.secsTo(QDateTime::currentDateTime());
        stats["start_time"] = _startTime.toString(Qt::ISODate);
    }
    
    return stats;
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    QMutexLocker locker(&_clientsMutex);
    
    // 检查客户端数量限制
    if (_clients.size() >= _maxClients) {
        LOG_WARNING(QString("Rejected connection: maximum clients reached (%1)").arg(_maxClients));
        
        // 创建临时socket发送拒绝消息
        QTcpSocket tempSocket;
        tempSocket.setSocketDescriptor(socketDescriptor);
        
        QJsonObject rejectMessage;
        rejectMessage["action"] = "connection_rejected";
        rejectMessage["reason"] = "Server full";
        rejectMessage["max_clients"] = _maxClients;
        
        QJsonDocument doc(rejectMessage);
        tempSocket.write(doc.toJson(QJsonDocument::Compact));
        tempSocket.flush();
        tempSocket.disconnectFromHost();
        
        return;
    }
    
    // 创建客户端处理器
    ClientHandler* client = new ClientHandler(socketDescriptor, _useTLS, this);
    
    // 设置TLS证书
    if (_useTLS && !_certFile.isEmpty() && !_keyFile.isEmpty()) {
        client->setTlsCertificate(_certFile, _keyFile);
    }
    
    // 设置心跳超时 - 增加超时时间以避免误判
    client->setHeartbeatTimeout(_heartbeatInterval * 3); // 3倍心跳间隔作为超时
    
    // 连接信号
    connect(client, &ClientHandler::connected, this, [this, client]() {
        LOG_INFO(QString("Client connected: %1").arg(client->clientId()));
        emit clientConnected(client);
    });
    
    connect(client, &ClientHandler::disconnected, this, &TcpServer::onClientDisconnected);
    connect(client, &ClientHandler::authenticated, this, &TcpServer::onClientAuthenticated);
    connect(client, &ClientHandler::clientError, this, &TcpServer::onClientError);
    
    // 添加到客户端列表
    _clients[client->clientId()] = client;
    _totalConnections++;
    

}

void TcpServer::onClientDisconnected()
{
    ClientHandler* client = qobject_cast<ClientHandler*>(sender());
    if (!client) {
        LOG_WARNING("Received disconnect signal from unknown client");
        return;
    }
    
    QString clientId = client->clientId();
    qint64 userId = client->userId();
    

    
    {
        QMutexLocker locker(&_clientsMutex);
        
        // 从客户端列表中移除
        if (_clients.contains(clientId)) {
            _clients.remove(clientId);
        
        }
        
        // 从用户客户端映射中移除
        if (userId > 0 && _userClients.contains(userId)) {
            _userClients.remove(userId);
            emit userLoggedOut(userId);
        
        }
    }
    
    LOG_INFO(QString("Client disconnected: %1 (Total: %2)")
             .arg(clientId).arg(_clients.size()));
    
    emit clientDisconnected(client);
    
    // 延迟删除客户端对象，避免在信号处理中删除
    QTimer::singleShot(0, client, &ClientHandler::deleteLater);
}

void TcpServer::onClientAuthenticated(qint64 userId)
{
    ClientHandler* client = qobject_cast<ClientHandler*>(sender());
    if (!client) {
        return;
    }
    
    QMutexLocker locker(&_clientsMutex);
    
    // 检查是否已有该用户的连接
    if (_userClients.contains(userId)) {
        ClientHandler* existingClient = _userClients[userId];
        LOG_WARNING(QString("User %1 already connected, disconnecting previous session").arg(userId));
        existingClient->disconnect("New session started");
    }
    
    // 添加到用户客户端映射
    _userClients[userId] = client;
    
    LOG_INFO(QString("Client authenticated: %1 -> User %2").arg(client->clientId()).arg(userId));
    emit userLoggedIn(userId, client);
}

void TcpServer::onClientError(const QString &error)
{
    ClientHandler* client = qobject_cast<ClientHandler*>(sender());
    if (client) {
        LOG_ERROR(QString("Client error [%1]: %2").arg(client->clientId()).arg(error));
    } else {
        LOG_ERROR(QString("Client error: %1").arg(error));
    }
}

void TcpServer::onHeartbeatTimer()
{

    
    // 避免在定时器回调中进行耗时操作
    QTimer::singleShot(0, this, [this]() {
        checkClientHeartbeats();
        cleanupClients();
    });
}

void TcpServer::cleanupClients()
{
    // 清理已断开的客户端在onClientDisconnected中处理
    // 这里可以添加其他清理逻辑
    
    // 监控资源使用情况
    static int checkCount = 0;
    checkCount++;
    
    if (checkCount % 10 == 0) { // 每10次检查记录一次

    }
}

void TcpServer::checkClientHeartbeats()
{
    // 避免长时间持有锁，先收集需要处理的客户端
    QList<ClientHandler*> timeoutClients;
    
    {
        QMutexLocker locker(&_clientsMutex);
        
        // 快速检查，避免在锁内进行耗时操作
        for (auto it = _clients.begin(); it != _clients.end(); ++it) {
            ClientHandler* client = it.value();
            if (client && client->isHeartbeatTimeout()) {
                timeoutClients.append(client);
            }
        }
    }
    
    // 在锁外处理超时客户端，避免死锁
    for (ClientHandler* client : timeoutClients) {
        if (client) {
            LOG_WARNING(QString("Client heartbeat timeout: %1").arg(client->clientId()));
            client->disconnect("Heartbeat timeout");
        }
    }
    

}

QString TcpServer::generateClientId()
{
    return QString("client_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(++_clientIdCounter);
}
