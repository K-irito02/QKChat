#ifndef NETWORKQUALITYMONITOR_H
#define NETWORKQUALITYMONITOR_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QMutex>
#include <QString>

/**
 * @brief 网络质量监控器
 * 
 * 负责监控网络连接质量，包括：
 * - RTT延迟统计
 * - 网络质量评分
 * - 历史数据管理
 */
class NetworkQualityMonitor : public QObject
{
    Q_OBJECT

public:
    explicit NetworkQualityMonitor(QObject *parent = nullptr);
    ~NetworkQualityMonitor();

    /**
     * @brief 记录心跳发送时间
     * @param requestId 请求ID
     */
    void recordHeartbeatSent(const QString& requestId);

    /**
     * @brief 记录心跳接收时间
     * @param requestId 请求ID
     */
    void recordHeartbeatReceived(const QString& requestId);

    /**
     * @brief 获取网络质量评分 (0-100)
     * @return 网络质量评分
     */
    int getNetworkQuality() const;

    /**
     * @brief 获取平均RTT延迟 (毫秒)
     * @return 平均RTT延迟
     */
    int getAverageRtt() const;

    /**
     * @brief 获取最新RTT延迟 (毫秒)
     * @return 最新RTT延迟
     */
    int getLatestRtt() const;

    /**
     * @brief 重置监控数据
     */
    void reset();

    /**
     * @brief 检查是否有足够的RTT数据
     * @return 是否有足够数据
     */
    bool hasEnoughData() const;

signals:
    /**
     * @brief 网络质量变化信号
     * @param quality 新的网络质量评分
     */
    void networkQualityChanged(int quality);

private:
    /**
     * @brief 计算网络质量评分
     */
    void calculateNetworkQuality();

    /**
     * @brief 更新平均RTT
     */
    void updateAverageRtt();

private:
    mutable QMutex _mutex; // 线程安全保护
    
    QMap<QString, qint64> _heartbeatTimestamps; // requestId -> timestamp
    QList<int> _rttHistory; // 最近RTT记录
    int _averageRtt; // 平均RTT延迟
    int _networkQuality; // 网络质量评分 (0-100)
    int _latestRtt; // 最新RTT延迟
    
    static const int MAX_HISTORY_SIZE = 10; // 最大历史记录数
    static const int MIN_DATA_POINTS = 3; // 最小数据点数
    static const int EXCELLENT_RTT = 50; // 优秀RTT阈值
    static const int GOOD_RTT = 100; // 良好RTT阈值
    static const int FAIR_RTT = 200; // 一般RTT阈值
    static const int POOR_RTT = 500; // 较差RTT阈值
};

#endif // NETWORKQUALITYMONITOR_H 