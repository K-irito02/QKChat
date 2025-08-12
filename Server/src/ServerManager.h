#ifndef SERVERMANAGER_H
#define SERVERMANAGER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include "database/DatabaseManager.h"
#include "database/RedisClient.h"
#include "auth/EmailService.h"
#include "network/ThreadPoolServer.h"
#include "network/AsyncMessageQueue.h"
#include "network/ProtocolHandler.h"
#include "utils/Logger.h"

/**
 * @brief 服务器管理器类
 * 
 * 负责协调和管理所有服务器组件，包括数据库连接池、Redis、邮件服务、
 * 线程池服务器、异步消息队列等。提供统一的服务器启动、停止和状态监控功能。
 */
class ServerManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 服务器状态枚举
     */
    enum ServerState {
        Stopped,
        Starting,
        Running,
        Stopping,
        Error
    };
    Q_ENUM(ServerState)

    explicit ServerManager(QObject *parent = nullptr);
    ~ServerManager();
    
    /**
     * @brief 获取单例实例
     * @return 服务器管理器实例
     */
    static ServerManager* instance();
    
    /**
     * @brief 初始化服务器
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 启动服务器
     * @param port TCP服务器端口
     * @return 启动是否成功
     */
    bool startServer(quint16 port = 8080);
    
    /**
     * @brief 停止服务器
     */
    void stopServer();
    
    /**
     * @brief 获取服务器状态
     * @return 服务器状态
     */
    ServerState serverState() const { return _serverState; }
    
    /**
     * @brief 获取服务器统计信息
     * @return 统计信息JSON对象
     */
    QJsonObject getServerStatistics() const;
    
    /**
     * @brief 获取连接的客户端数量
     * @return 客户端数量
     */
    int getClientCount() const;
    
    /**
     * @brief 获取在线用户数量
     * @return 在线用户数量
     */
    int getOnlineUserCount() const;

signals:
    /**
     * @brief 服务器状态改变信号
     * @param state 新状态
     */
    void serverStateChanged(ServerState state);
    
    /**
     * @brief 客户端连接信号
     * @param clientCount 当前客户端数量
     */
    void clientConnected(int clientCount);
    
    /**
     * @brief 客户端断开连接信号
     * @param clientCount 当前客户端数量
     */
    void clientDisconnected(int clientCount);
    
    /**
     * @brief 用户登录信号
     * @param userId 用户ID
     * @param username 用户名
     */
    void userLoggedIn(qint64 userId, const QString &username);
    
    /**
     * @brief 用户注册信号
     * @param userId 用户ID
     * @param username 用户名
     * @param email 邮箱
     */
    void userRegistered(qint64 userId, const QString &username, const QString &email);
    
    /**
     * @brief 服务器错误信号
     * @param error 错误信息
     */
    void serverError(const QString &error);

private slots:
    void onThreadPoolClientConnected(ClientHandler* client);
    void onThreadPoolClientDisconnected(ClientHandler* client);
    void onThreadPoolUserLoggedIn(qint64 userId, ClientHandler* client);
    void onThreadPoolUserLoggedOut(qint64 userId);
    void onProtocolUserLoggedIn(qint64 userId, const QString &clientId, const QString &sessionToken);
    void onProtocolUserRegistered(qint64 userId, const QString &username, const QString &email);
    void onDatabaseConnectionChanged(bool connected);
    void onRedisConnectionChanged(bool connected);
    void onMessageQueueError(const QString &error);

private:
    /**
     * @brief 设置服务器状态
     * @param state 新状态
     */
    void setServerState(ServerState state);
    
    /**
     * @brief 初始化数据库连接池
     * @return 初始化是否成功
     */
    bool initializeDatabasePool();
    
    /**
     * @brief 初始化Redis
     * @return 初始化是否成功
     */
    bool initializeRedis();
    
    /**
     * @brief 初始化邮件服务
     * @return 初始化是否成功
     */
    bool initializeEmailService();
    
    /**
     * @brief 初始化线程池服务器
     * @return 初始化是否成功
     */
    bool initializeThreadPoolServer();
    
    /**
     * @brief 初始化异步消息队列
     * @return 初始化是否成功
     */
    bool initializeMessageQueue();

private slots:
    /**
     * @brief 异步初始化证书
     */
    void initializeCertificatesAsync();

    /**
     * @brief 异步初始化可选组件
     */
    void initializeOptionalComponentsAsync();

private:
    static ServerManager* s_instance;
    
    ServerState _serverState;
    quint16 _serverPort;
    
    // 服务组件
    DatabaseManager* _databaseManager;
    RedisClient* _redisClient;
    EmailService* _emailService;
    ThreadPoolServer* _threadPoolServer;
    AsyncMessageQueue* _messageQueue;
    ProtocolHandler* _protocolHandler;
    
    // 统计信息
    int _clientCount;
    qint64 _totalConnections;
    qint64 _totalRegistrations;
    QDateTime _startTime;
};

#endif // SERVERMANAGER_H