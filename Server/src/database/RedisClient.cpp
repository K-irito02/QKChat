#include "RedisClient.h"
#include "../utils/Logger.h"
#include <QMutexLocker>
#include <QEventLoop>
#include <QTimer>


// 静态成员初始化
RedisClient* RedisClient::s_instance = nullptr;
QMutex RedisClient::s_mutex;

RedisClient::RedisClient(QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _port(6379)
    , _database(0)
    , _isConnected(false)
    , _connectTimeout(2000)  // 减少连接超时时间，避免阻塞
    , _commandTimeout(3000)  // 减少命令超时时间
    , _reconnectInterval(10000)
{
    _socket = new QTcpSocket(this);
    _reconnectTimer = new QTimer(this);
    _reconnectTimer->setSingleShot(true);
    
    // 连接信号
    connect(_socket, &QTcpSocket::connected, this, &RedisClient::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &RedisClient::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &RedisClient::onReadyRead);
    connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &RedisClient::onSocketError);
    connect(_reconnectTimer, &QTimer::timeout, this, &RedisClient::onReconnectTimer);
}

RedisClient::~RedisClient()
{
    close();
}

RedisClient* RedisClient::instance()
{
    // 双重检查锁定模式，提高性能
    if (!s_instance) {
        QMutexLocker locker(&s_mutex);
        if (!s_instance) {
            s_instance = new RedisClient();
        }
    }
    return s_instance;
}

bool RedisClient::initialize(const QString &host, int port, const QString &password, int database)
{
    QMutexLocker locker(&_commandMutex);
    
    _host = host;
    _port = port;
    _password = password;
    _database = database;
    
    // 关闭现有连接
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState) {
            _socket->waitForDisconnected(3000);
        }
    }
    
    // 连接到Redis服务器
    _socket->connectToHost(_host, _port);
    
    if (!_socket->waitForConnected(_connectTimeout)) {
        _lastError = QString("Failed to connect to Redis: %1").arg(_socket->errorString());
        logError(_lastError);
        return false;
    }
    
    // 认证（如果需要）
    if (!_password.isEmpty()) {
        Result authResult = sendCommand("AUTH", {_password});
        if (authResult != Success) {
            _lastError = "Redis authentication failed";
            logError(_lastError);
            return false;
        }
    }
    
    // 选择数据库
    if (_database != 0) {
        Result selectResult = sendCommand("SELECT", {QString::number(_database)});
        if (selectResult != Success) {
            _lastError = QString("Failed to select Redis database %1").arg(_database);
            logError(_lastError);
            return false;
        }
    }
    
    _isConnected = true;
    
    LOG_INFO(QString("Connected to Redis: %1:%2 (DB: %3)")
             .arg(_host).arg(_port).arg(_database));
    
    emit connectionStateChanged(true);
    return true;
}

void RedisClient::close()
{
    QMutexLocker locker(&_commandMutex);
    
    _reconnectTimer->stop();
    
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->disconnectFromHost();
        if (_socket->state() != QAbstractSocket::UnconnectedState) {
            _socket->waitForDisconnected(3000);
        }
    }
    
    _isConnected = false;
    emit connectionStateChanged(false);
    
    LOG_INFO("Redis connection closed");
}

bool RedisClient::isConnected() const
{
    QMutexLocker locker(&_commandMutex);
    return _isConnected && _socket->state() == QAbstractSocket::ConnectedState;
}

RedisClient::Result RedisClient::set(const QString &key, const QString &value, int expireSeconds)
{
    if (expireSeconds > 0) {
        return sendCommand("SETEX", {key, QString::number(expireSeconds), value});
    } else {
        return sendCommand("SET", {key, value});
    }
}

RedisClient::Result RedisClient::get(const QString &key, QString &value)
{
    QString response;
    Result result = sendCommand("GET", {key});
    
    if (result == Success) {
        if (readResponse(response) == Success) {
            if (response == "$-1") {
                return NotFound;
            }
            value = response;
            return Success;
        }
    }
    
    return result;
}

RedisClient::Result RedisClient::del(const QString &key)
{
    return sendCommand("DEL", {key});
}

bool RedisClient::exists(const QString &key)
{
    QString response;
    Result result = sendCommand("EXISTS", {key});
    
    if (result == Success) {
        if (readResponse(response) == Success) {
            return response.toInt() > 0;
        }
    }
    
    return false;
}

RedisClient::Result RedisClient::expire(const QString &key, int expireSeconds)
{
    return sendCommand("EXPIRE", {key, QString::number(expireSeconds)});
}

int RedisClient::ttl(const QString &key)
{
    QString response;
    Result result = sendCommand("TTL", {key});
    
    if (result == Success) {
        if (readResponse(response) == Success) {
            return response.toInt();
        }
    }
    
    return -2; // 键不存在
}

qint64 RedisClient::incr(const QString &key, qint64 increment)
{
    QString response;
    Result result;
    
    if (increment == 1) {
        result = sendCommand("INCR", {key});
    } else {
        result = sendCommand("INCRBY", {key, QString::number(increment)});
    }
    
    if (result == Success) {
        if (readResponse(response) == Success) {
            return response.toLongLong();
        }
    }
    
    return -1;
}

qint64 RedisClient::decr(const QString &key, qint64 decrement)
{
    QString response;
    Result result;
    
    if (decrement == 1) {
        result = sendCommand("DECR", {key});
    } else {
        result = sendCommand("DECRBY", {key, QString::number(decrement)});
    }
    
    if (result == Success) {
        if (readResponse(response) == Success) {
            return response.toLongLong();
        }
    }
    
    return -1;
}

bool RedisClient::ping()
{
    Result result = sendCommand("PING");
    return result == Success;
}

QString RedisClient::info()
{
    QString response;
    Result result = sendCommand("INFO");
    
    if (result == Success) {
        if (readResponse(response) == Success) {
            return response;
        }
    }
    
    return "";
}

RedisClient::Result RedisClient::flushdb()
{
    return sendCommand("FLUSHDB");
}

RedisClient::Result RedisClient::setVerificationCode(const QString &email, const QString &code, int expireMinutes)
{
    QString key = QString("verification_code:%1").arg(email);
    return set(key, code, expireMinutes * 60);
}

RedisClient::Result RedisClient::getVerificationCode(const QString &email, QString &code)
{
    QString key = QString("verification_code:%1").arg(email);
    return get(key, code);
}

RedisClient::Result RedisClient::deleteVerificationCode(const QString &email)
{
    QString key = QString("verification_code:%1").arg(email);
    return del(key);
}

RedisClient::Result RedisClient::setSessionToken(qint64 userId, const QString &token, int expireHours)
{
    QString key = QString("session_token:%1").arg(userId);
    return set(key, token, expireHours * 3600);
}

RedisClient::Result RedisClient::getSessionToken(qint64 userId, QString &token)
{
    QString key = QString("session_token:%1").arg(userId);
    return get(key, token);
}

RedisClient::Result RedisClient::deleteSessionToken(qint64 userId)
{
    QString key = QString("session_token:%1").arg(userId);
    return del(key);
}

void RedisClient::onConnected()
{
    LOG_INFO("Redis connected successfully");
    _isConnected = true;
    emit connectionStateChanged(true);
}

void RedisClient::onDisconnected()
{
    LOG_WARNING("Redis disconnected");
    _isConnected = false;
    emit connectionStateChanged(false);

    // 启动重连定时器
    if (!_reconnectTimer->isActive()) {
        _reconnectTimer->start(_reconnectInterval);
    }
}

void RedisClient::onReadyRead()
{
    // 数据读取在sendCommand中处理
}

void RedisClient::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = _socket->errorString();
    _lastError = QString("Redis socket error: %1").arg(errorString);
    logError(_lastError);

    _isConnected = false;
    emit connectionStateChanged(false);
    emit redisError(errorString);
}

void RedisClient::onReconnectTimer()
{
    LOG_INFO("Attempting to reconnect to Redis...");
    reconnect();
}

bool RedisClient::reconnect()
{
    return initialize(_host, _port, _password, _database);
}

RedisClient::Result RedisClient::sendCommand(const QString &command, const QStringList &args)
{
    QMutexLocker locker(&_commandMutex);

    // 直接检查连接状态，避免调用isConnected()导致递归锁定
    if (!_isConnected || _socket->state() != QAbstractSocket::ConnectedState) {
        if (!reconnect()) {
            return ConnectionError;
        }
    }

    QByteArray data = formatCommand(command, args);

    qint64 bytesWritten = _socket->write(data);
    if (bytesWritten == -1) {
        _lastError = "Failed to write command to Redis";
        logError(_lastError);
        return Error;
    }

    // 释放锁后再进行阻塞等待
    locker.unlock();

    if (!_socket->waitForBytesWritten(_commandTimeout)) {
        QMutexLocker relocker(&_commandMutex);
        _lastError = "Timeout writing command to Redis";
        logError(_lastError);
        return Timeout;
    }

    return Success;
}

RedisClient::Result RedisClient::readResponse(QString &response, int timeout)
{
    // 在锁外进行阻塞等待，避免阻塞其他线程
    if (!_socket->waitForReadyRead(timeout)) {
        QMutexLocker locker(&_commandMutex);
        _lastError = "Timeout reading response from Redis";
        logError(_lastError);
        return Timeout;
    }

    QMutexLocker locker(&_commandMutex);

    QByteArray data = _socket->readAll();
    
    // 调试：打印原始响应数据


    if (!parseResponse(data, response)) {
        _lastError = "Failed to parse Redis response";
        logError(_lastError);
        return Error;
    }
    
    // 调试：打印解析后的响应


    return Success;
}

bool RedisClient::parseResponse(const QByteArray &data, QString &result)
{
    if (data.isEmpty()) {
        return false;
    }

    // 调试：打印原始数据的十六进制表示


    // 处理混合响应（如 "OK\r\n$6\r\n607496"）
    // 查找最后一个批量字符串响应
    int lastBulkStringPos = data.lastIndexOf('$');
    if (lastBulkStringPos > 0) {
    
        QByteArray bulkStringData = data.mid(lastBulkStringPos);
        return parseResponse(bulkStringData, result);
    }

    char type = data[0];
    
    switch (type) {
        case '+': // 简单字符串
            {
                QString content = QString::fromUtf8(data.mid(1)).trimmed();
                result = content;
            
                return true;
            }

        case '-': // 错误
            {
                QString content = QString::fromUtf8(data.mid(1)).trimmed();
                _lastError = content;
                logError(_lastError);
                return false;
            }

        case ':': // 整数
            {
                QString content = QString::fromUtf8(data.mid(1)).trimmed();
                result = content;
            
                return true;
            }

        case '$': // 批量字符串
            {
                // 找到第一个\r\n，获取长度
                int firstCRLF = data.indexOf("\r\n");
                if (firstCRLF == -1) {
                
                    return false;
                }
                
                QString lengthStr = QString::fromUtf8(data.mid(1, firstCRLF - 1));
                int length = lengthStr.toInt();
                
            
                
                if (length == -1) {
                    result = "$-1"; // NULL
                
                    return true;
                } else if (length >= 0) {
                    // 实际字符串从第一个\r\n之后开始
                    int contentStart = firstCRLF + 2;
                    if (data.length() >= contentStart + length) {
                        result = QString::fromUtf8(data.mid(contentStart, length));
                    
                        return true;
                    } else {
                    
                    }
                }
                return false;
            }

        case '*': // 数组
            {
                QString content = QString::fromUtf8(data.mid(1)).trimmed();
                result = content;
            
                return true;
            }

        default:
        
            return false;
    }
}

QByteArray RedisClient::formatCommand(const QString &command, const QStringList &args)
{
    QByteArray result;
    QStringList allArgs;
    allArgs << command << args;

    // Redis协议格式：*<参数数量>\r\n$<参数长度>\r\n<参数内容>\r\n...
    result.append(QString("*%1\r\n").arg(allArgs.size()).toUtf8());

    for (const QString &arg : allArgs) {
        QByteArray argBytes = arg.toUtf8();
        result.append(QString("$%1\r\n").arg(argBytes.size()).toUtf8());
        result.append(argBytes);
        result.append("\r\n");
    }

    return result;
}

void RedisClient::logError(const QString &error)
{
    LOG_ERROR(error);
    _lastError = error;
}
