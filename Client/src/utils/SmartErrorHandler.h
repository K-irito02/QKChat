#ifndef SMARTERRORHANDLER_H
#define SMARTERRORHANDLER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QString>

/**
 * @brief 错误类型枚举
 */
enum class ErrorType {
    NetworkError,        // 网络连接错误
    ServerError,         // 服务器错误
    AuthenticationError, // 认证错误
    TimeoutError,        // 超时错误
    HeartbeatError,      // 心跳错误
    UnknownError         // 未知错误
};

/**
 * @brief 智能错误处理器
 * 
 * 负责智能分类和处理各种错误，包括：
 * - 错误分类和统计
 * - 重试策略管理
 * - 错误冷却机制
 */
class SmartErrorHandler : public QObject
{
    Q_OBJECT

public:
    explicit SmartErrorHandler(QObject *parent = nullptr);
    ~SmartErrorHandler();

    /**
     * @brief 处理错误
     * @param errorType 错误类型
     * @param errorMessage 错误消息
     * @return 是否应该重试
     */
    bool handleError(const QString& errorType, const QString& errorMessage);

    /**
     * @brief 检查是否应该重试
     * @param errorType 错误类型
     * @return 是否应该重试
     */
    bool shouldRetry(const QString& errorType);

    /**
     * @brief 获取重试延迟时间（毫秒）
     * @param errorType 错误类型
     * @return 重试延迟时间
     */
    int getRetryDelay(const QString& errorType);

    /**
     * @brief 重置错误计数
     * @param errorType 错误类型
     */
    void resetErrorCount(const QString& errorType);

    /**
     * @brief 重置所有错误计数
     */
    void resetAllErrorCounts();

    /**
     * @brief 获取错误统计信息
     * @param errorType 错误类型
     * @return 错误计数
     */
    int getErrorCount(const QString& errorType) const;

    /**
     * @brief 检查错误是否在冷却期
     * @param errorType 错误类型
     * @return 是否在冷却期
     */
    bool isInCooldown(const QString& errorType) const;

signals:
    /**
     * @brief 错误处理建议信号
     * @param errorType 错误类型
     * @param shouldRetry 是否应该重试
     * @param retryDelay 重试延迟
     */
    void errorHandlingSuggestion(const QString& errorType, bool shouldRetry, int retryDelay);

private:
    /**
     * @brief 将错误消息分类为错误类型
     * @param errorMessage 错误消息
     * @return 错误类型
     */
    ErrorType classifyError(const QString& errorMessage);

    /**
     * @brief 获取错误类型的字符串表示
     * @param errorType 错误类型
     * @return 错误类型字符串
     */
    QString errorTypeToString(ErrorType errorType) const;

    /**
     * @brief 检查是否应该重试特定错误类型
     * @param errorType 错误类型
     * @return 是否应该重试
     */
    bool shouldRetryErrorType(ErrorType errorType) const;

    /**
     * @brief 计算重试延迟时间
     * @param errorType 错误类型
     * @param errorCount 错误计数
     * @return 重试延迟时间
     */
    int calculateRetryDelay(ErrorType errorType, int errorCount) const;
    
    /**
     * @brief 获取最大重试次数
     * @param errorType 错误类型
     * @return 最大重试次数
     */
    int getMaxRetries(ErrorType errorType) const;

private:
    mutable QMutex _mutex; // 线程安全保护
    
    QMap<QString, int> _errorCounts; // 错误类型 -> 计数
    QMap<QString, qint64> _lastErrorTime; // 错误类型 -> 最后错误时间
    
    static const int MAX_RETRY_COUNT = 5; // 最大重试次数
    static const int ERROR_COOLDOWN_MS = 30000; // 错误冷却时间 (30秒)
    static const int MAX_RETRY_DELAY = 30000; // 最大重试延迟 (30秒)
    
    // 重试策略配置
    static const int NETWORK_ERROR_MAX_RETRIES = 10;
    static const int SERVER_ERROR_MAX_RETRIES = 3;
    static const int AUTH_ERROR_MAX_RETRIES = 0; // 认证错误不重试
    static const int TIMEOUT_ERROR_MAX_RETRIES = 5;
    static const int HEARTBEAT_ERROR_MAX_RETRIES = 3;
};

#endif // SMARTERRORHANDLER_H 