#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QSslSocket>
#include <QTimer>
#include <QMap>
#include <QMutex>
#include "ClientHandler.h"

// 前向声明
class ProtocolHandler;

/**
 * @brief TCP服务器类
 * 
 * 负责管理客户端连接，支持TLS加密通信。
 * 提供多客户端连接管理、心跳检测和连接状态监控功能。
 */
class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();
    
    /**
     * @brief 启动服务器
     * @param port 监听端口
     * @param address 监听地址
     * @param useTLS 是否使用TLS加密
     * @return 启动是否成功
     */
    bool startServer(quint16 port, const QHostAddress &address = QHostAddress::Any, bool useTLS = true);
    
    /**
     * @brief 停止服务器
     */
    void stopServer();
    
    /**
     * @brief 获取连接的客户端数量
     * @return 客户端数量
     */
    int clientCount() const;
    
    /**
     * @brief 获取所有客户端处理器
     * @return 客户端处理器列表
     */
    QList<ClientHandler*> getClients() const;
    
    /**
     * @brief 根据用户ID获取客户端处理器
     * @param userId 用户ID
     * @return 客户端处理器，未找到返回nullptr
     */
    ClientHandler* getClientByUserId(qint64 userId);
    
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
    
    /**
     * @brief 断开指定用户的连接
     * @param userId 用户ID
     * @return 断开是否成功
     */
    bool disconnectUser(qint64 userId);
    
    /**
     * @brief 设置TLS证书和私钥
     * @param certFile 证书文件路径
     * @param keyFile 私钥文件路径
     * @return 设置是否成功
     */
    bool setTlsCertificate(const QString &certFile, const QString &keyFile);
    
    /**
     * @brief 设置心跳检测间隔
     * @param interval 间隔时间（毫秒）
     */
    void setHeartbeatInterval(int interval);
    
    /**
     * @brief 设置最大客户端连接数
     * @param maxClients 最大连接数
     */
    void setMaxClients(int maxClients);

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
    void onClientDisconnected();
    void onClientAuthenticated(qint64 userId);
    void onClientError(const QString &error);
    void onHeartbeatTimer();

private:
    /**
     * @brief 清理断开的客户端
     */
    void cleanupClients();
    
    /**
     * @brief 检查客户端心跳
     */
    void checkClientHeartbeats();
    
    /**
     * @brief 生成客户端ID
     * @return 唯一客户端ID
     */
    QString generateClientId();

private:
    QMap<QString, ClientHandler*> _clients;          // 客户端ID -> 客户端处理器
    QMap<qint64, ClientHandler*> _userClients;       // 用户ID -> 客户端处理器
    QTimer* _heartbeatTimer;
    ProtocolHandler* _protocolHandler;

    bool _useTLS;
    QString _certFile;
    QString _keyFile;
    
    int _heartbeatInterval;
    int _maxClients;
    int _clientIdCounter;
    
    mutable QMutex _clientsMutex;
    
    // 统计信息
    qint64 _totalConnections;
    qint64 _totalMessages;
    QDateTime _startTime;
};

#endif // TCPSERVER_H
