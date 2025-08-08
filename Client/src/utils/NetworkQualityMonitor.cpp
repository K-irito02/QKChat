#include "NetworkQualityMonitor.h"
#include "../utils/Logger.h"
#include <QDateTime>

#include <QtMath>

NetworkQualityMonitor::NetworkQualityMonitor(QObject *parent)
    : QObject(parent)
    , _averageRtt(0)
    , _networkQuality(50) // 默认中等质量
    , _latestRtt(0)
{
}

NetworkQualityMonitor::~NetworkQualityMonitor()
{
}

void NetworkQualityMonitor::recordHeartbeatSent(const QString& requestId)
{
    QMutexLocker locker(&_mutex);
    
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    _heartbeatTimestamps[requestId] = timestamp;
}

void NetworkQualityMonitor::recordHeartbeatReceived(const QString& requestId)
{
    QMutexLocker locker(&_mutex);
    
    if (!_heartbeatTimestamps.contains(requestId)) {
        LOG_WARNING(QString("Received heartbeat response for unknown request: %1").arg(requestId));
        return;
    }
    
    qint64 sentTime = _heartbeatTimestamps[requestId];
    qint64 receivedTime = QDateTime::currentMSecsSinceEpoch();
    int rtt = static_cast<int>(receivedTime - sentTime);
    
    // 移除已处理的时间戳
    _heartbeatTimestamps.remove(requestId);
    
    // 记录RTT
    _latestRtt = rtt;
    _rttHistory.append(rtt);
    
    // 限制历史记录大小
    if (_rttHistory.size() > MAX_HISTORY_SIZE) {
        _rttHistory.removeFirst();
    }
    
    // 更新统计数据
    updateAverageRtt();
    calculateNetworkQuality();
}

int NetworkQualityMonitor::getNetworkQuality() const
{
    QMutexLocker locker(&_mutex);
    return _networkQuality;
}

int NetworkQualityMonitor::getAverageRtt() const
{
    QMutexLocker locker(&_mutex);
    return _averageRtt;
}

int NetworkQualityMonitor::getLatestRtt() const
{
    QMutexLocker locker(&_mutex);
    return _latestRtt;
}

void NetworkQualityMonitor::reset()
{
    QMutexLocker locker(&_mutex);
    
    _heartbeatTimestamps.clear();
    _rttHistory.clear();
    _averageRtt = 0;
    _networkQuality = 50; // 重置为中等质量
    _latestRtt = 0;
}

bool NetworkQualityMonitor::hasEnoughData() const
{
    QMutexLocker locker(&_mutex);
    return _rttHistory.size() >= MIN_DATA_POINTS;
}

void NetworkQualityMonitor::updateAverageRtt()
{
    if (_rttHistory.isEmpty()) {
        _averageRtt = 0;
        return;
    }
    
    int sum = 0;
    for (int rtt : _rttHistory) {
        sum += rtt;
    }
    _averageRtt = sum / _rttHistory.size();
}

void NetworkQualityMonitor::calculateNetworkQuality()
{
    if (_rttHistory.size() < MIN_DATA_POINTS) {
        // 数据不足，保持默认质量
        return;
    }
    
    int oldQuality = _networkQuality;
    
    // 基于平均RTT计算质量评分
    if (_averageRtt <= EXCELLENT_RTT) {
        _networkQuality = 90 + qMin(10, (EXCELLENT_RTT - _averageRtt) / 5);
    } else if (_averageRtt <= GOOD_RTT) {
        _networkQuality = 70 + qMin(20, (GOOD_RTT - _averageRtt) * 2 / 5);
    } else if (_averageRtt <= FAIR_RTT) {
        _networkQuality = 50 + qMin(20, (FAIR_RTT - _averageRtt) / 10);
    } else if (_averageRtt <= POOR_RTT) {
        _networkQuality = 30 + qMin(20, (POOR_RTT - _averageRtt) / 15);
    } else {
        _networkQuality = qMax(10, 30 - (_averageRtt - POOR_RTT) / 50);
    }
    
    // 考虑RTT稳定性（标准差）
    if (_rttHistory.size() >= 3) {
        int variance = 0;
        for (int rtt : _rttHistory) {
            variance += (rtt - _averageRtt) * (rtt - _averageRtt);
        }
        int stdDev = static_cast<int>(qSqrt(variance / _rttHistory.size()));
        
        // 稳定性调整：标准差越大，质量评分越低
        if (stdDev > 100) {
            _networkQuality = qMax(10, _networkQuality - 20);
        } else if (stdDev > 50) {
            _networkQuality = qMax(10, _networkQuality - 10);
        }
    }
    
    // 确保质量评分在有效范围内
    _networkQuality = qBound(0, _networkQuality, 100);
    
    // 如果质量评分发生变化，发出信号
    if (oldQuality != _networkQuality) {
        LOG_INFO(QString("Network quality changed: %1 -> %2 (avg RTT: %3ms)")
                 .arg(oldQuality).arg(_networkQuality).arg(_averageRtt));
        emit networkQualityChanged(_networkQuality);
    }
} 