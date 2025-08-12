#include "MessageWorker.h"
#include "AsyncMessageQueue.h"
#include "../utils/Logger.h"
#include <QThread>
#include <QTimer>

MessageWorker::MessageWorker(AsyncMessageQueue* queue, QObject* parent)
    : QObject(parent)
    , _queue(queue)
{

}

MessageWorker::~MessageWorker()
{

}

void MessageWorker::processMessageBatch()
{
    if (!_queue) {
        LOG_ERROR("MessageWorker: queue is null");
        return;
    }


    
    // 这里的具体实现将在AsyncMessageQueue中处理
    // MessageWorker主要负责线程管理和信号连接
} 