#include "NetworkClient.h"
#include "../utils/Logger.h"
#include <QUuid>
#include <QJsonDocument>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslCipher>
#include <QFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMutexLocker>

// 静态成员初始化
int NetworkClient::s_requestCounter = 0;
QMutex NetworkClient::s_counterMutex;

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _connectionState(Disconnected)
    , _serverPort(0)
    , _useTLS(true)
    , _connectionTimeout(10000)  // 10秒
    , _heartbeatInterval(30000)  // 30秒
    , _reconnectInterval(1000)   // 1秒
    , _maxReconnectAttempts(10)  // 最大重连10次
    , _currentReconnectAttempts(0)
    , _autoReconnect(true)
    , _connectionTimer(nullptr)
    , _heartbeatTimer(nullptr)
    , _reconnectTimer(nullptr)
{
    LOG_INFO("=== NetworkClient constructor started ===");
    
    // 简化对象创建
    try {
        // 创建SSL套接字
        _socket = new QSslSocket(this);
        LOG_INFO("SSL socket created");
        
        // 创建定时器
        _connectionTimer = new QTimer(this);
        _connectionTimer->setSingleShot(true);
        LOG_INFO("Connection timer created");
        
        _heartbeatTimer = new QTimer(this);
        LOG_INFO("Heartbeat timer created");
        
        _reconnectTimer = new QTimer(this);
        _reconnectTimer->setSingleShot(true);
        LOG_INFO("Reconnect timer created");
        
        // 连接信号
        connect(_socket, &QSslSocket::connected, this, &NetworkClient::onConnected);
        connect(_socket, &QSslSocket::disconnected, this, &NetworkClient::onDisconnected);
        connect(_socket, &QSslSocket::readyRead, this, &NetworkClient::onReadyRead);
        connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
                this, &NetworkClient::onSocketError);
        connect(_socket, &QSslSocket::sslErrors, this, &NetworkClient::onSslErrors);
        LOG_INFO("Socket signals connected");
        
        connect(_connectionTimer, &QTimer::timeout, this, &NetworkClient::onConnectionTimeout);
        connect(_heartbeatTimer, &QTimer::timeout, this, &NetworkClient::onHeartbeatTimeout);
        connect(_reconnectTimer, &QTimer::timeout, this, &NetworkClient::onReconnectTimer);
        LOG_INFO("Timer signals connected");
        
        // 暂时禁用复杂的网络质量监控和错误处理
        _qualityMonitor = nullptr;
        _errorHandler = nullptr;
        LOG_INFO("Quality monitor and error handler disabled");
        
        // 配置SSL
        configureSsl();
        LOG_INFO("SSL configured");
        
        LOG_INFO("=== NetworkClient constructor completed successfully ===");
        
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
    
    // 清理新添加的对象（暂时禁用）
    // if (_qualityMonitor) {
    //     _qualityMonitor->deleteLater();
    //     _qualityMonitor = nullptr;
    // }
    // if (_errorHandler) {
    //     _errorHandler->deleteLater();
    //     _errorHandler = nullptr;
    // }
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
    
    // 临时禁用SSL以测试基本连接
    if (_useTLS) {
        LOG_WARNING("SSL temporarily disabled for testing - using plain TCP connection");
        _socket->connectToHost(host, port);
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
    LOG_DEBUG("=== sendHeartbeat() called ===");
    
    try {
        // 简化连接检查
        if (_connectionState != Connected || !_socket || _socket->state() != QAbstractSocket::ConnectedState) {
            LOG_WARNING("Cannot send heartbeat: not connected to server");
            return;
        }
        
        LOG_DEBUG("Preparing to send heartbeat");
        
        QJsonObject request;
        request["action"] = "heartbeat";
        request["timestamp"] = QDateTime::currentSecsSinceEpoch();
        
        LOG_DEBUG("Created heartbeat request JSON");
        
        // 直接发送心跳，避免复杂的异步嵌套
        QString requestId = sendJsonRequest(request);
        if (!requestId.isEmpty()) {
            LOG_DEBUG(QString("Sent heartbeat request: %1").arg(requestId));
        } else {
            LOG_WARNING("Failed to send heartbeat request");
        }
        
        LOG_DEBUG("=== sendHeartbeat() completed ===");
        
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
    LOG_INFO("=== onConnected() called ===");
    
    try {
        // 基本连接处理
        if (_connectionTimer) {
            _connectionTimer->stop();
        }
        
        LOG_INFO("Connected to server successfully");
        
        // 设置连接状态
        if (_connectionState == Reconnecting) {
            handleReconnectionSuccess();
        } else {
            setConnectionState(Connected);
        }
        
        // 记录连接信息
        if (_useTLS) {
            LOG_INFO("SSL temporarily disabled - using plain TCP connection");
        } else {
            LOG_INFO("Using plain TCP connection");
        }
        
        // 直接启动心跳定时器，简化逻辑
        LOG_INFO("=== Starting heartbeat timer ===");
        
        if (_connectionState == Connected && _heartbeatTimer) {
            LOG_INFO("Starting heartbeat timer");
            _heartbeatTimer->setSingleShot(false);
            _heartbeatTimer->setInterval(_heartbeatInterval);
            _heartbeatTimer->start();
            
            if (_heartbeatTimer->isActive()) {
                LOG_INFO(QString("Heartbeat timer started successfully, interval: %1ms").arg(_heartbeatInterval));
            } else {
                LOG_ERROR("Failed to start heartbeat timer!");
            }
        } else {
            LOG_WARNING("Cannot start heartbeat timer - not connected or timer is null");
        }
        
        LOG_INFO("=== onConnected() completed successfully ===");
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in onConnected(): %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in onConnected()");
    }
}

void NetworkClient::onDisconnected()
{
    _connectionTimer->stop();
    
    LOG_INFO("Disconnected from server");
    LOG_DEBUG(QString("Heartbeat timer state before stop: %1").arg(_heartbeatTimer->isActive() ? "Active" : "Inactive"));
    
    // 只有在非重连状态下才停止心跳定时器
    if (_connectionState != Reconnecting) {
        _heartbeatTimer->stop();
        LOG_DEBUG("Heartbeat timer stopped due to disconnection");
    } else {
        LOG_DEBUG("Heartbeat timer kept active during reconnection");
    }
    
    // 如果不是主动断开，尝试重连
    if (_autoReconnect && _connectionState != Disconnected) {
        LOG_INFO("Connection lost, attempting reconnection");
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
    
    // 记录心跳定时器状态
    LOG_DEBUG(QString("Heartbeat timer state during socket error: %1").arg(_heartbeatTimer->isActive() ? "Active" : "Inactive"));
    
    // 只有在非重连状态下才停止心跳定时器
    if (_connectionState != Reconnecting) {
        _heartbeatTimer->stop();
        LOG_DEBUG("Heartbeat timer stopped due to socket error");
    } else {
        LOG_DEBUG("Heartbeat timer kept active during reconnection");
    }
    
    // 简化错误处理，避免复杂的错误处理逻辑
    LOG_WARNING(QString("Socket error detected: %1").arg(errorString));
    
    setConnectionState(Error);
    emit networkError(errorString);
}

void NetworkClient::onSslErrors(const QList<QSslError> &errors)
{
    QStringList errorStrings;
    QStringList errorDetails;
    
    for (const QSslError &error : errors) {
        QString errorStr = error.errorString();
        errorStrings << errorStr;
        
        // 记录详细的SSL错误信息
        QString detail = QString("SSL Error: %1 (Code: %2)")
                        .arg(errorStr)
                        .arg(static_cast<int>(error.error()));
        errorDetails << detail;
        LOG_WARNING(detail);
    }
    
    QString errorString = errorStrings.join("; ");
    LOG_WARNING(QString("SSL errors detected: %1").arg(errorString));
    
    // 记录SSL配置信息以便调试
    QSslConfiguration config = _socket->sslConfiguration();
    LOG_DEBUG(QString("SSL Protocol: %1").arg(static_cast<int>(config.protocol())));
    LOG_DEBUG(QString("SSL Ciphers: %1").arg(config.ciphers().size()));
    LOG_DEBUG(QString("SSL Peer Verify Mode: %1").arg(static_cast<int>(config.peerVerifyMode())));
    
    // 记录SSL会话信息
    LOG_DEBUG(QString("SSL Session Protocol: %1").arg(static_cast<int>(_socket->sessionProtocol())));
    LOG_DEBUG(QString("SSL Session Cipher: %1").arg(_socket->sessionCipher().name()));
    LOG_DEBUG(QString("SSL Session Cipher Authentication: %1").arg(_socket->sessionCipher().authenticationMethod()));
    LOG_DEBUG(QString("SSL Session Cipher Encryption: %1").arg(_socket->sessionCipher().encryptionMethod()));
    
    // 在开发环境中忽略所有SSL错误
    _socket->ignoreSslErrors();
    
    // 继续连接，不因为SSL错误而断开
    LOG_INFO("Ignoring SSL errors and continuing connection");
    
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
    LOG_DEBUG("=== onHeartbeatTimeout() called ===");
    
    try {
        // 检查连接状态
        if (_connectionState != Connected) {
            LOG_WARNING("Not connected, skipping heartbeat");
            if (_heartbeatTimer && _heartbeatTimer->isActive()) {
                _heartbeatTimer->stop();
                LOG_DEBUG("Stopped heartbeat timer due to disconnected state");
            }
            return;
        }
        
        // 检查心跳定时器是否存在
        if (!_heartbeatTimer) {
            LOG_ERROR("Heartbeat timer is null in onHeartbeatTimeout!");
            return;
        }
        
        LOG_DEBUG("Calling sendHeartbeat()");
        
        // 直接调用sendHeartbeat，避免异步嵌套
        if (_connectionState == Connected && _socket && _socket->state() == QAbstractSocket::ConnectedState) {
            sendHeartbeat();
        }
        
        LOG_DEBUG("=== onHeartbeatTimeout() completed ===");
        
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
        LOG_DEBUG("Already reconnecting, skipping");
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
    
    LOG_INFO(QString("Starting reconnection attempt %1/%2 in %3ms")
             .arg(_currentReconnectAttempts)
             .arg(_maxReconnectAttempts)
             .arg(delay));
    
    // 确保重连定时器是单次触发
    _reconnectTimer->setSingleShot(true);
    _reconnectTimer->start(delay);
    
    LOG_DEBUG(QString("Reconnect timer started, will attempt connection in %1ms").arg(delay));
}

void NetworkClient::onReconnectTimer()
{
    LOG_INFO(QString("Reconnection attempt %1/%2")
             .arg(_currentReconnectAttempts)
             .arg(_maxReconnectAttempts));
    
    // 检查心跳定时器状态
    LOG_DEBUG(QString("Heartbeat timer state during reconnection: %1").arg(_heartbeatTimer->isActive() ? "Active" : "Inactive"));
    
    // 尝试重新连接
    if (_socket->state() == QAbstractSocket::UnconnectedState) {
        LOG_DEBUG(QString("Socket state is Unconnected, attempting connection to %1:%2").arg(_serverHost).arg(_serverPort));
        
        if (_useTLS) {
            _socket->connectToHost(_serverHost, _serverPort);
        } else {
            _socket->connectToHost(_serverHost, _serverPort);
        }
        
        // 启动连接超时
        _connectionTimer->start(_connectionTimeout);
        LOG_DEBUG(QString("Connection timer started with timeout: %1ms").arg(_connectionTimeout));
    } else {
        LOG_WARNING(QString("Socket is not in Unconnected state: %1").arg(_socket->state()));
    }
}

void NetworkClient::handleReconnectionSuccess()
{
    LOG_INFO("Reconnection successful");
    _currentReconnectAttempts = 0;
    // 暂时禁用错误处理器
    // if (_errorHandler) {
    //     _errorHandler->resetAllErrorCounts();
    // }
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
    // if (_qualityMonitor) {
    //     int quality = _qualityMonitor->getNetworkQuality();
    //     int avgRtt = _qualityMonitor->getAverageRtt();
    //     
    //     LOG_INFO(QString("Network quality updated: %1/100, avg RTT: %2ms")
    //              .arg(quality)
    //              .arg(avgRtt));
    //     
    //     // 根据网络质量调整心跳间隔（可选功能）
    //     // 这里可以添加自适应心跳间隔的逻辑
    // }
}

void NetworkClient::setConnectionState(ConnectionState state)
{
    try {
        if (_connectionState != state) {
            _connectionState = state;
            emit connectionStateChanged(state);
            LOG_INFO(QString("Connection state changed to %1").arg(static_cast<int>(state)));
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in setConnectionState(): %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in setConnectionState()");
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
    
    // 注意：不在此处锁定_dataMutex，避免与processReceivedData产生死锁
    // 请求类型的设置由调用方法负责
    
    return requestId;
}

void NetworkClient::processReceivedData(const QByteArray &data)
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
                processJsonResponse(doc.object());
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in processReceivedData(): %1").arg(e.what()));
        _receiveBuffer.clear();
    } catch (...) {
        LOG_ERROR("Unknown exception in processReceivedData()");
        _receiveBuffer.clear();
    }
}

void NetworkClient::processJsonResponse(const QJsonObject &response)
{
    QString requestId = response["request_id"].toString();
    QString action = response["action"].toString();

    // 注意：此方法由processReceivedData调用，_dataMutex已经被锁定
    // 不要在此处再次锁定_dataMutex，避免死锁
    
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
        LOG_DEBUG(QString("Heartbeat response request ID: %1").arg(requestId));
        
        // 验证心跳定时器状态
        if (_heartbeatTimer->isActive()) {
            LOG_DEBUG(QString("Heartbeat timer active after response, next heartbeat in %1ms").arg(_heartbeatTimer->remainingTime()));
        } else {
            LOG_ERROR("Heartbeat timer stopped after receiving heartbeat response!");
        }
        
        // 暂时禁用网络质量监控
        // if (_qualityMonitor) {
        //     _qualityMonitor->recordHeartbeatReceived(requestId);
        // }
        
        // 心跳响应不需要特殊处理，只是确认连接正常
    } else {
        LOG_WARNING(QString("Received unknown response: %1").arg(requestId));
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
            LOG_DEBUG("Connection check - Socket is null");
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
        
        LOG_DEBUG(QString("Connection check - Socket state: %1, Connection state: %2, Connected: %3")
                  .arg(static_cast<int>(socketState))
                  .arg(static_cast<int>(_connectionState))
                  .arg(socketConnected && stateConnected ? "Yes" : "No"));
        
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

void NetworkClient::configureSsl()
{
    try {
        if (!_socket) {
            LOG_ERROR("Socket is null in configureSsl()");
            return;
        }
        
        // 使用默认SSL配置
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setProtocol(QSsl::TlsV1_2);
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        sslConfig.setPeerVerifyDepth(0);
        
        // 忽略SSL错误
        _socket->ignoreSslErrors();
        
        // 设置SSL配置
        _socket->setSslConfiguration(sslConfig);
        
        LOG_INFO("SSL configured successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in configureSsl(): %1").arg(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in configureSsl()");
    }
}
