#include "NetworkClient.h"
#include "../utils/Logger.h"
#include <QUuid>
#include <QJsonDocument>
// #include <QSslCertificate>
// #include <QSslKey>
// #include <QSslCipher>
#include <QFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMutexLocker>

// 静态成员初始化
int NetworkClient::s_requestCounter = 0;
QMutex NetworkClient::s_counterMutex;
NetworkClient* NetworkClient::s_instance = nullptr;
QMutex NetworkClient::s_instanceMutex;

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _connectionState(Disconnected)
    , _serverPort(0)
    , _useTLS(false)  // 暂时禁用SSL
    , _connectionTimeout(10000)  // 10秒
    , _heartbeatInterval(30000)  // 30秒
    , _reconnectInterval(1000)   // 1秒
    , _maxReconnectAttempts(10)  // 最大重连10次
    , _currentReconnectAttempts(0)
    , _autoReconnect(true)
    , _connectionTimer(nullptr)
    , _heartbeatTimer(nullptr)
    , _reconnectTimer(nullptr)
    , _isAuthenticated(false)
    , _userId(-1)  // 初始化为-1，表示未认证
{
    // NetworkClient constructor
    
    // 简化对象创建
    try {
        // 创建普通TCP套接字而不是SSL套接字
        _socket = new QTcpSocket(this);

        
        // 创建定时器
        _connectionTimer = new QTimer(this);
        _connectionTimer->setSingleShot(true);

        
        _heartbeatTimer = new QTimer(this);

        
        _reconnectTimer = new QTimer(this);
        _reconnectTimer->setSingleShot(true);

        
        // 连接信号（使用队列连接确保线程安全）
        connect(_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected, Qt::QueuedConnection);
        connect(_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected, Qt::QueuedConnection);
        connect(_socket, &QTcpSocket::readyRead, this, &NetworkClient::onReadyRead, Qt::QueuedConnection);
        connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &NetworkClient::onSocketError, Qt::QueuedConnection);
        // connect(_socket, &QSslSocket::sslErrors, this, &NetworkClient::onSslErrors, Qt::QueuedConnection);
        // Socket signals connected
        
        connect(_connectionTimer, &QTimer::timeout, this, &NetworkClient::onConnectionTimeout, Qt::QueuedConnection);
        connect(_heartbeatTimer, &QTimer::timeout, this, &NetworkClient::onHeartbeatTimeout, Qt::QueuedConnection);
        connect(_reconnectTimer, &QTimer::timeout, this, &NetworkClient::onReconnectTimer, Qt::QueuedConnection);
        // Timer signals connected
        
        // 暂时禁用复杂的网络质量监控和错误处理
        _qualityMonitor = nullptr;
        _errorHandler = nullptr;

        

        

        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in NetworkClient constructor: %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in NetworkClient constructor");
    }
}

NetworkClient::~NetworkClient()
{
    // 停止所有定时器
    if (_connectionTimer) {
        _connectionTimer->stop();
    }
    if (_heartbeatTimer) {
        _heartbeatTimer->stop();
    }
    if (_reconnectTimer) {
        _reconnectTimer->stop();
    }
    
    // 断开连接
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
    _useTLS = false;  // 强制禁用SSL
    
    setConnectionState(Connecting);
    
    
    
    // 启动连接超时定时器
    _connectionTimer->start(_connectionTimeout);

    _socket->connectToHost(host, port);
    
    return true;
}

void NetworkClient::disconnectFromServer()
{
    if (_connectionState == Disconnected) {
        return;
    }
    
    // Disconnecting from server
    
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

QString NetworkClient::sendLoginRequest(const QString &username, const QString &password, bool rememberMe)
{
    QJsonObject request;
    request["action"] = "login";
    request["username"] = username;
    request["password"] = password;
    request["remember_me"] = rememberMe;
    request["client_version"] = "1.0.0";
    request["platform"] = "Windows";
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "login";
        // Sent login request for user
    }
    
    return requestId;
}

QString NetworkClient::sendRegisterRequest(const QString &username, const QString &email, 
                                          const QString &password, const QString &verificationCode)
{
    QJsonObject request;
    request["action"] = "register";
    request["username"] = username;
    request["email"] = email;
    request["password"] = password;
    request["verification_code"] = verificationCode;
    request["client_version"] = "1.0.0";
    request["platform"] = "Windows";
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "register";
        // Sent register request for user
    }
    
    return requestId;
}

QString NetworkClient::sendVerificationCodeRequest(const QString &email)
{
    if (!isConnected()) {
        return QString();
    }
    
    QJsonObject request;
    request["action"] = "send_verification_code";
    request["email"] = email;
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "verification_code";
        // Sent verification code request
    }
    
    return requestId;
}

QString NetworkClient::sendCheckUsernameRequest(const QString &username)
{
    if (!isConnected()) {
        return QString();
    }
    
    QJsonObject request;
    request["action"] = "check_username";
    request["username"] = username;
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "check_username";
        // Sent check username request
    }
    
    return requestId;
}

QString NetworkClient::sendCheckEmailRequest(const QString &email)
{
    if (!isConnected()) {
        return QString();
    }
    
    QJsonObject request;
    request["action"] = "check_email";
    request["email"] = email;
    
    QString requestId = sendJsonRequest(request);
    if (!requestId.isEmpty()) {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "check_email";
        // Sent check email request
    }
    
    return requestId;
}

void NetworkClient::sendHeartbeat()
{
    try {
        // 简化连接检查
        if (_connectionState != Connected || !_socket || _socket->state() != QAbstractSocket::ConnectedState) {
            LOG_WARNING("Cannot send heartbeat: not connected to server");
            return;
        }
        
        QJsonObject request;
        request["action"] = "heartbeat";
        request["timestamp"] = QDateTime::currentSecsSinceEpoch();
        
        // 直接发送心跳，避免复杂的异步嵌套
        QString requestId = sendJsonRequest(request);
        if (requestId.isEmpty()) {
            LOG_WARNING("Failed to send heartbeat request");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in sendHeartbeat(): %1").arg(e.what()));
        // 停止心跳定时器以防止进一步的异常
        if (_heartbeatTimer && _heartbeatTimer->isActive()) {
            _heartbeatTimer->stop();
            LOG_ERROR("Stopped heartbeat timer due to exception");
        }
    } catch (...) {
        LOG_ERROR("Unknown exception in sendHeartbeat()");
        // 停止心跳定时器以防止进一步的异常
        if (_heartbeatTimer && _heartbeatTimer->isActive()) {
            _heartbeatTimer->stop();
            LOG_ERROR("Stopped heartbeat timer due to unknown exception");
        }
    }
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

    
    try {
        // 基本连接处理
        if (_connectionTimer) {
            _connectionTimer->stop();
        }
        

        
        // 设置连接状态
        if (_connectionState == Reconnecting) {
            handleReconnectionSuccess();
        } else {
            setConnectionState(Connected);
        }
        
        // 记录连接信息


        // 启动心跳定时器

        
        if (_connectionState == Connected && _heartbeatTimer) {

            _heartbeatTimer->setSingleShot(false);
            _heartbeatTimer->setInterval(_heartbeatInterval);
            _heartbeatTimer->start();
            
            if (_heartbeatTimer->isActive()) {
        
            } else {
                LOG_ERROR("Failed to start heartbeat timer!");
            }
        } else {
            LOG_WARNING("Cannot start heartbeat timer - not connected or timer is null");
        }
        

        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in onConnected(): %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in onConnected()");
    }
}

void NetworkClient::onDisconnected()
{
    _connectionTimer->stop();
    
    // LOG_INFO removed
    
    // 只有在非重连状态下才停止心跳定时器
    if (_connectionState != Reconnecting) {
        _heartbeatTimer->stop();
    }
    
    // 如果不是主动断开，尝试重连
    if (_autoReconnect && _connectionState != Disconnected) {
        // LOG_INFO removed
        startReconnection();
    }
    
    setConnectionState(Disconnected);
    
    // 清空接收缓冲区和待处理请求
    _receiveBuffer.clear();
    _pendingRequests.clear();
}

void NetworkClient::onReadyRead()
{
    try {
        QByteArray data = _socket->readAll();
        
        // 直接处理新接收的数据，不重复追加到缓冲区
        processReceivedData(data);
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in onReadyRead(): %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in onReadyRead()");
    }
}

void NetworkClient::onSocketError(QAbstractSocket::SocketError error)
{
    _connectionTimer->stop();
    
    QString errorString = _socket->errorString();
    LOG_ERROR(QString("Socket error: %1 (%2)").arg(errorString).arg(error));
    
    // 只有在非重连状态下才停止心跳定时器
    if (_connectionState != Reconnecting) {
        _heartbeatTimer->stop();
    }
    
    // 简化错误处理，避免复杂的错误处理逻辑
    LOG_WARNING(QString("Socket error detected: %1").arg(errorString));
    
    setConnectionState(Error);
    emit networkError(errorString);
}

// void NetworkClient::onSslErrors(const QList<QSslError> &errors)
// {
//     // 暂时禁用SSL错误处理，因为现在使用普通TCP连接
//     Q_UNUSED(errors)
//     // LOG_INFO removed
//     
//     // QStringList errorStrings;
//     // QStringList errorDetails;
//     
//     // for (const QSslError &error : errors) {
//     //     QString errorStr = error.errorString();
//     //     errorStrings << errorStr;
//     //     
//     //     // 记录详细的SSL错误信息
//     //     QString detail = QString("SSL Error: %1 (Code: %2)")
//     //                     .arg(errorStr)
//     //                     .arg(static_cast<int>(error.error()));
//     //     errorDetails << detail;
//     //     LOG_WARNING(detail);
//     // }
//     
//     // QString errorString = errorStrings.join("; ");
//     // LOG_WARNING(QString("SSL errors detected: %1").arg(errorString));
//     
//     // 在开发环境中忽略所有SSL错误
//     // _socket->ignoreSslErrors();
//     
//     // 继续连接，不因为SSL错误而断开
//     // // LOG_INFO removed
//     
//     // emit sslErrors(errors);
// }

void NetworkClient::onConnectionTimeout()
{
    LOG_ERROR("Connection timeout");
    
    setConnectionState(Error);
    emit networkError("Connection timeout");
    
    disconnectFromServer();
}

void NetworkClient::onHeartbeatTimeout()
{
    try {
        // 检查连接状态
        if (_connectionState != Connected) {
            LOG_WARNING("Not connected, skipping heartbeat");
            if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                _heartbeatTimer->stop();
            }
            return;
        }
        
        // 检查心跳定时器是否存在
        if (!_heartbeatTimer) {
            LOG_ERROR("Heartbeat timer is null in onHeartbeatTimeout!");
            return;
        }
        
        // 直接调用sendHeartbeat，避免异步嵌套
        if (_connectionState == Connected && _socket && _socket->state() == QAbstractSocket::ConnectedState) {
            sendHeartbeat();
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in onHeartbeatTimeout(): %1").arg(e.what()));
        // 停止心跳定时器以防止进一步的异常
        if (_heartbeatTimer && _heartbeatTimer->isActive()) {
            _heartbeatTimer->stop();
            LOG_ERROR("Stopped heartbeat timer due to exception in onHeartbeatTimeout");
        }
    } catch (...) {
        LOG_ERROR("Unknown exception in onHeartbeatTimeout()");
        // 停止心跳定时器以防止进一步的异常
        if (_heartbeatTimer && _heartbeatTimer->isActive()) {
            _heartbeatTimer->stop();
            LOG_ERROR("Stopped heartbeat timer due to unknown exception in onHeartbeatTimeout");
        }
    }
}

void NetworkClient::startReconnection()
{
    if (_connectionState == Reconnecting) {
        return;
    }
    
    // 限制重连次数，避免无限循环
    if (_currentReconnectAttempts >= _maxReconnectAttempts) {
        LOG_WARNING(QString("Max reconnection attempts reached (%1), stopping reconnection")
                   .arg(_maxReconnectAttempts));
        handleReconnectionFailure();
        return;
    }
    
    _currentReconnectAttempts++;
    setConnectionState(Reconnecting);
    
    // 简化重连延迟，避免过长的延迟
    int delay = qMin(_reconnectInterval * _currentReconnectAttempts, 10000); // 最大10秒


    
    // 确保重连定时器是单次触发
    _reconnectTimer->setSingleShot(true);
    _reconnectTimer->start(delay);
}

void NetworkClient::onReconnectTimer()
{
    // 尝试重新连接
    if (_socket->state() == QAbstractSocket::UnconnectedState) {
        // 暂时禁用SSL，只使用普通TCP连接
        _socket->connectToHost(_serverHost, _serverPort);
        
        // 启动连接超时
        _connectionTimer->start(_connectionTimeout);
    } else {
        LOG_WARNING(QString("Socket is not in Unconnected state: %1").arg(_socket->state()));
    }
}

void NetworkClient::handleReconnectionSuccess()
{
    // LOG_INFO removed
    _currentReconnectAttempts = 0;
    setConnectionState(Connected);
}

void NetworkClient::handleReconnectionFailure()
{
    LOG_ERROR(QString("Reconnection failed after %1 attempts").arg(_currentReconnectAttempts));
    _currentReconnectAttempts = 0;
    setConnectionState(Error);
    emit networkError("Reconnection failed");
}

void NetworkClient::updateNetworkQuality()
{
    // 暂时禁用网络质量监控

}

void NetworkClient::setConnectionState(ConnectionState state)
{
    try {
        if (_connectionState != state) {
            _connectionState = state;
            emit connectionStateChanged(state);
        
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in setConnectionState(): %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in setConnectionState()");
    }
}

QString NetworkClient::sendChatRequest(const QJsonObject &request)
{
    if (!isConnected()) {
        LOG_ERROR("Cannot send chat request: not connected to server");
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
        LOG_ERROR("Failed to send chat request to server");
        return QString();
    }

    _socket->flush();
    
    // 将聊天请求添加到待处理列表
    {
        QMutexLocker locker(&_dataMutex);
        _pendingRequests[requestId] = "chat";
    }
    
    return requestId;
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
    
    // 注意：不在此处锁定_dataMutex，避免与processReceivedData产生死锁
    // 请求类型的设置由调用方法负责
    
    return requestId;
}

void NetworkClient::processReceivedData(const QByteArray &data)
{
    QList<QJsonObject> responsesToProcess;

    // 在锁内快速处理数据，避免长时间持有锁
    {
        QMutexLocker locker(&_dataMutex);

        try {
            // 线程安全地添加接收数据
            _receiveBuffer.append(data);

            // 限制接收缓冲区大小，防止内存耗尽
            const int MAX_BUFFER_SIZE = 1024 * 1024; // 1MB
            if (_receiveBuffer.size() > MAX_BUFFER_SIZE) {
                LOG_ERROR("Receive buffer size exceeded limit, clearing buffer");
                _receiveBuffer.clear();
                return;
            }

            while (_receiveBuffer.size() >= 4) {
                // 读取消息长度
                QDataStream stream(_receiveBuffer);
                stream.setByteOrder(QDataStream::BigEndian);
                quint32 messageLength;
                stream >> messageLength;

                // 检查消息长度是否合理
                const quint32 MAX_MESSAGE_SIZE = 64 * 1024; // 64KB
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
                    responsesToProcess.append(doc.object());
                }
            }

        } catch (const std::exception& e) {
            LOG_ERROR(QString("Exception in processReceivedData(): %1").arg(e.what()));
            _receiveBuffer.clear();
        } catch (...) {
            LOG_ERROR("Unknown exception in processReceivedData()");
            _receiveBuffer.clear();
        }
    } // 锁在这里释放

    // 在锁外处理响应，避免死锁
    for (const QJsonObject& response : responsesToProcess) {
        processJsonResponse(response);
    }
}

void NetworkClient::processJsonResponse(const QJsonObject &response)
{
    QString requestId = response["request_id"].toString();
    QString action = response["action"].toString();

    // 注意：此方法现在在锁外调用，需要在访问共享数据时加锁
    QString requestType;
    bool hasRequest = false;

    {
        QMutexLocker locker(&_dataMutex);
        if (_pendingRequests.contains(requestId)) {
            requestType = _pendingRequests.take(requestId);
            hasRequest = true;
        } else {
            LOG_WARNING(QString("No matching request found for ID: %1").arg(requestId));
        }
    }

    if (hasRequest) {
        if (requestType == "login") {
            emit loginResponse(requestId, response);
        } else if (requestType == "register") {
            emit registerResponse(requestId, response);
        } else if (requestType == "verification_code") {
            emit verificationCodeResponse(requestId, response);
        } else if (requestType == "check_username") {
            emit usernameAvailabilityResponse(requestId, response);
        } else if (requestType == "check_email") {
            emit emailAvailabilityResponse(requestId, response);
        } else if (requestType == "chat") {
            // 聊天请求的响应直接转发给ChatNetworkClient
            emit messageReceived(response);
        }
    } else if (action == "heartbeat_response") {
        // 处理心跳响应
        // 验证心跳定时器状态
        if (!_heartbeatTimer->isActive()) {
            LOG_ERROR("Heartbeat timer stopped after receiving heartbeat response!");
        }
        
        // 心跳响应不需要特殊处理，只是确认连接正常
        // LOG_DEBUG removed
        return; // 直接返回，避免重复处理
    } else if (action == "error") {
        // 处理错误响应
        QString errorCode = response["error"].toString();
        QString errorMessage = response["error_message"].toString();
        LOG_WARNING(QString("Received error response: %1 - %2").arg(errorCode).arg(errorMessage));
        
        // 如果是认证错误，可能需要重新登录
        if (errorCode == "AUTH_FAILED" || errorCode == "SESSION_EXPIRED") {
            LOG_WARNING("Authentication failed, clearing session");
            setAuthenticated(false);
            emit authenticationFailed(errorMessage);
        } else if (errorCode == "RATE_LIMIT_EXCEEDED") {
            // 处理限流错误
            LOG_WARNING(QString("Rate limit exceeded: %1").arg(errorMessage));
            emit rateLimitExceeded(errorMessage);
        } else if (errorMessage.contains("数据库错误") || errorMessage.contains("database error") || 
                   errorMessage.contains("Driver not loaded")) {
            // 处理数据库错误
            LOG_ERROR(QString("Database error detected: %1").arg(errorMessage));
            emit databaseError(errorMessage);
        } else {
            // 处理其他错误
            emit requestFailed(errorCode, errorMessage);
        }
    } else {
        // 发送消息接收信号，让ChatNetworkClient处理
        emit messageReceived(response);
    }
}

bool NetworkClient::isConnected() const
{
    try {
        // 检查事件循环
        if (!QCoreApplication::instance()) {
            LOG_ERROR("QCoreApplication instance is null in isConnected!");
            return false;
        }
        
        if (!_socket) {
            LOG_ERROR("Socket is null in isConnected!");
            return false;
        }
        
        // 安全地获取socket状态
        QAbstractSocket::SocketState socketState;
        try {
            socketState = _socket->state();
        } catch (const std::exception& e) {
            LOG_ERROR(QString("Exception getting socket state: %1").arg(e.what()));
            return false;
        } catch (...) {
            LOG_ERROR("Unknown exception getting socket state");
            return false;
        }
        
        bool socketConnected = socketState == QAbstractSocket::ConnectedState;
        bool stateConnected = _connectionState == Connected;
        
        // Connection status check
        
        return socketConnected && stateConnected;
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in isConnected(): %1").arg(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception in isConnected()");
        return false;
    }
}

QString NetworkClient::generateRequestId()
{
    // 使用线程安全的方式生成请求ID
    QMutexLocker locker(&s_counterMutex);
    
    return QString("req_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(++s_requestCounter);
}

NetworkClient* NetworkClient::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new NetworkClient();
        }
    }
    return s_instance;
}

void NetworkClient::sendMessage(const QJsonObject& message)
{
    if (!isConnected()) {
        LOG_ERROR("Cannot send message: not connected to server");
        return;
    }
    
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    if (_socket && _socket->state() == QAbstractSocket::ConnectedState) {
        _socket->write(data);
        _socket->flush();
    
    } else {
        LOG_ERROR("Socket not available or not connected");
    }
}

QString NetworkClient::clientId() const
{
    return _clientId;
}

qint64 NetworkClient::userId() const
{
    return _userId;
}

bool NetworkClient::isAuthenticated() const
{
    return _isAuthenticated;
}

QString NetworkClient::sessionToken() const
{
    return _sessionToken;
}

void NetworkClient::setAuthenticated(bool authenticated, const QString& token, qint64 userId)
{

    
    _isAuthenticated = authenticated;
    if (authenticated && !token.isEmpty()) {
        _sessionToken = token;
        _userId = userId;
    
    } else if (!authenticated) {
        _sessionToken.clear();
        _userId = -1;
        // LOG_INFO removed
    }
    

}

void NetworkClient::setClientId(const QString& clientId)
{

    _clientId = clientId;
}


