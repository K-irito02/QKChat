#include "NetworkClient.h"
#include "../utils/Logger.h"
#include <QUuid>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QStandardPaths>
#include <QMutexLocker>

// 静态成员初始化
int NetworkClient::s_requestCounter = 0;

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _connectionState(Disconnected)
    , _serverPort(0)
    , _useTLS(true)
    , _connectionTimeout(10000)  // 10秒
    , _heartbeatInterval(30000)  // 30秒
{
    // 创建SSL套接字
    _socket = new QSslSocket(this);
    
    // 创建定时器
    _connectionTimer = new QTimer(this);
    _connectionTimer->setSingleShot(true);
    
    _heartbeatTimer = new QTimer(this);
    
    // 连接信号
    connect(_socket, &QSslSocket::connected, this, &NetworkClient::onConnected);
    connect(_socket, &QSslSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(_socket, &QSslSocket::readyRead, this, &NetworkClient::onReadyRead);
    connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            this, &NetworkClient::onSocketError);
    connect(_socket, &QSslSocket::sslErrors, this, &NetworkClient::onSslErrors);
    
    connect(_connectionTimer, &QTimer::timeout, this, &NetworkClient::onConnectionTimeout);
    connect(_heartbeatTimer, &QTimer::timeout, this, &NetworkClient::onHeartbeatTimeout);
    
    // 配置SSL
    configureSsl();
}

NetworkClient::~NetworkClient()
{
    disconnectFromServer();
}

bool NetworkClient::connectToServer(const QString &host, quint16 port, bool useTLS)
{
    if (_connectionState == Connected || _connectionState == Connecting) {
        LOG_WARNING("Already connected or connecting to server");
        return false;
    }
    
    _serverHost = host;
    _serverPort = port;
    _useTLS = useTLS;
    
    setConnectionState(Connecting);
    
    LOG_INFO(QString("Connecting to server %1:%2 (TLS: %3)")
             .arg(host).arg(port).arg(useTLS ? "Yes" : "No"));
    
    // 启动连接超时定时器
    _connectionTimer->start(_connectionTimeout);
    
    if (_useTLS) {
        _socket->connectToHostEncrypted(host, port);
    } else {
        _socket->connectToHost(host, port);
    }
    
    return true;
}

void NetworkClient::disconnectFromServer()
{
    if (_connectionState == Disconnected) {
        return;
    }
    
    LOG_INFO("Disconnecting from server");
    
    _connectionTimer->stop();
    _heartbeatTimer->stop();
    
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->disconnectFromHost();
        
        // 等待断开连接
        if (_socket->state() != QAbstractSocket::UnconnectedState) {
            _socket->waitForDisconnected(3000);
        }
    }
    
    setConnectionState(Disconnected);
}

QString NetworkClient::sendLoginRequest(const QString &username, const QString &passwordHash, bool rememberMe)
{
    QJsonObject request;
    request["action"] = "login";
    request["username"] = username;
    request["password_hash"] = passwordHash;
    request["remember_me"] = rememberMe;
    request["client_version"] = "1.0.0";
    request["platform"] = "Windows";
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "login";
        LOG_INFO(QString("Sent login request for user: %1").arg(username));
    }
    
    return requestId;
}

QString NetworkClient::sendRegisterRequest(const QString &username, const QString &email, 
                                          const QString &passwordHash, const QString &verificationCode)
{
    QJsonObject request;
    request["action"] = "register";
    request["username"] = username;
    request["email"] = email;
    request["password_hash"] = passwordHash;
    request["verification_code"] = verificationCode;
    request["client_version"] = "1.0.0";
    request["platform"] = "Windows";
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "register";
        LOG_INFO(QString("Sent register request for user: %1, email: %2").arg(username).arg(email));
    }
    
    return requestId;
}

QString NetworkClient::sendVerificationCodeRequest(const QString &email)
{
    QJsonObject request;
    request["action"] = "send_verification_code";
    request["email"] = email;
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "verification_code";
        LOG_INFO(QString("Sent verification code request for email: %1").arg(email));
    }
    
    return requestId;
}

void NetworkClient::sendHeartbeat()
{
    if (!isConnected()) {
        return;
    }
    
    QJsonObject request;
    request["action"] = "heartbeat";
    request["timestamp"] = QDateTime::currentSecsSinceEpoch();
    
    sendJsonRequest(request);
}

void NetworkClient::setConnectionTimeout(int timeout)
{
    _connectionTimeout = timeout;
}

void NetworkClient::setHeartbeatInterval(int interval)
{
    _heartbeatInterval = interval;
    if (_heartbeatTimer->isActive()) {
        _heartbeatTimer->start(interval);
    }
}

void NetworkClient::onConnected()
{
    _connectionTimer->stop();
    setConnectionState(Connected);
    
    LOG_INFO("Connected to server successfully");
    
    // 启动心跳定时器
    _heartbeatTimer->start(_heartbeatInterval);
}

void NetworkClient::onDisconnected()
{
    _connectionTimer->stop();
    _heartbeatTimer->stop();
    
    LOG_INFO("Disconnected from server");
    
    setConnectionState(Disconnected);
    
    // 清空接收缓冲区和待处理请求
    _receiveBuffer.clear();
    _pendingRequests.clear();
}

void NetworkClient::onReadyRead()
{
    QByteArray data = _socket->readAll();
    _receiveBuffer.append(data);
    
    // 处理接收到的数据
    processReceivedData(_receiveBuffer);
}

void NetworkClient::onSocketError(QAbstractSocket::SocketError error)
{
    _connectionTimer->stop();
    _heartbeatTimer->stop();
    
    QString errorString = _socket->errorString();
    LOG_ERROR(QString("Socket error: %1 (%2)").arg(errorString).arg(error));
    
    setConnectionState(Error);
    emit networkError(errorString);
}

void NetworkClient::onSslErrors(const QList<QSslError> &errors)
{
    QStringList errorStrings;
    for (const QSslError &error : errors) {
        errorStrings << error.errorString();
    }
    
    QString errorString = errorStrings.join("; ");
    LOG_WARNING(QString("SSL errors: %1").arg(errorString));
    
    // 在开发环境中忽略SSL错误
    _socket->ignoreSslErrors();
    
    emit sslErrors(errors);
}

void NetworkClient::onConnectionTimeout()
{
    LOG_ERROR("Connection timeout");
    
    setConnectionState(Error);
    emit networkError("Connection timeout");
    
    disconnectFromServer();
}

void NetworkClient::onHeartbeatTimeout()
{
    sendHeartbeat();
}

void NetworkClient::setConnectionState(ConnectionState state)
{
    if (_connectionState != state) {
        _connectionState = state;
        emit connectionStateChanged(state);
    }
}

QString NetworkClient::sendJsonRequest(const QJsonObject &request)
{
    if (!isConnected()) {
        LOG_ERROR("Cannot send request: not connected to server");
        return QString();
    }

    QString requestId = generateRequestId();
    QJsonObject requestWithId = request;
    requestWithId["request_id"] = requestId;
    requestWithId["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonDocument doc(requestWithId);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    // 添加消息长度前缀（4字节）
    QByteArray lengthPrefix;
    QDataStream stream(&lengthPrefix, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(data.size());

    QByteArray message = lengthPrefix + data;

    qint64 bytesWritten = _socket->write(message);
    if (bytesWritten == -1) {
        LOG_ERROR("Failed to send request to server");
        return QString();
    }

    _socket->flush();
    
    // 线程安全地添加待处理请求
    QMutexLocker locker(&_dataMutex);
    // 注意：这里不设置请求类型，因为具体的请求类型会在调用方法中设置
    
    return requestId;
}

void NetworkClient::processReceivedData(const QByteArray &data)
{
    QMutexLocker locker(&_dataMutex);
    
    // 线程安全地添加接收数据
    _receiveBuffer.append(data);
    
    while (_receiveBuffer.size() >= 4) {
        // 读取消息长度
        QDataStream stream(_receiveBuffer);
        stream.setByteOrder(QDataStream::BigEndian);
        quint32 messageLength;
        stream >> messageLength;

        // 检查是否接收到完整消息
        if (_receiveBuffer.size() < 4 + messageLength) {
            break; // 等待更多数据
        }

        // 提取消息内容
        QByteArray messageData = _receiveBuffer.mid(4, messageLength);
        _receiveBuffer.remove(0, 4 + messageLength);

        // 解析JSON响应
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(messageData, &error);

        if (error.error != QJsonParseError::NoError) {
            LOG_ERROR(QString("Failed to parse JSON response: %1").arg(error.errorString()));
            continue;
        }

        if (doc.isObject()) {
            processJsonResponse(doc.object());
        }
    }
}

void NetworkClient::processJsonResponse(const QJsonObject &response)
{
    QString requestId = response["request_id"].toString();
    QString action = response["action"].toString();

    QMutexLocker locker(&_dataMutex);
    
    if (_pendingRequests.contains(requestId)) {
        QString requestType = _pendingRequests.take(requestId);

        if (requestType == "login") {
            emit loginResponse(requestId, response);
        } else if (requestType == "register") {
            emit registerResponse(requestId, response);
        } else if (requestType == "verification_code") {
            emit verificationCodeResponse(requestId, response);
        }
    } else if (action == "heartbeat_response") {
        // 处理心跳响应
        LOG_DEBUG("Received heartbeat response");
    } else {
        LOG_WARNING(QString("Received unknown response: %1").arg(requestId));
    }
}

QString NetworkClient::generateRequestId()
{
    return QString("req_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(++s_requestCounter);
}

void NetworkClient::configureSsl()
{
    QSslConfiguration sslConfig = _socket->sslConfiguration();

    // 设置SSL协议版本
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    // 加载证书（如果存在）
    QString certPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/certs/server.crt";
    if (QFile::exists(certPath)) {
        QList<QSslCertificate> certs = QSslCertificate::fromPath(certPath);
        if (!certs.isEmpty()) {
            sslConfig.setCaCertificates(certs);
        }
    }

    _socket->setSslConfiguration(sslConfig);
}
