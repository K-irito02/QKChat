#include "ClientHandler.h"
#include "ProtocolHandler.h"
#include "../utils/Logger.h"
#include <QSslCertificate>
#include <QSslKey>
#include <QSslCipher>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QThread>
#include <QApplication>
#include <QSet>
#include <QMutex>

// 静态成员初始化
int ClientHandler::s_clientCounter = 0;

ClientHandler::ClientHandler(qintptr socketDescriptor, ProtocolHandler *protocolHandler, bool useTLS, QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _protocolHandler(protocolHandler)
    , _userId(-1)
    , _state(Initialized)  // 初始状态为Initialized
    , _heartbeatTimeout(60000) // 60秒
    , _useTLS(useTLS)
    , _messagesSent(0)
    , _messagesReceived(0)
    , _bytesReceived(0)
    , _bytesSent(0)
{
    // 生成客户端ID
    _clientId = generateClientId();
    _connectTime = QDateTime::currentDateTime();
    _lastActivity = _connectTime;
    
    // 根据配置创建套接字
    if (_useTLS) {
        QSslSocket* sslSocket = new QSslSocket(this);
        _socket = sslSocket;
        
        // 连接SSL套接字信号
        connect(sslSocket, &QSslSocket::connected, this, &ClientHandler::onConnected);
        connect(sslSocket, &QSslSocket::disconnected, this, &ClientHandler::onDisconnected);
        connect(sslSocket, &QSslSocket::readyRead, this, &ClientHandler::onReadyRead);
        connect(sslSocket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
                this, &ClientHandler::onSocketError);
        connect(sslSocket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
                this, &ClientHandler::onSslErrors);
    } else {
        QTcpSocket* tcpSocket = new QTcpSocket(this);
        _socket = tcpSocket;
        
        // 连接普通TCP套接字信号
        connect(tcpSocket, &QTcpSocket::connected, this, &ClientHandler::onConnected);
        connect(tcpSocket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
        connect(tcpSocket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
        connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &ClientHandler::onSocketError);
    }
    
    // 设置套接字描述符
    if (!_socket) {
        LOG_ERROR(QString("Socket is null for client %1").arg(_clientId));
        setState(Error);
        return;
    }
    
    if (!_socket->setSocketDescriptor(socketDescriptor)) {
        LOG_ERROR(QString("Failed to set socket descriptor for client %1: %2").arg(_clientId).arg(_socket->errorString()));
        setState(Error);
        return;
    }
    
    LOG_INFO(QString("Client handler created: %1").arg(_clientId));
}

ClientHandler::~ClientHandler()
{
    // 断开所有信号连接，避免在析构过程中触发信号
    disconnect();
    
    // 断开网络连接
    if (_socket) {
        // 先断开信号连接
        _socket->disconnect();
        
        if (_socket->state() != QAbstractSocket::UnconnectedState) {
            _socket->disconnectFromHost();
            // 等待断开连接，但不要无限等待
            if (!_socket->waitForDisconnected(1000)) {
                LOG_WARNING(QString("Socket disconnect timeout for client %1").arg(_clientId));
            }
        }
        
        // 删除套接字对象
        _socket->deleteLater();
        _socket = nullptr;
    }
    
    // 清理协议处理器连接
    if (_protocolHandler) {
        // 断开与协议处理器的连接
        QObject::disconnect(_protocolHandler, nullptr, this, nullptr);
        _protocolHandler = nullptr;
    }
}

QHostAddress ClientHandler::peerAddress() const
{
    return _socket ? _socket->peerAddress() : QHostAddress();
}

bool ClientHandler::isConnected() const
{
    return _socket && _socket->state() == QAbstractSocket::ConnectedState;
}

bool ClientHandler::sendMessage(const QJsonObject &message)
{
    if (!isConnected()) {
        LOG_WARNING(QString("Cannot send message to disconnected client: %1").arg(_clientId));
        return false;
    }
    
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    // 添加消息长度前缀（4字节）
    QByteArray lengthPrefix;
    QDataStream stream(&lengthPrefix, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(data.size());
    
    QByteArray fullMessage = lengthPrefix + data;
    
    qint64 bytesWritten = _socket->write(fullMessage);
    if (bytesWritten == -1) {
        LOG_ERROR(QString("Failed to send message to client %1: %2").arg(_clientId).arg(_socket->errorString()));
        return false;
    }
    
    _socket->flush();
    _messagesSent++;
    _bytesSent += bytesWritten;
    
    updateLastActivity();
    

    return true;
}

void ClientHandler::disconnect(const QString &reason)
{
    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState) {
        // 发送断开连接消息
        if (!reason.isEmpty()) {
            QJsonObject disconnectMessage;
            disconnectMessage["action"] = "disconnect";
            disconnectMessage["reason"] = reason;
            disconnectMessage["timestamp"] = QDateTime::currentSecsSinceEpoch();
            
            sendMessage(disconnectMessage);
        }
        
        _socket->disconnectFromHost();
        
        if (_socket->state() != QAbstractSocket::UnconnectedState) {
            _socket->waitForDisconnected(3000);
        }
    }
    
    setState(Disconnected);
    
    LOG_INFO(QString("Client disconnected: %1 (Reason: %2)").arg(_clientId).arg(reason.isEmpty() ? "Normal" : reason));
}

bool ClientHandler::setTlsCertificate(const QString &certFile, const QString &keyFile)
{
    if (!_useTLS) {
        LOG_WARNING(QString("TLS certificate requested but TLS is disabled for client %1").arg(_clientId));
        return false;
    }
    
    _certFile = certFile;
    _keyFile = keyFile;
    
    if (!QFile::exists(certFile) || !QFile::exists(keyFile)) {
        LOG_ERROR(QString("TLS certificate files not found for client %1").arg(_clientId));
        return false;
    }
    
    // 加载证书
    QFile certFileObj(certFile);
    if (!certFileObj.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open certificate file: %1").arg(certFile));
        return false;
    }
    
    QSslCertificate certificate(&certFileObj, QSsl::Pem);
    if (certificate.isNull()) {
        LOG_ERROR(QString("Invalid certificate file: %1").arg(certFile));
        return false;
    }
    
    // 加载私钥
    QFile keyFileObj(keyFile);
    if (!keyFileObj.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open private key file: %1").arg(keyFile));
        return false;
    }
    
    QSslKey privateKey(&keyFileObj, QSsl::Rsa, QSsl::Pem);
    if (privateKey.isNull()) {
        LOG_ERROR(QString("Invalid private key file: %1").arg(keyFile));
        return false;
    }
    
    // 设置证书和私钥（需要转换为QSslSocket）
    QSslSocket* sslSocket = qobject_cast<QSslSocket*>(_socket);
    if (sslSocket) {
        sslSocket->setLocalCertificate(certificate);
        sslSocket->setPrivateKey(privateKey);
        LOG_INFO(QString("TLS certificate set for client %1").arg(_clientId));
        return true;
    } else {
        LOG_ERROR(QString("Socket is not SSL socket for client %1").arg(_clientId));
        return false;
    }
}

void ClientHandler::setHeartbeatTimeout(int timeout)
{
    _heartbeatTimeout = timeout;
}

bool ClientHandler::isHeartbeatTimeout() const
{
    if (_heartbeatTimeout <= 0) {
        return false;
    }
    
    qint64 elapsed = _lastActivity.msecsTo(QDateTime::currentDateTime());
    bool timeout = elapsed > _heartbeatTimeout;
    
    if (timeout) {
        LOG_WARNING(QString("Client %1 heartbeat timeout: elapsed=%2ms, timeout=%3ms")
                   .arg(_clientId).arg(elapsed).arg(_heartbeatTimeout));
    }
    
    return timeout;
}

QJsonObject ClientHandler::getClientInfo() const
{
    QJsonObject info;
    info["client_id"] = _clientId;
    info["user_id"] = _userId;
    info["state"] = static_cast<int>(_state);
    info["peer_address"] = peerAddress().toString();
    info["connect_time"] = _connectTime.toString(Qt::ISODate);
    info["last_activity"] = _lastActivity.toString(Qt::ISODate);
    info["messages_sent"] = _messagesSent;
    info["messages_received"] = _messagesReceived;
    info["bytes_sent"] = _bytesSent;
    info["bytes_received"] = _bytesReceived;
    info["use_tls"] = _useTLS;
    info["is_authenticated"] = isAuthenticated();
    
    if (_heartbeatTimeout > 0) {
        qint64 elapsed = _lastActivity.msecsTo(QDateTime::currentDateTime());
        info["heartbeat_remaining"] = qMax(0LL, _heartbeatTimeout - elapsed);
    }
    
    return info;
}

void ClientHandler::onConnected()
{
    LOG_INFO(QString("Client connected: %1 from %2").arg(_clientId).arg(peerAddress().toString()));
    
    // 记录连接信息
    if (_useTLS) {
        LOG_INFO(QString("Using SSL connection for client %1").arg(_clientId));
    } else {
        LOG_INFO(QString("Using plain TCP connection for client %1").arg(_clientId));
    }
    
    emit connected();
}

void ClientHandler::onDisconnected()
{
    // 避免在已断开状态下重复发射信号
    if (_state != Disconnected) {
        setState(Disconnected);
        LOG_INFO(QString("Client disconnected: %1").arg(_clientId));
        emit disconnected();
    }
}

void ClientHandler::onReadyRead()
{
    QByteArray data = _socket->readAll();
    _bytesReceived += data.size();
    updateLastActivity();
    
    _receiveBuffer.append(data);
    processReceivedData(_receiveBuffer);
}

void ClientHandler::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = _socket->errorString();
    LOG_ERROR(QString("Socket error for client %1: %2").arg(_clientId).arg(errorString));
    
    // 避免在错误状态下重复发射信号
    if (_state != Error) {
        setState(Error);
        emit clientError(errorString);
    }
}

void ClientHandler::onSslErrors(const QList<QSslError> &errors)
{
    if (!_useTLS) {
        LOG_WARNING(QString("SSL errors received but TLS is disabled for client %1").arg(_clientId));
        return;
    }
    
    QStringList errorStrings;
    
    for (const QSslError &error : errors) {
        QString errorStr = error.errorString();
        errorStrings << errorStr;
        LOG_WARNING(QString("SSL Error for client %1: %2").arg(_clientId).arg(errorStr));
    }
    
    QString errorString = errorStrings.join("; ");
    LOG_WARNING(QString("SSL errors detected for client %1: %2").arg(_clientId).arg(errorString));
    
    // 在生产环境中，应该根据具体的SSL错误决定是否忽略
    QSslSocket* sslSocket = qobject_cast<QSslSocket*>(_socket);
    if (sslSocket) {
        sslSocket->ignoreSslErrors();
    } else {
        LOG_ERROR(QString("Socket is not SSL socket for client %1").arg(_clientId));
    }
}

void ClientHandler::onProtocolUserLoggedIn(qint64 userId, const QString &clientId, const QString &sessionToken)
{
    Q_UNUSED(clientId)
    Q_UNUSED(sessionToken)
    _userId = userId;
    setState(Authenticated);
    emit authenticated(userId);
}

void ClientHandler::processReceivedData(const QByteArray &data)
{
    // Processing received data
    LOG_INFO(QString("Client: %1, Received data size: %2 bytes").arg(_clientId).arg(data.size()));
    
    // 将新数据添加到接收缓冲区
    _receiveBuffer.append(data);
    
    // 处理缓冲区中的所有完整消息
    while (_receiveBuffer.size() >= 4) {
        // 读取消息长度（前4字节）
        QDataStream stream(_receiveBuffer);
        stream.setByteOrder(QDataStream::BigEndian);
        
        quint32 messageLength;
        stream >> messageLength;
        
        LOG_INFO(QString("Message length: %1 bytes, Buffer size: %2 bytes").arg(messageLength).arg(_receiveBuffer.size()));
        
        // 检查消息长度是否合理
        const quint32 MAX_MESSAGE_SIZE = 64 * 1024; // 64KB
        const quint32 MAX_BUFFER_SIZE = 1024 * 1024; // 1MB
        if (messageLength > MAX_MESSAGE_SIZE) {
            LOG_ERROR(QString("Message length too large: %1 bytes, clearing buffer").arg(messageLength));
            _receiveBuffer.clear();
            return;
        }
        
        if (messageLength == 0) {
            LOG_ERROR("Invalid message length: 0, removing header");
            _receiveBuffer.remove(0, 4);
            continue;
        }
        
        // 检查缓冲区大小是否合理
        if (_receiveBuffer.size() > MAX_BUFFER_SIZE) {
            LOG_ERROR(QString("Buffer size too large: %1 bytes, clearing buffer").arg(_receiveBuffer.size()));
            _receiveBuffer.clear();
            return;
        }
        
        // 检查是否接收到完整消息
        if (_receiveBuffer.size() < 4 + messageLength) {
            LOG_INFO(QString("Incomplete message, waiting for more data. Need: %1, Have: %2")
                    .arg(4 + messageLength).arg(_receiveBuffer.size()));
            break; // 等待更多数据
        }
        
        // 提取消息数据
        QByteArray messageData = _receiveBuffer.mid(4, messageLength);
        _receiveBuffer.remove(0, 4 + messageLength);
        
        LOG_INFO(QString("Extracted message data: %1 bytes").arg(messageData.size()));
        
        // 解析JSON消息
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(messageData, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            LOG_WARNING(QString("Invalid JSON from client %1: %2").arg(_clientId).arg(parseError.errorString()));
            sendErrorResponse("", "Invalid JSON format");
            continue;
        }
        
        if (!doc.isObject()) {
            LOG_WARNING(QString("Non-object JSON from client %1").arg(_clientId));
            sendErrorResponse("", "JSON must be an object");
            continue;
        }
        
        QJsonObject message = doc.object();
        QString action = message["action"].toString();
        QString requestId = message["request_id"].toString();
        
        LOG_INFO(QString("Parsed message - Action: %1, RequestID: %2").arg(action).arg(requestId));
        
        // 检查是否为重复消息（仅对非心跳消息进行检查）
        if (action != "heartbeat" && !requestId.isEmpty()) {
            static QSet<QString> processedRequests;
            static QMutex processedRequestsMutex;
            
            QMutexLocker locker(&processedRequestsMutex);
            if (processedRequests.contains(requestId)) {
                LOG_WARNING(QString("Duplicate message detected, skipping: %1").arg(requestId));
                continue;
            }
            processedRequests.insert(requestId);
            
            // 限制已处理请求的数量，防止内存泄漏
            if (processedRequests.size() > 1000) {
                processedRequests.clear();
            }
        }
        
        _messagesReceived++;
        
        processMessage(message);
    }
    
    // Data processing completed
}

void ClientHandler::processMessage(const QJsonObject &message)
{
    QString action = message["action"].toString();
    QString requestId = message["request_id"].toString();
    
    // Processing message
    LOG_INFO(QString("Action: %1, RequestID: %2, ClientState: %3").arg(action).arg(requestId).arg(static_cast<int>(_state)));

    // 处理心跳消息
    if (action == "heartbeat") {
        // LOG_INFO removed
        handleHeartbeat(message);
        return;
    }

    // 处理认证消息（包括可用性检查）
    if (action == "login" || action == "register" || action == "send_verification_code" || 
        action == "check_username" || action == "check_email") {
        // LOG_INFO removed
        if (_state == Connected || _state == Authenticating) {
            handleAuthRequest(message);
        } else {
            LOG_WARNING(QString("Invalid state for authentication: %1").arg(static_cast<int>(_state)));
            sendErrorResponse(message["request_id"].toString(), "Invalid state for authentication");
        }
        return;
    }

    // 其他消息需要先认证
    if (!isAuthenticated()) {
        LOG_WARNING("Message requires authentication but user is not authenticated");
        sendErrorResponse(message["request_id"].toString(), "Authentication required");
        return;
    }

    // 处理已认证用户的消息
    // LOG_INFO removed
    emit messageReceived(message);
    
    // Message processing completed
}

void ClientHandler::handleAuthRequest(const QJsonObject &message)
{
    setState(Authenticating);

    if (!_protocolHandler) {
        LOG_ERROR("No ProtocolHandler available for authentication");
        sendErrorResponse(message["request_id"].toString(), "Server configuration error");
        setState(Connected);
        return;
    }

    // 断开之前的连接，避免重复连接
    QObject::disconnect(_protocolHandler, &ProtocolHandler::userLoggedIn, this, nullptr);

    // 连接协议处理器的信号（每个ClientHandler实例都需要连接）
    // 使用Qt::UniqueConnection确保不会重复连接
    connect(_protocolHandler, &ProtocolHandler::userLoggedIn,
            this, &ClientHandler::onProtocolUserLoggedIn, Qt::UniqueConnection);

    // 处理消息并获取响应
    QJsonObject response = _protocolHandler->handleMessage(message, _clientId, peerAddress().toString());

    // 发送响应
    sendMessage(response);

    // 如果认证失败，重置状态
    if (!response["success"].toBool()) {
        setState(Connected);
    }
}

void ClientHandler::handleHeartbeat(const QJsonObject &message)
{
    updateLastActivity();
    sendHeartbeatResponse();


}

void ClientHandler::sendAuthResponse(bool success, const QString &message, const QJsonObject &userData)
{
    QJsonObject response;
    response["action"] = "auth_response";
    response["success"] = success;
    response["message"] = message;
    response["timestamp"] = QDateTime::currentSecsSinceEpoch();

    if (success && !userData.isEmpty()) {
        response["user_data"] = userData;
    }

    sendMessage(response);
}

void ClientHandler::sendHeartbeatResponse()
{
    QJsonObject response;
    response["action"] = "heartbeat_response";
    response["timestamp"] = QDateTime::currentSecsSinceEpoch();
    response["server_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    sendMessage(response);
}

void ClientHandler::sendErrorResponse(const QString &requestId, const QString &error)
{
    QJsonObject response;
    response["request_id"] = requestId;
    response["action"] = "error";
    response["success"] = false;
    response["error"] = error;
    response["timestamp"] = QDateTime::currentSecsSinceEpoch();

    sendMessage(response);
}

void ClientHandler::updateLastActivity()
{
    _lastActivity = QDateTime::currentDateTime();
}

void ClientHandler::setState(ClientState state)
{
    if (_state != state) {
        ClientState oldState = _state;
        _state = state;


    }
}

QString ClientHandler::generateClientId()
{
    return QString("client_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(++s_clientCounter);
}

void ClientHandler::startProcessing()
{
    if (_state != Initialized) {
        LOG_WARNING(QString("Cannot start processing for client %1: invalid state").arg(_clientId));
        return;
    }
    
    // 检查套接字状态
    if (!_socket || _socket->state() != QAbstractSocket::ConnectedState) {
        LOG_ERROR(QString("Socket not connected for client %1").arg(_clientId));
        setState(Error);
        return;
    }
    
    setState(Connected);
    
    // 发送连接成功信号
    emit connected();
}

// 线程安全的信号发射方法
void ClientHandler::emitConnected()
{
    emit connected();
}

void ClientHandler::emitDisconnected()
{
    emit disconnected();
}

void ClientHandler::emitAuthenticated(qint64 userId)
{
    emit authenticated(userId);
}

void ClientHandler::emitMessageReceived(const QJsonObject &message)
{
    emit messageReceived(message);
}
