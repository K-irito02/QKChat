#include "DatabaseErrorHandler.h"
#include "Logger.h"
#include <QDateTime>
#include <QThread>

// 静态成员初始化
DatabaseErrorHandler* DatabaseErrorHandler::s_instance = nullptr;
QMutex DatabaseErrorHandler::s_instanceMutex;

DatabaseErrorHandler::DatabaseErrorHandler(QObject *parent)
    : QObject(parent)
    , _circuitBreakerTimer(new QTimer(this))
{
    initializeDefaults();
    
    // 设置熔断器定时器
    _circuitBreakerTimer->setInterval(30000); // 30秒检查一次
    connect(_circuitBreakerTimer, &QTimer::timeout, this, &DatabaseErrorHandler::onCircuitBreakerTimeout);
    _circuitBreakerTimer->start();
}

DatabaseErrorHandler::~DatabaseErrorHandler()
{
    if (_circuitBreakerTimer) {
        _circuitBreakerTimer->stop();
    }
}

DatabaseErrorHandler* DatabaseErrorHandler::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new DatabaseErrorHandler();
        }
    }
    return s_instance;
}

void DatabaseErrorHandler::initializeDefaults()
{
    QMutexLocker locker(&_mutex);
    
    // 设置默认错误阈值
    _errorThresholds[ConnectionError] = 10;
    _errorThresholds[TimeoutError] = 5;
    _errorThresholds[DeadlockError] = 3;
    _errorThresholds[ConstraintError] = 20;
    _errorThresholds[PermissionError] = 5;
    _errorThresholds[SyntaxError] = 10;
    _errorThresholds[ResourceError] = 8;
    _errorThresholds[UnknownError] = 15;
    
    // 初始化错误计数
    for (int i = ConnectionError; i <= UnknownError; ++i) {
        ErrorType type = static_cast<ErrorType>(i);
        _errorCounts[type] = 0;
        _circuitBreakerStates[type] = false;
    }
}

DatabaseErrorHandler::ErrorType DatabaseErrorHandler::classifyError(const QSqlError& error)
{
    if (!error.isValid()) {
        return UnknownError;
    }
    
    QString errorText = error.text().toLower();
    QString driverText = error.driverText().toLower();
    QString databaseText = error.databaseText().toLower();
    
    // 连接错误
    if (errorText.contains("connection") || errorText.contains("network") ||
        driverText.contains("connection") || databaseText.contains("connection")) {
        return ConnectionError;
    }
    
    // 超时错误
    if (errorText.contains("timeout") || errorText.contains("timed out") ||
        driverText.contains("timeout") || databaseText.contains("timeout")) {
        return TimeoutError;
    }
    
    // 死锁错误
    if (errorText.contains("deadlock") || errorText.contains("lock") ||
        driverText.contains("deadlock") || databaseText.contains("deadlock")) {
        return DeadlockError;
    }
    
    // 约束错误
    if (errorText.contains("constraint") || errorText.contains("duplicate") ||
        errorText.contains("unique") || errorText.contains("foreign key")) {
        return ConstraintError;
    }
    
    // 权限错误
    if (errorText.contains("permission") || errorText.contains("access denied") ||
        errorText.contains("unauthorized") || errorText.contains("privilege")) {
        return PermissionError;
    }
    
    // 语法错误
    if (errorText.contains("syntax") || errorText.contains("sql") ||
        driverText.contains("syntax") || databaseText.contains("syntax")) {
        return SyntaxError;
    }
    
    // 资源错误
    if (errorText.contains("resource") || errorText.contains("memory") ||
        errorText.contains("disk") || errorText.contains("space")) {
        return ResourceError;
    }
    
    return UnknownError;
}

DatabaseErrorHandler::RecoveryStrategy DatabaseErrorHandler::getRecoveryStrategy(ErrorType errorType)
{
    switch (errorType) {
        case ConnectionError:
        case TimeoutError:
            return Retry;
        case DeadlockError:
            return Retry;
        case ConstraintError:
            return Ignore;
        case PermissionError:
            return Abort;
        case SyntaxError:
            return Abort;
        case ResourceError:
            return CircuitBreaker;
        case UnknownError:
        default:
            return Fallback;
    }
}

bool DatabaseErrorHandler::handleError(const QSqlError& error, const QString& context)
{
    ErrorType errorType = classifyError(error);
    RecoveryStrategy strategy = getRecoveryStrategy(errorType);
    
    // 记录错误
    logError(error, context);
    
    // 更新错误计数
    updateErrorCount(errorType);
    
    // 检查熔断器
    checkCircuitBreaker(errorType);
    
    // 根据策略处理错误
    switch (strategy) {
        case Retry:
            LOG_INFO(QString("Retry strategy for %1 error in context: %2").arg(errorType).arg(context));
            return true; // 允许重试
            
        case Fallback:
            LOG_WARNING(QString("Fallback strategy for %1 error in context: %2").arg(errorType).arg(context));
            return false; // 使用降级策略
            
        case CircuitBreaker:
            LOG_ERROR(QString("Circuit breaker triggered for %1 error in context: %2").arg(errorType).arg(context));
            emit circuitBreakerTriggered(errorType);
            return false;
            
        case Ignore:
            LOG_DEBUG(QString("Ignoring %1 error in context: %2").arg(errorType).arg(context));
            return true; // 忽略错误
            
        case Abort:
            LOG_ERROR(QString("Aborting due to %1 error in context: %2").arg(errorType).arg(context));
            return false; // 中止操作
            
        default:
            return false;
    }
}

void DatabaseErrorHandler::logError(const QSqlError& error, const QString& context)
{
    ErrorType errorType = classifyError(error);
    
    QString logMessage = QString("Database error [%1] in context '%2': %3")
                        .arg(errorType)
                        .arg(context.isEmpty() ? "unknown" : context)
                        .arg(error.text());
    
    switch (errorType) {
        case ConnectionError:
        case TimeoutError:
        case DeadlockError:
            LOG_WARNING(logMessage);
            break;
        case ConstraintError:
        case PermissionError:
        case SyntaxError:
        case ResourceError:
        case UnknownError:
        default:
            LOG_ERROR(logMessage);
            break;
    }
}

QMap<DatabaseErrorHandler::ErrorType, int> DatabaseErrorHandler::getErrorStatistics() const
{
    QMutexLocker locker(&_mutex);
    return _errorCounts;
}

void DatabaseErrorHandler::resetErrorStatistics()
{
    QMutexLocker locker(&_mutex);
    for (auto it = _errorCounts.begin(); it != _errorCounts.end(); ++it) {
        it.value() = 0;
    }
}

void DatabaseErrorHandler::setErrorThreshold(ErrorType errorType, int threshold)
{
    QMutexLocker locker(&_mutex);
    _errorThresholds[errorType] = threshold;
}

bool DatabaseErrorHandler::shouldCircuitBreak(ErrorType errorType) const
{
    QMutexLocker locker(&_mutex);
    return _circuitBreakerStates.value(errorType, false);
}

void DatabaseErrorHandler::updateErrorCount(ErrorType errorType)
{
    QMutexLocker locker(&_mutex);
    _errorCounts[errorType]++;
    _lastErrorTimes[errorType] = QDateTime::currentDateTime();
    
    // 检查是否超过阈值
    int threshold = _errorThresholds.value(errorType, 10);
    if (_errorCounts[errorType] >= threshold) {
        emit errorThresholdExceeded(errorType, _errorCounts[errorType]);
    }
}

void DatabaseErrorHandler::checkCircuitBreaker(ErrorType errorType)
{
    QMutexLocker locker(&_mutex);
    
    int threshold = _errorThresholds.value(errorType, 10);
    if (_errorCounts[errorType] >= threshold) {
        _circuitBreakerStates[errorType] = true;
        emit circuitBreakerTriggered(errorType);
        
        LOG_ERROR(QString("Circuit breaker activated for error type %1 (count: %2, threshold: %3)")
                 .arg(errorType).arg(_errorCounts[errorType]).arg(threshold));
    }
}

void DatabaseErrorHandler::resetCircuitBreaker(ErrorType errorType)
{
    QMutexLocker locker(&_mutex);
    _circuitBreakerStates[errorType] = false;
    _errorCounts[errorType] = 0;
    
    LOG_INFO(QString("Circuit breaker reset for error type %1").arg(errorType));
}

void DatabaseErrorHandler::onCircuitBreakerTimeout()
{
    QMutexLocker locker(&_mutex);
    
    QDateTime now = QDateTime::currentDateTime();
    for (auto it = _circuitBreakerStates.begin(); it != _circuitBreakerStates.end(); ++it) {
        if (it.value()) { // 如果熔断器处于开启状态
            ErrorType errorType = it.key();
            QDateTime lastErrorTime = _lastErrorTimes.value(errorType);
            
            // 如果距离上次错误超过5分钟，重置熔断器
            if (lastErrorTime.isValid() && lastErrorTime.secsTo(now) > 300) {
                resetCircuitBreaker(errorType);
            }
        }
    }
}
