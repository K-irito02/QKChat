#include "SmartErrorHandler.h"
#include "../utils/Logger.h"
#include <QDateTime>

#include <QRandomGenerator>

SmartErrorHandler::SmartErrorHandler(QObject *parent)
    : QObject(parent)
{
}

SmartErrorHandler::~SmartErrorHandler()
{
}

bool SmartErrorHandler::handleError(const QString& errorType, const QString& errorMessage)
{
    ErrorType classifiedType;
    QString classifiedTypeStr;
    bool shouldRetry;
    int retryDelay;

    // 在锁内快速处理数据
    {
        QMutexLocker locker(&_mutex);

        classifiedType = classifyError(errorMessage);
        classifiedTypeStr = errorTypeToString(classifiedType);

        // 更新错误统计
        _errorCounts[classifiedTypeStr]++;
        _lastErrorTime[classifiedTypeStr] = QDateTime::currentMSecsSinceEpoch();

        LOG_WARNING(QString("Handling error: %1 (classified as: %2, count: %3)")
                    .arg(errorMessage)
                    .arg(classifiedTypeStr)
                    .arg(_errorCounts[classifiedTypeStr]));

        // 检查是否应该重试
        shouldRetry = shouldRetryErrorType(classifiedType) &&
                      _errorCounts[classifiedTypeStr] <= getMaxRetries(classifiedType);

        retryDelay = calculateRetryDelay(classifiedType, _errorCounts[classifiedTypeStr]);

        LOG_INFO(QString("Error handling suggestion: retry=%1, delay=%2ms")
                 .arg(shouldRetry ? "true" : "false")
                 .arg(retryDelay));
    } // 锁在这里释放

    // 在锁外发射信号，避免死锁
    emit errorHandlingSuggestion(classifiedTypeStr, shouldRetry, retryDelay);

    return shouldRetry;
}

bool SmartErrorHandler::shouldRetry(const QString& errorType)
{
    QMutexLocker locker(&_mutex);
    
    if (isInCooldown(errorType)) {
        return false;
    }
    
    int errorCount = _errorCounts.value(errorType, 0);
    ErrorType type = classifyError(errorType); // 使用错误类型字符串作为消息来分类
    
    bool shouldRetry = shouldRetryErrorType(type) && errorCount <= getMaxRetries(type);
    
    return shouldRetry;
}

int SmartErrorHandler::getRetryDelay(const QString& errorType)
{
    QMutexLocker locker(&_mutex);
    
    int errorCount = _errorCounts.value(errorType, 0);
    ErrorType type = classifyError(errorType);
    
    return calculateRetryDelay(type, errorCount);
}

void SmartErrorHandler::resetErrorCount(const QString& errorType)
{
    QMutexLocker locker(&_mutex);
    
    _errorCounts.remove(errorType);
    _lastErrorTime.remove(errorType);
}

void SmartErrorHandler::resetAllErrorCounts()
{
    QMutexLocker locker(&_mutex);
    
    _errorCounts.clear();
    _lastErrorTime.clear();
}

int SmartErrorHandler::getErrorCount(const QString& errorType) const
{
    QMutexLocker locker(&_mutex);
    return _errorCounts.value(errorType, 0);
}

bool SmartErrorHandler::isInCooldown(const QString& errorType) const
{
    QMutexLocker locker(&_mutex);
    
    if (!_lastErrorTime.contains(errorType)) {
        return false;
    }
    
    qint64 lastErrorTime = _lastErrorTime[errorType];
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = currentTime - lastErrorTime;
    
    return elapsed < ERROR_COOLDOWN_MS;
}

ErrorType SmartErrorHandler::classifyError(const QString& errorMessage)
{
    QString message = errorMessage.toLower();
    
    // 网络连接错误
    if (message.contains("connection") || 
        message.contains("network") ||
        message.contains("host") ||
        message.contains("refused") ||
        message.contains("unreachable") ||
        message.contains("timeout") ||
        message.contains("disconnected")) {
        return ErrorType::NetworkError;
    }
    
    // 服务器错误
    if (message.contains("server") ||
        message.contains("internal") ||
        message.contains("500") ||
        message.contains("502") ||
        message.contains("503") ||
        message.contains("504")) {
        return ErrorType::ServerError;
    }
    
    // 认证错误
    if (message.contains("auth") ||
        message.contains("login") ||
        message.contains("password") ||
        message.contains("unauthorized") ||
        message.contains("401") ||
        message.contains("403")) {
        return ErrorType::AuthenticationError;
    }
    
    // 超时错误
    if (message.contains("timeout") ||
        message.contains("timed out") ||
        message.contains("expired")) {
        return ErrorType::TimeoutError;
    }
    
    // 心跳错误
    if (message.contains("heartbeat") ||
        message.contains("ping") ||
        message.contains("keepalive")) {
        return ErrorType::HeartbeatError;
    }
    
    // 默认为网络错误
    return ErrorType::NetworkError;
}

QString SmartErrorHandler::errorTypeToString(ErrorType errorType) const
{
    switch (errorType) {
        case ErrorType::NetworkError:
            return "NetworkError";
        case ErrorType::ServerError:
            return "ServerError";
        case ErrorType::AuthenticationError:
            return "AuthenticationError";
        case ErrorType::TimeoutError:
            return "TimeoutError";
        case ErrorType::HeartbeatError:
            return "HeartbeatError";
        case ErrorType::UnknownError:
        default:
            return "UnknownError";
    }
}

bool SmartErrorHandler::shouldRetryErrorType(ErrorType errorType) const
{
    switch (errorType) {
        case ErrorType::NetworkError:
            return true; // 网络错误可以重试
        case ErrorType::ServerError:
            return true; // 服务器错误可以重试
        case ErrorType::AuthenticationError:
            return false; // 认证错误不重试
        case ErrorType::TimeoutError:
            return true; // 超时错误可以重试
        case ErrorType::HeartbeatError:
            return true; // 心跳错误可以重试
        case ErrorType::UnknownError:
        default:
            return true; // 未知错误默认重试
    }
}

int SmartErrorHandler::calculateRetryDelay(ErrorType errorType, int errorCount) const
{
    if (errorCount <= 0) {
        return 1000; // 1秒
    }
    
    // 指数退避策略
    int baseDelay = 1000; // 1秒基础延迟
    int maxDelay = MAX_RETRY_DELAY;
    
    // 根据错误类型调整基础延迟
    switch (errorType) {
        case ErrorType::NetworkError:
            baseDelay = 1000; // 1秒
            break;
        case ErrorType::ServerError:
            baseDelay = 5000; // 5秒
            break;
        case ErrorType::TimeoutError:
            baseDelay = 2000; // 2秒
            break;
        case ErrorType::HeartbeatError:
            baseDelay = 3000; // 3秒
            break;
        default:
            baseDelay = 1000; // 1秒
            break;
    }
    
    // 计算指数退避延迟
    int delay = baseDelay * (1 << (errorCount - 1)); // 2^(errorCount-1)
    
    // 添加随机抖动 (±20%)
    int jitter = delay * 0.2;
    delay += (QRandomGenerator::global()->bounded(2 * jitter + 1)) - jitter;
    
    // 确保延迟在合理范围内
    delay = qBound(1000, delay, maxDelay);
    
    return delay;
}

int SmartErrorHandler::getMaxRetries(ErrorType errorType) const
{
    switch (errorType) {
        case ErrorType::NetworkError:
            return NETWORK_ERROR_MAX_RETRIES;
        case ErrorType::ServerError:
            return SERVER_ERROR_MAX_RETRIES;
        case ErrorType::AuthenticationError:
            return AUTH_ERROR_MAX_RETRIES;
        case ErrorType::TimeoutError:
            return TIMEOUT_ERROR_MAX_RETRIES;
        case ErrorType::HeartbeatError:
            return HEARTBEAT_ERROR_MAX_RETRIES;
        case ErrorType::UnknownError:
        default:
            return 3; // 默认3次
    }
} 
