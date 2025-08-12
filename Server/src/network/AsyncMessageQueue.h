#ifndef ASYNCMESSAGEQUEUE_H
#define ASYNCMESSAGEQUEUE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QTimer>
#include <QJsonObject>
#include <QAtomicInt>
#include <QDateTime>
#include "MessageWorker.h"

/**
 * @brief 异步消息队列类
 * 
 * 提供高性能的异步消息处理能力，支持消息优先级、批量处理、流量控制等功能。
 * 解决同步消息处理导致的性能瓶颈问题。
 */
/**
 * @brief 消息优先级枚举
 */
enum MessagePriority {
    Critical = 0,    // 关键消息（心跳、认证）
    High = 1,        // 高优先级消息（系统通知）
    Normal = 2,      // 普通消息（聊天消息）
    Low = 3          // 低优先级消息（统计、日志）
};

/**
 * @brief 消息结构
 */
struct Message {
    QString messageId;
    qint64 userId;
    QString clientId;
    QJsonObject content;
    MessagePriority priority;
    QDateTime timestamp;
    int retryCount;
    
    Message() : userId(-1), priority(Normal), retryCount(0) {}
    
    bool operator<(const Message& other) const {
        // 优先级越小越优先
        if (priority != other.priority) {
            return priority > other.priority;
        }
        // 同优先级按时间排序
        return timestamp > other.timestamp;
    }
};

/**
 * @brief 队列配置结构
 */
struct QueueConfig {
    int maxQueueSize = 10000;        // 最大队列长度
    int workerThreads = 4;           // 工作线程数
    int batchSize = 50;              // 批处理大小
    int processingInterval = 10;     // 处理间隔(ms)
    int maxRetryCount = 3;           // 最大重试次数
    int retryDelay = 1000;           // 重试延迟(ms)
    bool enableFlowControl = true;   // 启用流量控制
    int flowControlThreshold = 8000; // 流量控制阈值
};

class AsyncMessageQueue : public QObject
{
    Q_OBJECT

public:

    explicit AsyncMessageQueue(QObject *parent = nullptr);
    ~AsyncMessageQueue();
    
    /**
     * @brief 获取单例实例
     */
    static AsyncMessageQueue* instance();
    
    /**
     * @brief 初始化消息队列
     * @param config 队列配置
     * @return 初始化是否成功
     */
    bool initialize(const QueueConfig& config = QueueConfig());
    
    /**
     * @brief 关闭消息队列
     */
    void shutdown();
    
    /**
     * @brief 发送消息到队列
     * @param userId 用户ID
     * @param clientId 客户端ID
     * @param message 消息内容
     * @param priority 消息优先级
     * @return 消息ID，失败返回空字符串
     */
    QString enqueueMessage(qint64 userId, const QString& clientId, 
                          const QJsonObject& message, MessagePriority priority = Normal);
    
    /**
     * @brief 广播消息到所有用户
     * @param message 消息内容
     * @param priority 消息优先级
     * @return 成功入队的消息数量
     */
    int broadcastMessage(const QJsonObject& message, MessagePriority priority = Normal);
    
    /**
     * @brief 发送消息到指定用户组
     * @param userIds 用户ID列表
     * @param message 消息内容
     * @param priority 消息优先级
     * @return 成功入队的消息数量
     */
    int sendToUsers(const QList<qint64>& userIds, const QJsonObject& message, 
                   MessagePriority priority = Normal);
    
    /**
     * @brief 获取队列统计信息
     * @return 统计信息JSON对象
     */
    QJsonObject getStatistics() const;
    
    /**
     * @brief 获取队列当前大小
     * @return 队列大小
     */
    int queueSize() const;
    
    /**
     * @brief 检查队列是否健康
     * @return 队列是否健康
     */
    bool isHealthy() const;
    
    /**
     * @brief 清空队列
     */
    void clearQueue();

signals:
    /**
     * @brief 消息处理完成信号
     * @param messageId 消息ID
     * @param success 是否成功
     */
    void messageProcessed(const QString& messageId, bool success);
    
    /**
     * @brief 队列满警告信号
     * @param currentSize 当前队列大小
     */
    void queueFullWarning(int currentSize);
    
    /**
     * @brief 队列错误信号
     * @param error 错误信息
     */
    void queueError(const QString& error);

private slots:
    void processMessages();
    void handleRetryMessages();
    void performHealthCheck();

private:
    /**
     * @brief 生成消息ID
     */
    QString generateMessageId();
    
    /**
     * @brief 实际发送消息
     * @param message 消息
     * @return 发送是否成功
     */
    bool sendMessage(const Message& message);
    
    /**
     * @brief 获取下一批消息
     * @param batchSize 批次大小
     * @return 消息列表
     */
    QList<Message> getNextBatch(int batchSize);
    
    /**
     * @brief 添加重试消息
     * @param message 消息
     */
    void addRetryMessage(const Message& message);

private:
    static AsyncMessageQueue* s_instance;
    static QMutex s_instanceMutex;
    
    QueueConfig _config;
    
    // 消息队列（按优先级排序）
    QQueue<Message> _messageQueue;
    QQueue<Message> _retryQueue;
    
    // 线程同步
    mutable QMutex _queueMutex;
    QWaitCondition _messageAvailable;
    
    // 工作线程
    QList<QThread*> _workerThreads;
    QList<MessageWorker*> _workers;
    
    // 定时器
    QTimer* _retryTimer;
    QTimer* _healthCheckTimer;
    
    // 统计信息
    QAtomicInt _totalEnqueued;
    QAtomicInt _totalProcessed;
    QAtomicInt _totalFailed;
    QAtomicInt _totalRetried;
    QAtomicInt _currentQueueSize;
    
    // 流量控制
    QAtomicInt _messagesPerSecond;
    QDateTime _lastResetTime;
    
    bool _initialized;
    bool _shuttingDown;
    
    // 消息ID计数器
    QAtomicInt _messageIdCounter;
};

#endif // ASYNCMESSAGEQUEUE_H