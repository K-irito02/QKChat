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

// 静态成员初始化
int ClientHandler::s_clientCounter = 0;

ClientHandler::ClientHandler(qintptr socketDescriptor, ProtocolHandler *protocolHandler, bool useTLS, QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _protocolHandler(protocolHandler)
    , _userId(-1)
    , _state(Connected)
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
    
    // 创建SSL套接字
    _socket = new QSslSocket(this);
    
    // 连接信号
    connect(_socket, &QSslSocket::connected, this, &ClientHandler::onConnected);
    connect(_socket, &QSslSocket::disconnected, this, &ClientHandler::onDisconnected);
    connect(_socket, &QSslSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            this, &ClientHandler::onSocketError);
    connect(_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            this, &ClientHandler::onSslErrors);
    
    // 设置套接字描述符
    if (!_socket->setSocketDescriptor(socketDescriptor)) {
        LOG_ERROR(QString("Failed to set socket descriptor for client %1").arg(_clientId));
        setState(Error);
        return;
    }
    

}

ClientHandler::~ClientHandler()
{
    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->disconnectFromHost();
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
    
    // 设置证书和私钥
    _socket->setLocalCertificate(certificate);
    _socket->setPrivateKey(privateKey);
    

    return true;
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
        LOG_INFO(QString("SSL temporarily disabled for client %1 - using plain TCP connection").arg(_clientId));
    } else {
        LOG_INFO(QString("Using plain TCP connection for client %1").arg(_clientId));
    }
    
    emit connected();
}

void ClientHandler::onDisconnected()
{
    setState(Disconnected);
    LOG_INFO(QString("Client disconnected: %1").arg(_clientId));
    emit disconnected();
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
    
    setState(Error);
    emit clientError(errorString);
}

void ClientHandler::onSslErrors(const QList<QSslError> &errors)
{
    QStringList errorStrings;
    
    for (const QSslError &error : errors) {
        QString errorStr = error.errorString();
        errorStrings << errorStr;
        LOG_WARNING(QString("SSL Error for client %1: %2").arg(_clientId).arg(errorStr));
    }
    
    QString errorString = errorStrings.join("; ");
    LOG_WARNING(QString("SSL errors detected for client %1: %2").arg(_clientId).arg(errorString));
    
    // 在生产环境中，应该根据具体的SSL错误决定是否忽略
    _socket->ignoreSslErrors();
}

void ClientHandler::processReceivedData(const QByteArray &data)
{
    QByteArray buffer = data; // 创建可修改的副本
    while (buffer.size() >= 4) {
        // 读取消息长度（前4字节）
        QDataStream stream(&buffer, QIODevice::ReadOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        
        quint32 messageLength;
        stream >> messageLength;
        
        // 检查是否接收到完整消息
        if (buffer.size() < 4 + messageLength) {
            break; // 等待更多数据
        }
        
        // 提取消息数据
        QByteArray messageData = buffer.mid(4, messageLength);
        buffer.remove(0, 4 + messageLength);
        
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
        _messagesReceived++;
        
    
        
        processMessage(message);
    }
}

void ClientHandler::processMessage(const QJsonObject &message)
{
    QString action = message["action"].toString();

    // 处理心跳消息
    if (action == "heartbeat") {
        handleHeartbeat(message);
        return;
    }

    // 处理认证消息
    if (action == "login" || action == "register" || action == "send_verification_code") {
        if (_state == Connected || _state == Authenticating) {
            handleAuthRequest(message);
        } else {
            sendErrorResponse(message["request_id"].toString(), "Invalid state for authentication");
        }
        return;
    }

    // 其他消息需要先认证
    if (!isAuthenticated()) {
        sendErrorResponse(message["request_id"].toString(), "Authentication required");
        return;
    }

    // 处理已认证用户的消息
    emit messageReceived(message);
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

    // 连接协议处理器的信号（每个ClientHandler实例都需要连接）
    // 使用Qt::UniqueConnection确保不会重复连接
    connect(_protocolHandler, &ProtocolHandler::userLoggedIn,
            this, [this](qint64 userId, const QString &clientId, const QString &sessionToken) {
                Q_UNUSED(clientId)
                Q_UNUSED(sessionToken)
                _userId = userId;
                setState(Authenticated);
                emit authenticated(userId);
            }, Qt::UniqueConnection);

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
