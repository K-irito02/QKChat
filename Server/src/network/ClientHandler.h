#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QSslSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QHostAddress>

/**
 * @brief 客户端处理器类
 * 
 * 负责处理单个客户端的连接、认证、消息收发等功能。
 * 支持TLS加密通信和心跳检测机制。
 */
class ClientHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 客户端状态枚举
     */
    enum ClientState {
        Connected,
        Authenticating,
        Authenticated,
        Disconnected,
        Error
    };
    Q_ENUM(ClientState)

    explicit ClientHandler(qintptr socketDescriptor, bool useTLS = true, QObject *parent = nullptr);
    ~ClientHandler();
    
    /**
     * @brief 获取客户端ID
     * @return 客户端ID
     */
    QString clientId() const { return _clientId; }
    
    /**
     * @brief 获取用户ID
     * @return 用户ID，未认证返回-1
     */
    qint64 userId() const { return _userId; }
    
    /**
     * @brief 获取客户端状态
     * @return 客户端状态
     */
    ClientState state() const { return _state; }
    
    /**
     * @brief 获取客户端IP地址
     * @return IP地址
     */
    QHostAddress peerAddress() const;
    
    /**
     * @brief 获取连接时间
     * @return 连接时间
     */
    QDateTime connectTime() const { return _connectTime; }
    
    /**
     * @brief 获取最后活动时间
     * @return 最后活动时间
     */
    QDateTime lastActivity() const { return _lastActivity; }
    
    /**
     * @brief 检查是否已认证
     * @return 是否已认证
     */
    bool isAuthenticated() const { return _state == Authenticated; }
    
    /**
     * @brief 检查连接是否有效
     * @return 连接是否有效
     */
    bool isConnected() const;
    
    /**
     * @brief 发送JSON消息
     * @param message JSON消息
     * @return 发送是否成功
     */
    bool sendMessage(const QJsonObject &message);
    
    /**
     * @brief 断开连接
     * @param reason 断开原因
     */
    void disconnect(const QString &reason = "");
    
    /**
     * @brief 设置TLS证书
     * @param certFile 证书文件路径
     * @param keyFile 私钥文件路径
     * @return 设置是否成功
     */
    bool setTlsCertificate(const QString &certFile, const QString &keyFile);
    
    /**
     * @brief 设置心跳超时时间
     * @param timeout 超时时间（毫秒）
     */
    void setHeartbeatTimeout(int timeout);
    
    /**
     * @brief 检查心跳超时
     * @return 是否超时
     */
    bool isHeartbeatTimeout() const;
    
    /**
     * @brief 获取客户端信息
     * @return 客户端信息JSON对象
     */
    QJsonObject getClientInfo() const;

signals:
    /**
     * @brief 客户端已连接信号
     */
    void connected();
    
    /**
     * @brief 客户端已断开连接信号
     */
    void disconnected();
    
    /**
     * @brief 客户端已认证信号
     * @param userId 用户ID
     */
    void authenticated(qint64 userId);
    
    /**
     * @brief 接收到消息信号
     * @param message JSON消息
     */
    void messageReceived(const QJsonObject &message);
    
    /**
     * @brief 客户端错误信号
     * @param error 错误信息
     */
    void clientError(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);

private:
    /**
     * @brief 处理接收到的数据
     * @param data 数据
     */
    void processReceivedData(const QByteArray &data);
    
    /**
     * @brief 处理JSON消息
     * @param message JSON消息
     */
    void processMessage(const QJsonObject &message);
    
    /**
     * @brief 处理认证请求
     * @param message 认证消息
     */
    void handleAuthRequest(const QJsonObject &message);
    
    /**
     * @brief 处理心跳消息
     * @param message 心跳消息
     */
    void handleHeartbeat(const QJsonObject &message);
    
    /**
     * @brief 发送认证响应
     * @param success 认证是否成功
     * @param message 响应消息
     * @param userData 用户数据（可选）
     */
    void sendAuthResponse(bool success, const QString &message, const QJsonObject &userData = QJsonObject());
    
    /**
     * @brief 发送心跳响应
     */
    void sendHeartbeatResponse();
    
    /**
     * @brief 发送错误响应
     * @param requestId 请求ID
     * @param error 错误信息
     */
    void sendErrorResponse(const QString &requestId, const QString &error);
    
    /**
     * @brief 更新最后活动时间
     */
    void updateLastActivity();
    
    /**
     * @brief 设置客户端状态
     * @param state 新状态
     */
    void setState(ClientState state);
    
    /**
     * @brief 生成客户端ID
     * @return 唯一客户端ID
     */
    QString generateClientId();

private:
    QSslSocket* _socket;
    QString _clientId;
    qint64 _userId;
    ClientState _state;
    
    QDateTime _connectTime;
    QDateTime _lastActivity;
    int _heartbeatTimeout;
    
    QByteArray _receiveBuffer;
    
    bool _useTLS;
    QString _certFile;
    QString _keyFile;
    
    // 统计信息
    qint64 _messagesSent;
    qint64 _messagesReceived;
    qint64 _bytesReceived;
    qint64 _bytesSent;
    
    static int s_clientCounter;
};

#endif // CLIENTHANDLER_H
