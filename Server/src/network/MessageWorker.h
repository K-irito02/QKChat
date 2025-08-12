#ifndef MESSAGEWORKER_H
#define MESSAGEWORKER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QJsonObject>

// 前向声明
class AsyncMessageQueue;

/**
 * @brief 消息工作线程类
 * 负责处理异步消息队列中的消息
 */
class MessageWorker : public QObject
{
    Q_OBJECT

public:
    explicit MessageWorker(AsyncMessageQueue* queue, QObject* parent = nullptr);
    ~MessageWorker();

public slots:
    /**
     * @brief 处理消息批次
     */
    void processMessageBatch();

private:
    AsyncMessageQueue* _queue;
};

#endif // MESSAGEWORKER_H 