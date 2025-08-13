#include "AsyncMessageQueue.h"
#include "MessageWorker.h"
#include "../utils/Logger.h"
#include "TcpServer.h"
#include <QUuid>
#include <QJsonDocument>
#include <QCoreApplication>

// 静态成员初始化
AsyncMessageQueue* AsyncMessageQueue::s_instance = nullptr;
QMutex AsyncMessageQueue::s_instanceMutex;

AsyncMessageQueue::AsyncMessageQueue(QObject *parent)
    : QObject(parent)
    , _totalEnqueued(0)
    , _totalProcessed(0)
    , _totalFailed(0)
    , _totalRetried(0)
    , _currentQueueSize(0)
    , _messagesPerSecond(0)
    , _initialized(false)
    , _shuttingDown(false)
    , _messageIdCounter(0)
{
    _lastResetTime = QDateTime::currentDateTime();
    
    // 创建定时器
    _retryTimer = new QTimer(this);
    _healthCheckTimer = new QTimer(this);
    
    connect(_retryTimer, &QTimer::timeout, this, &AsyncMessageQueue::handleRetryMessages);
    connect(_healthCheckTimer, &QTimer::timeout, this, &AsyncMessageQueue::performHealthCheck);
}

AsyncMessageQueue::~AsyncMessageQueue()
{
    shutdown();
}

AsyncMessageQueue* AsyncMessageQueue::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new AsyncMessageQueue();
        }
    }
    return s_instance;
}

bool AsyncMessageQueue::initialize(const QueueConfig& config)
{
    QMutexLocker locker(&_queueMutex);
    
    if (_initialized) {
        LOG_WARNING("Async message queue already initialized");
        return true;
    }
    
    _config = config;
    _shuttingDown = false;
    
    LOG_INFO(QString("Initializing async message queue: threads=%1, batchSize=%2, maxQueue=%3")
             .arg(_config.workerThreads).arg(_config.batchSize).arg(_config.maxQueueSize));
    
    // 创建工作线程
    for (int i = 0; i < _config.workerThreads; ++i) {
        QThread* thread = new QThread(this);
        MessageWorker* worker = new MessageWorker(this);
        
        worker->moveToThread(thread);
        
        // 连接信号
        connect(thread, &QThread::started, worker, &MessageWorker::processMessageBatch);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        
        _workerThreads.append(thread);
        _workers.append(worker);
        
        thread->start();
    }
    
    // 启动定时器
    _retryTimer->start(_config.retryDelay);
    _healthCheckTimer->start(30000); // 30秒健康检查
    
    _initialized = true;
    
    LOG_INFO(QString("Async message queue initialized with %1 worker threads")
             .arg(_config.workerThreads));
    
    return true;
}

void AsyncMessageQueue::shutdown()
{
    if (!_initialized || _shuttingDown) {
        return;
    }
    
    // LOG_INFO removed
    
    _shuttingDown = true;
    
    // 停止定时器
    _retryTimer->stop();
    _healthCheckTimer->stop();
    
    // 唤醒所有等待的工作线程
    _messageAvailable.wakeAll();
    
    // 等待工作线程完成
    for (QThread* thread : _workerThreads) {
        thread->quit();
        if (!thread->wait(5000)) {
            LOG_WARNING("Force terminating worker thread");
            thread->terminate();
            thread->wait(1000);
        }
    }
    
    // 清理资源
    _workerThreads.clear();
    _workers.clear();
    
    QMutexLocker locker(&_queueMutex);
    _messageQueue.clear();
    _retryQueue.clear();
    _currentQueueSize.storeRelease(0);
    
    _initialized = false;
    

}

QString AsyncMessageQueue::enqueueMessage(qint64 userId, const QString& clientId, 
                                        const QJsonObject& message, MessagePriority priority)
{
    if (_shuttingDown) {
        return QString();
    }
    
    QMutexLocker locker(&_queueMutex);
    
    // 检查队列是否已满
    if (_messageQueue.size() >= _config.maxQueueSize) {
        if (_config.enableFlowControl) {
            // 流量控制：丢弃低优先级消息
            if (priority >= Normal) {
                LOG_WARNING("Message queue full, dropping low priority message");
                emit queueFullWarning(_messageQueue.size());
                return QString();
            }
        } else {
            LOG_ERROR("Message queue full, cannot enqueue message");
            emit queueFullWarning(_messageQueue.size());
            return QString();
        }
    }
    
    // 创建消息
    Message msg;
    msg.messageId = generateMessageId();
    msg.userId = userId;
    msg.clientId = clientId;
    msg.content = message;
    msg.priority = priority;
    msg.timestamp = QDateTime::currentDateTime();
    msg.retryCount = 0;
    
    // 根据优先级插入队列
    bool inserted = false;
    QQueue<Message>::iterator it = _messageQueue.begin();
    while (it != _messageQueue.end()) {
        if (msg.priority < it->priority) {
            _messageQueue.insert(it, msg);
            inserted = true;
            break;
        }
        ++it;
    }
    
    if (!inserted) {
        _messageQueue.enqueue(msg);
    }
    
    _currentQueueSize.fetchAndAddOrdered(1);
    _totalEnqueued.fetchAndAddOrdered(1);
    
    // 唤醒工作线程
    _messageAvailable.wakeOne();
    
    return msg.messageId;
}

int AsyncMessageQueue::broadcastMessage(const QJsonObject& message, MessagePriority priority)
{
    // 这里需要获取所有在线用户列表
    // 简化实现，实际应该从TcpServer获取
    QList<qint64> allUsers; // 应该从服务器获取所有在线用户
    
    return sendToUsers(allUsers, message, priority);
}

int AsyncMessageQueue::sendToUsers(const QList<qint64>& userIds, const QJsonObject& message, 
                                 MessagePriority priority)
{
    int successCount = 0;
    
    for (qint64 userId : userIds) {
        QString messageId = enqueueMessage(userId, QString(), message, priority);
        if (!messageId.isEmpty()) {
            successCount++;
        }
    }
    
    return successCount;
}

QJsonObject AsyncMessageQueue::getStatistics() const
{
    QMutexLocker locker(&_queueMutex);
    
    QJsonObject stats;
    stats["initialized"] = _initialized;
    stats["current_queue_size"] = _currentQueueSize.loadAcquire();
    stats["retry_queue_size"] = _retryQueue.size();
    stats["total_enqueued"] = _totalEnqueued.loadAcquire();
    stats["total_processed"] = _totalProcessed.loadAcquire();
    stats["total_failed"] = _totalFailed.loadAcquire();
    stats["total_retried"] = _totalRetried.loadAcquire();
    stats["messages_per_second"] = _messagesPerSecond.loadAcquire();
    stats["worker_threads"] = _config.workerThreads;
    stats["max_queue_size"] = _config.maxQueueSize;
    stats["batch_size"] = _config.batchSize;
    
    return stats;
}

int AsyncMessageQueue::queueSize() const
{
    return _currentQueueSize.loadAcquire();
}

bool AsyncMessageQueue::isHealthy() const
{
    return _initialized && !_shuttingDown && 
           _currentQueueSize.loadAcquire() < _config.flowControlThreshold;
}

void AsyncMessageQueue::clearQueue()
{
    QMutexLocker locker(&_queueMutex);
    
    int clearedCount = _messageQueue.size() + _retryQueue.size();
    _messageQueue.clear();
    _retryQueue.clear();
    _currentQueueSize.storeRelease(0);
    
    LOG_INFO(QString("Cleared %1 messages from queue").arg(clearedCount));
}

void AsyncMessageQueue::processMessages()
{
    // 这个方法由工作线程调用
    while (!_shuttingDown) {
        QList<Message> batch = getNextBatch(_config.batchSize);
        
        if (batch.isEmpty()) {
            // 没有消息，等待
            QMutexLocker locker(&_queueMutex);
            if (!_shuttingDown && _messageQueue.isEmpty()) {
                _messageAvailable.wait(&_queueMutex, _config.processingInterval);
            }
            continue;
        }
        
        // 处理批次消息
        for (const Message& message : batch) {
            bool success = sendMessage(message);
            
            if (success) {
                _totalProcessed.fetchAndAddOrdered(1);
                emit messageProcessed(message.messageId, true);
            } else {
                _totalFailed.fetchAndAddOrdered(1);
                
                // 添加到重试队列
                if (message.retryCount < _config.maxRetryCount) {
                    Message retryMsg = message;
                    retryMsg.retryCount++;
                    addRetryMessage(retryMsg);
                    _totalRetried.fetchAndAddOrdered(1);
                } else {
                    LOG_ERROR(QString("Message failed after %1 retries: %2")
                             .arg(_config.maxRetryCount).arg(message.messageId));
                    emit messageProcessed(message.messageId, false);
                }
            }
        }
        
        // 更新每秒消息数统计
        _messagesPerSecond.fetchAndAddOrdered(batch.size());
    }
}

void AsyncMessageQueue::handleRetryMessages()
{
    QMutexLocker locker(&_queueMutex);
    
    if (_retryQueue.isEmpty()) {
        return;
    }
    
    // 将重试消息重新加入主队列
    while (!_retryQueue.isEmpty()) {
        Message retryMsg = _retryQueue.dequeue();
        
        // 根据优先级插入
        bool inserted = false;
        QQueue<Message>::iterator it = _messageQueue.begin();
        while (it != _messageQueue.end()) {
            if (retryMsg.priority < it->priority) {
                _messageQueue.insert(it, retryMsg);
                inserted = true;
                break;
            }
            ++it;
        }
        
        if (!inserted) {
            _messageQueue.enqueue(retryMsg);
        }
    }
    
    _messageAvailable.wakeAll();
}

void AsyncMessageQueue::performHealthCheck()
{
    // 重置每秒消息数计数器
    QDateTime now = QDateTime::currentDateTime();
    if (_lastResetTime.secsTo(now) >= 1) {
        _messagesPerSecond.storeRelease(0);
        _lastResetTime = now;
    }
    
    // 检查队列健康状态
    int currentSize = _currentQueueSize.loadAcquire();
    if (currentSize > _config.flowControlThreshold) {
        LOG_WARNING(QString("Message queue size exceeds threshold: %1/%2")
                   .arg(currentSize).arg(_config.maxQueueSize));
        emit queueFullWarning(currentSize);
    }
    

}

QString AsyncMessageQueue::generateMessageId()
{
    return QString("msg_%1_%2")
           .arg(QDateTime::currentMSecsSinceEpoch())
           .arg(_messageIdCounter.fetchAndAddOrdered(1));
}

bool AsyncMessageQueue::sendMessage(const Message& message)
{
    // 实际发送消息到客户端
    try {
        // 注意：这里不能直接调用TcpServer::instance()，因为可能导致循环依赖
        // 应该通过信号槽机制或者依赖注入来发送消息

        // 暂时禁用AsyncMessageQueue的消息发送，避免重复发送
        // 所有消息发送都通过ClientHandler::sendMessage进行
        LOG_WARNING(QString("AsyncMessageQueue::sendMessage called but disabled to prevent duplicate messages: %1").arg(message.messageId));

        // 返回false，这样消息会被重试，但是由于重试次数限制，最终会被丢弃
        return false;

    } catch (...) {
        LOG_ERROR(QString("Failed to send message: %1").arg(message.messageId));
        return false;
    }
}

QList<Message> AsyncMessageQueue::getNextBatch(int batchSize)
{
    QMutexLocker locker(&_queueMutex);
    
    QList<Message> batch;
    int count = qMin(batchSize, _messageQueue.size());
    
    for (int i = 0; i < count; ++i) {
        if (!_messageQueue.isEmpty()) {
            batch.append(_messageQueue.dequeue());
            _currentQueueSize.fetchAndSubOrdered(1);
        }
    }
    
    return batch;
}

void AsyncMessageQueue::addRetryMessage(const Message& message)
{
    QMutexLocker locker(&_queueMutex);
    _retryQueue.enqueue(message);
}

#include "AsyncMessageQueue.moc"
