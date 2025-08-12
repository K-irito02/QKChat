#ifndef THREADPOOLSERVER_H
#define THREADPOOLSERVER_H

#include <QTcpServer>
#include <QThreadPool>
#include <QRunnable>
#include <QMutex>
#include <QAtomicInt>
#include <QTimer>
#include <QJsonObject>
#include "ClientHandler.h"

class ProtocolHandler;

/**
 * @brief 线程池服务器类
 * 
 * 使用线程池处理客户端连接，解决单线程架构的性能瓶颈。
 * 支持动态线程池管理、负载均衡、连接限流等功能。
 */
/**
 * @brief 服务器配置结构
 */
struct ServerConfig {
    int minThreads = 4;              // 最小线程数
    int maxThreads = 16;             // 最大线程数
    int maxClients = 5000;           // 最大客户端数
    int connectionTimeout = 30000;   // 连接超时时间(ms)
    int heartbeatInterval = 30000;   // 心跳间隔(ms)
    bool enableLoadBalancing = true; // 启用负载均衡
    bool enableRateLimiting = true;  // 启用速率限制
    int maxConnectionsPerIP = 10;    // 每IP最大连接数
    int ipVerificationCodeInterval = 30; // 每IP验证码发送间隔(秒)
    int emailVerificationCodeInterval = 60; // 每邮箱验证码发送间隔(秒)
};

class ThreadPoolServer : public QTcpServer
{
    Q_OBJECT

public:

    explicit ThreadPoolServer(QObject *parent = nullptr);
    ~ThreadPoolServer();
    
    /**
     * @brief 获取单例实例
     */
    static ThreadPoolServer* instance();
    
    /**
     * @brief 初始化服务器
     * @param config 服务器配置
     * @return 初始化是否成功
     */
    bool initialize(const ServerConfig& config = ServerConfig());
    
    /**
     * @brief 启动服务器
     * @param port 监听端口
     * @param address 监听地址
     * @param useTLS 是否使用TLS
     * @return 启动是否成功
     */
    bool startServer(quint16 port, const QHostAddress &address = QHostAddress::Any, bool useTLS = true);
    
    /**
     * @brief 停止服务器
     */
    void stopServer();
    
    /**
     * @brief 设置协议处理器
     * @param protocolHandler 协议处理器实例
     */
    void setProtocolHandler(ProtocolHandler *protocolHandler);
    
    /**
     * @brief 获取服务器统计信息
     * @return 统计信息JSON对象
     */
    QJsonObject getServerStatistics() const;
    
    /**
     * @brief 获取连接的客户端数量
     * @return 客户端数量
     */
    int clientCount() const;
    
    /**
     * @brief 广播消息给所有客户端
     * @param message JSON消息
     */
    void broadcastMessage(const QJsonObject &message);
    
    /**
     * @brief 发送消息给指定用户
     * @param userId 用户ID
     * @param message JSON消息
     * @return 发送是否成功
     */
    bool sendMessageToUser(qint64 userId, const QJsonObject &message);

signals:
    /**
     * @brief 客户端连接信号
     * @param client 客户端处理器
     */
    void clientConnected(ClientHandler* client);
    
    /**
     * @brief 客户端断开连接信号
     * @param client 客户端处理器
     */
    void clientDisconnected(ClientHandler* client);
    
    /**
     * @brief 用户登录信号
     * @param userId 用户ID
     * @param client 客户端处理器
     */
    void userLoggedIn(qint64 userId, ClientHandler* client);
    
    /**
     * @brief 用户登出信号
     * @param userId 用户ID
     */
    void userLoggedOut(qint64 userId);
    
    /**
     * @brief 服务器错误信号
     * @param error 错误信息
     */
    void serverError(const QString &error);

protected:
    /**
     * @brief 处理新的客户端连接
     * @param socketDescriptor 套接字描述符
     */
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientConnected(ClientHandler* client);
    void onClientDisconnected(ClientHandler* client);
    void onClientAuthenticated(qint64 userId, ClientHandler* client);
    void onClientMessageReceived(ClientHandler* client, const QJsonObject &message);
    void performHealthCheck();
    void balanceLoad();

private:
    /**
     * @brief 客户端工作任务类
     */
    class ClientTask : public QRunnable {
    public:
        ClientTask(qintptr socketDescriptor, ThreadPoolServer* server, 
                  ProtocolHandler* protocolHandler, bool useTLS);
        void run() override;
        
    private:
        qintptr _socketDescriptor;
        ThreadPoolServer* _server;
        ProtocolHandler* _protocolHandler;
        bool _useTLS;
    };
    
    /**
     * @brief 检查IP连接限制
     * @param address IP地址
     * @return 是否允许连接
     */
    bool checkIPLimit(const QHostAddress& address);
    
    /**
     * @brief 更新IP连接计数
     * @param address IP地址
     * @param increment 增量（1表示连接，-1表示断开）
     */
    void updateIPCount(const QHostAddress& address, int increment);
    
    /**
     * @brief 选择最佳工作线程
     * @return 线程池指针
     */
    QThreadPool* selectBestThreadPool();

private:
    static ThreadPoolServer* s_instance;
    static QMutex s_instanceMutex;
    
    ServerConfig _config;
    ProtocolHandler* _protocolHandler;
    
    // 线程池管理
    QList<QThreadPool*> _threadPools;
    QAtomicInt _currentPoolIndex;
    
    // 客户端管理
    QMap<QString, ClientHandler*> _clients;          // 客户端ID -> 客户端处理器
    QMap<qint64, ClientHandler*> _userClients;       // 用户ID -> 客户端处理器
    QMap<QString, int> _ipConnections;               // IP字符串 -> 连接数
    
    mutable QMutex _clientsMutex;
    mutable QMutex _ipMutex;
    
    // 统计信息
    QAtomicInt _totalConnections;
    QAtomicInt _activeConnections;
    QAtomicInt _rejectedConnections;
    QDateTime _startTime;
    
    // 定时器
    QTimer* _healthCheckTimer;
    QTimer* _loadBalanceTimer;
    
    bool _useTLS;
    bool _initialized;
    bool _running;
};

#endif // THREADPOOLSERVER_H