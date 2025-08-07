#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QSslSocket>
#include <QTimer>
#include <QQueue>
#include <QMutex>

/**
 * @brief SMTP客户端类
 * 
 * 实现真实的SMTP协议邮件发送功能，支持TLS加密和SMTP认证。
 * 提供异步发送、重试机制和错误处理功能。
 */
class SmtpClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief SMTP发送状态枚举
     */
    enum SmtpState {
        Disconnected,
        Connecting,
        Connected,
        Authenticating,
        Authenticated,
        Sending,
        Error
    };
    Q_ENUM(SmtpState)

    /**
     * @brief 邮件消息结构
     */
    struct EmailMessage {
        QString from;
        QString fromName;
        QString to;
        QString subject;
        QString body;
        bool isHtml;
        QStringList attachments;
        QString messageId;
        int retryCount;
        
        EmailMessage() : isHtml(false), retryCount(0) {}
    };

    explicit SmtpClient(QObject *parent = nullptr);
    ~SmtpClient();
    
    /**
     * @brief 配置SMTP服务器
     * @param host SMTP服务器地址
     * @param port SMTP端口
     * @param username 用户名
     * @param password 密码
     * @param useTls 是否使用TLS
     * @param useStartTls 是否使用STARTTLS
     */
    void configure(const QString &host, int port, const QString &username, 
                  const QString &password, bool useTls = true, bool useStartTls = true);
    
    /**
     * @brief 发送邮件
     * @param to 收件人邮箱
     * @param subject 邮件主题
     * @param body 邮件内容
     * @param isHtml 是否为HTML格式
     * @param fromName 发件人名称
     * @return 邮件ID
     */
    QString sendEmail(const QString &to, const QString &subject, const QString &body, 
                     bool isHtml = true, const QString &fromName = "");
    
    /**
     * @brief 发送邮件（完整参数）
     * @param message 邮件消息对象
     * @return 邮件ID
     */
    QString sendEmail(const EmailMessage &message);
    
    /**
     * @brief 获取当前状态
     * @return SMTP状态
     */
    SmtpState currentState() const { return _state; }
    
    /**
     * @brief 获取队列中的邮件数量
     * @return 邮件数量
     */
    int queueSize() const;
    
    /**
     * @brief 设置连接超时时间
     * @param timeout 超时时间（毫秒）
     */
    void setConnectionTimeout(int timeout) { _connectionTimeout = timeout; }
    
    /**
     * @brief 设置最大重试次数
     * @param maxRetries 最大重试次数
     */
    void setMaxRetries(int maxRetries) { _maxRetries = maxRetries; }
    
    /**
     * @brief 连接到SMTP服务器
     * @return 连接是否成功启动
     */
    bool connectToServer();
    
    /**
     * @brief 断开SMTP连接
     */
    void disconnectFromServer();

signals:
    /**
     * @brief 邮件发送成功信号
     * @param messageId 邮件ID
     */
    void emailSent(const QString &messageId);
    
    /**
     * @brief 邮件发送失败信号
     * @param messageId 邮件ID
     * @param error 错误信息
     */
    void emailFailed(const QString &messageId, const QString &error);
    
    /**
     * @brief 连接状态改变信号
     * @param state 新状态
     */
    void stateChanged(SmtpState state);
    
    /**
     * @brief SMTP错误信号
     * @param error 错误信息
     */
    void smtpError(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);
    void onConnectionTimeout();
    void processQueue();

private:
    /**
     * @brief 处理SMTP响应
     * @param response 服务器响应
     */
    void handleSmtpResponse(const QString &response);
    
    /**
     * @brief 处理邮件发送响应
     * @param responseCode 响应代码
     * @param responseText 响应文本
     */
    void handleEmailSendingResponse(int responseCode, const QString &responseText);
    
    /**
     * @brief 发送SMTP命令
     * @param command 命令
     */
    void sendCommand(const QString &command);
    
    /**
     * @brief 开始TLS握手
     */
    void startTls();
    
    /**
     * @brief 进行SMTP认证
     */
    void authenticate();
    
    /**
     * @brief 发送当前邮件
     */
    void sendCurrentEmail();
    
    /**
     * @brief 完成当前邮件发送
     * @param success 是否成功
     * @param error 错误信息
     */
    void finishCurrentEmail(bool success, const QString &error = "");
    
    /**
     * @brief 重试当前邮件
     */
    void retryCurrentEmail();
    
    /**
     * @brief 设置状态
     * @param state 新状态
     */
    void setState(SmtpState state);
    
    /**
     * @brief 生成邮件ID
     * @return 唯一邮件ID
     */
    QString generateMessageId();
    
    /**
     * @brief 编码邮件头
     * @param text 文本
     * @return 编码后的文本
     */
    QString encodeHeader(const QString &text);
    
    /**
     * @brief 格式化邮件内容
     * @param message 邮件消息
     * @return 格式化后的邮件内容
     */
    QString formatEmailContent(const EmailMessage &message);
    
    /**
     * @brief Base64编码
     * @param data 数据
     * @return 编码后的字符串
     */
    QString base64Encode(const QString &data);

private:
    // 网络连接
    QSslSocket* _socket;
    QTimer* _connectionTimer;
    QTimer* _queueTimer;
    
    // SMTP配置
    QString _host;
    int _port;
    QString _username;
    QString _password;
    bool _useTls;
    bool _useStartTls;
    
    // 状态管理
    SmtpState _state;
    QString _lastResponse;
    int _expectedResponseCode;
    
    // 邮件队列
    QQueue<EmailMessage> _emailQueue;
    EmailMessage _currentEmail;
    mutable QMutex _queueMutex;
    
    // 配置参数
    int _connectionTimeout;
    int _maxRetries;
    
    // SMTP会话状态
    bool _tlsStarted;
    bool _authenticated;
    QString _serverCapabilities;
};

#endif // SMTPCLIENT_H
