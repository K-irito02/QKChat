#ifndef DATABASEERRORHANDLER_H
#define DATABASEERRORHANDLER_H

#include <QObject>
#include <QString>
#include <QSqlError>
#include <QMutex>
#include <QMap>
#include <QTimer>

/**
 * @brief 数据库错误处理器
 * 
 * 提供统一的数据库错误分类、处理和恢复机制
 */
class DatabaseErrorHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 错误类型枚举
     */
    enum ErrorType {
        ConnectionError,      // 连接错误
        TimeoutError,         // 超时错误
        DeadlockError,        // 死锁错误
        ConstraintError,      // 约束错误
        PermissionError,      // 权限错误
        SyntaxError,          // 语法错误
        ResourceError,        // 资源错误
        UnknownError          // 未知错误
    };

    /**
     * @brief 错误处理策略
     */
    enum RecoveryStrategy {
        Retry,               // 重试
        Fallback,            // 降级
        CircuitBreaker,      // 熔断
        Ignore,              // 忽略
        Abort                // 中止
    };

    /**
     * @brief 获取单例实例
     */
    static DatabaseErrorHandler* instance();

    /**
     * @brief 分类错误
     */
    ErrorType classifyError(const QSqlError& error);

    /**
     * @brief 获取错误处理策略
     */
    RecoveryStrategy getRecoveryStrategy(ErrorType errorType);

    /**
     * @brief 处理错误
     */
    bool handleError(const QSqlError& error, const QString& context = "");

    /**
     * @brief 记录错误
     */
    void logError(const QSqlError& error, const QString& context = "");

    /**
     * @brief 获取错误统计
     */
    QMap<ErrorType, int> getErrorStatistics() const;

    /**
     * @brief 重置错误统计
     */
    void resetErrorStatistics();

    /**
     * @brief 设置错误阈值
     */
    void setErrorThreshold(ErrorType errorType, int threshold);

    /**
     * @brief 检查是否需要熔断
     */
    bool shouldCircuitBreak(ErrorType errorType) const;

signals:
    void errorThresholdExceeded(ErrorType errorType, int count);
    void circuitBreakerTriggered(ErrorType errorType);
    void errorRecoveryAttempted(ErrorType errorType, bool success);

private slots:
    void onCircuitBreakerTimeout();

private:
    explicit DatabaseErrorHandler(QObject *parent = nullptr);
    ~DatabaseErrorHandler();

    static DatabaseErrorHandler* s_instance;
    static QMutex s_instanceMutex;

    mutable QMutex _mutex;
    QMap<ErrorType, int> _errorCounts;
    QMap<ErrorType, int> _errorThresholds;
    QMap<ErrorType, QDateTime> _lastErrorTimes;
    QMap<ErrorType, bool> _circuitBreakerStates;
    QTimer* _circuitBreakerTimer;

    /**
     * @brief 初始化默认配置
     */
    void initializeDefaults();

    /**
     * @brief 更新错误计数
     */
    void updateErrorCount(ErrorType errorType);

    /**
     * @brief 检查熔断器状态
     */
    void checkCircuitBreaker(ErrorType errorType);

    /**
     * @brief 重置熔断器
     */
    void resetCircuitBreaker(ErrorType errorType);
};

#endif // DATABASEERRORHANDLER_H
