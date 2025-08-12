#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QObject>
#include <QTcpSocket>
// #include <QSslSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
// #include <QSslConfiguration>
// #include <QSslError>
#include <QAbstractSocket>
#include <QMap>
#include <QMutex>
#include "../utils/NetworkQualityMonitor.h"
#include "../utils/SmartErrorHandler.h"

/**
 * @brief 网络客户端类
 * 
 * 负责与服务器的网络通信，支持TLS加密连接。
 * 提供登录、注册等认证相关的网络请求功能。
 */
class NetworkClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 连接状态枚举
     */
    enum ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting,
        Error
    };
    Q_ENUM(ConnectionState)

    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient();
    
    /**
     * @brief 连接到服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @param useTLS 是否使用TLS加密
     * @return 连接是否成功启动
     */
    bool connectToServer(const QString &host, quint16 port, bool useTLS = true);
    
    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromServer();
    
    /**
     * @brief 发送登录请求
     * @param username 用户名或邮箱
     * @param password 原始密码
     * @param rememberMe 是否记住登录
     * @return 请求ID
     */
    QString sendLoginRequest(const QString &username, const QString &password, bool rememberMe = false);
    
    /**
     * @brief 发送注册请求
     * @param username 用户名
     * @param email 邮箱
     * @param password 原始密码
     * @param verificationCode 验证码
     * @return 请求ID
     */
    QString sendRegisterRequest(const QString &username, const QString &email, 
                               const QString &password, const QString &verificationCode);
    
    /**
     * @brief 发送验证码请求
     * @param email 邮箱地址
     * @return 请求ID
     */
    QString sendVerificationCodeRequest(const QString &email);
    
    /**
     * @brief 发送检查用户名可用性请求
     * @param username 用户名
     * @return 请求ID
     */
    QString sendCheckUsernameRequest(const QString &username);
    
    /**
     * @brief 发送检查邮箱可用性请求
     * @param email 邮箱地址
     * @return 请求ID
     */
    QString sendCheckEmailRequest(const QString &email);
    
    /**
     * @brief 发送心跳包
     */
    void sendHeartbeat();
    
    /**
     * @brief 获取连接状态
     * @return 连接状态
     */
    ConnectionState connectionState() const { return _connectionState; }
    
    /**
     * @brief 检查是否已连接
     * @return 是否已连接
     */
    bool isConnected() const;
    
    /**
     * @brief 设置连接超时时间
     * @param timeout 超时时间（毫秒）
     */
    void setConnectionTimeout(int timeout);
    
    /**
     * @brief 设置心跳间隔
     * @param interval 心跳间隔（毫秒）
     */
    void setHeartbeatInterval(int interval);
    
    /**
     * @brief 获取单例实例
     */
    static NetworkClient* instance();
    
    /**
     * @brief 发送消息
     * @param message 消息数据
     */
    void sendMessage(const QJsonObject& message);
    
    /**
     * @brief 获取客户端ID
     * @return 客户端ID
     */
    QString clientId() const;
    
    /**
     * @brief 检查是否已认证
     * @return 是否已认证
     */
    bool isAuthenticated() const;
    
    /**
     * @brief 获取会话令牌
     * @return 会话令牌
     */
    QString sessionToken() const;

    /**
     * @brief 设置认证状态
     * @param authenticated 是否已认证
     * @param token 会话令牌
     */
    void setAuthenticated(bool authenticated, const QString& token = QString());

    /**
     * @brief 发送聊天请求
     * @param request 请求数据
     * @return 请求ID
     */
    QString sendChatRequest(const QJsonObject &request);

signals:
    /**
     * @brief 连接状态改变信号
     * @param state 新的连接状态
     */
    void connectionStateChanged(ConnectionState state);
    
    /**
     * @brief 登录响应信号
     * @param requestId 请求ID
     * @param response 响应数据
     */
    void loginResponse(const QString &requestId, const QJsonObject &response);
    
    /**
     * @brief 注册响应信号
     * @param requestId 请求ID
     * @param response 响应数据
     */
    void registerResponse(const QString &requestId, const QJsonObject &response);
    
    /**
     * @brief 验证码响应信号
     * @param requestId 请求ID
     * @param response 响应数据
     */
    void verificationCodeResponse(const QString &requestId, const QJsonObject &response);
    
    /**
     * @brief 用户名可用性检查响应
     * @param requestId 请求ID
     * @param response 响应数据
     */
    void usernameAvailabilityResponse(const QString &requestId, const QJsonObject &response);
    
    /**
     * @brief 邮箱可用性检查响应
     * @param requestId 请求ID
     * @param response 响应数据
     */
    void emailAvailabilityResponse(const QString &requestId, const QJsonObject &response);
    
    /**
     * @brief 网络错误信号
     * @param error 错误信息
     */
    void networkError(const QString &error);
    
    /**
     * @brief 消息接收信号
     * @param message 接收到的消息
     */
    void messageReceived(const QJsonObject &message);
    
    /**
     * @brief SSL错误信号
     * @param errors SSL错误列表
     */
    // void sslErrors(const QList<QSslError> &errors);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    // void onSslErrors(const QList<QSslError> &errors);
    void onConnectionTimeout();
    void onHeartbeatTimeout();
    void onReconnectTimer();

private:
    /**
     * @brief 设置连接状态
     * @param state 新状态
     */
    void setConnectionState(ConnectionState state);
    
    /**
     * @brief 发送JSON请求
     * @param request 请求数据
     * @return 请求ID
     */
    QString sendJsonRequest(const QJsonObject &request);
    
    /**
     * @brief 处理接收到的数据
     * @param data 数据
     */
    void processReceivedData(const QByteArray &data);
    
    /**
     * @brief 处理JSON响应
     * @param response 响应数据
     */
    void processJsonResponse(const QJsonObject &response);
    
    /**
     * @brief 生成请求ID
     * @return 唯一请求ID
     */
    QString generateRequestId();
    
    /**
     * @brief 配置SSL
     */

    
    /**
     * @brief 启动重连机制
     */
    void startReconnection();
    
    /**
     * @brief 处理重连成功
     */
    void handleReconnectionSuccess();
    
    /**
     * @brief 处理重连失败
     */
    void handleReconnectionFailure();
    
    /**
     * @brief 更新网络质量
     */
    void updateNetworkQuality();

private:
    QTcpSocket* _socket;
    ConnectionState _connectionState;
    QString _serverHost;
    quint16 _serverPort;
    bool _useTLS;
    
    QTimer* _connectionTimer;
    QTimer* _heartbeatTimer;
    QTimer* _reconnectTimer;
    int _connectionTimeout;
    int _heartbeatInterval;
    int _reconnectInterval;
    int _maxReconnectAttempts;
    int _currentReconnectAttempts;
    bool _autoReconnect;
    
    NetworkQualityMonitor* _qualityMonitor;
    SmartErrorHandler* _errorHandler;
    
    QByteArray _receiveBuffer;
    QMap<QString, QString> _pendingRequests; // requestId -> requestType
    QMutex _dataMutex; // 添加互斥锁保护共享数据
    
    // 认证相关
    QString _clientId;
    QString _sessionToken;
    bool _isAuthenticated;
    
    static int s_requestCounter;
    static QMutex s_counterMutex; // 保护静态计数器的互斥锁
    static NetworkClient* s_instance;
    static QMutex s_instanceMutex;
};

#endif // NETWORKCLIENT_H
