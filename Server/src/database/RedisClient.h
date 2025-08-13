#ifndef REDISCLIENT_H
#define REDISCLIENT_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QTimer>
#include <QMutex>
#include <QTcpSocket>
#include <QQueue>

/**
 * @brief Redis客户端类
 * 
 * 提供与Redis服务器的连接和基本操作功能。
 * 支持字符串操作、过期时间设置、连接池等功能。
 * 主要用于存储验证码、会话令牌和临时数据。
 */
class RedisClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Redis操作结果枚举
     */
    enum Result {
        Success,
        Error,
        NotFound,
        Timeout,
        ConnectionError
    };
    Q_ENUM(Result)

    explicit RedisClient(QObject *parent = nullptr);
    ~RedisClient();
    
    /**
     * @brief 获取单例实例
     * @return Redis客户端实例
     */
    static RedisClient* instance();
    
    /**
     * @brief 初始化Redis连接
     * @param host Redis服务器地址
     * @param port Redis端口
     * @param password 密码（可选）
     * @param database 数据库编号
     * @return 初始化是否成功
     */
    bool initialize(const QString &host = "localhost", int port = 6379, 
                   const QString &password = "", int database = 0);
    
    /**
     * @brief 关闭Redis连接
     */
    void close();
    
    /**
     * @brief 检查连接是否有效
     * @return 连接是否有效
     */
    bool isConnected() const;
    
    /**
     * @brief 设置字符串值
     * @param key 键
     * @param value 值
     * @param expireSeconds 过期时间（秒），0表示不过期
     * @return 操作结果
     */
    Result set(const QString &key, const QString &value, int expireSeconds = 0);
    
    /**
     * @brief 获取字符串值
     * @param key 键
     * @param value 输出值
     * @return 操作结果
     */
    Result get(const QString &key, QString &value);
    
    /**
     * @brief 删除键
     * @param key 键
     * @return 操作结果
     */
    Result del(const QString &key);
    
    /**
     * @brief 检查键是否存在
     * @param key 键
     * @return 是否存在
     */
    bool exists(const QString &key);
    
    /**
     * @brief 设置键的过期时间
     * @param key 键
     * @param expireSeconds 过期时间（秒）
     * @return 操作结果
     */
    Result expire(const QString &key, int expireSeconds);
    
    /**
     * @brief 获取键的剩余生存时间
     * @param key 键
     * @return 剩余秒数，-1表示永不过期，-2表示键不存在
     */
    int ttl(const QString &key);
    
    /**
     * @brief 递增数值
     * @param key 键
     * @param increment 递增值，默认1
     * @return 递增后的值，-1表示失败
     */
    qint64 incr(const QString &key, qint64 increment = 1);
    
    /**
     * @brief 递减数值
     * @param key 键
     * @param decrement 递减值，默认1
     * @return 递减后的值，-1表示失败
     */
    qint64 decr(const QString &key, qint64 decrement = 1);
    
    /**
     * @brief 测试连接
     * @return 连接是否正常
     */
    bool ping();
    
    /**
     * @brief 获取Redis信息
     * @return 服务器信息
     */
    QString info();
    
    /**
     * @brief 清空当前数据库
     * @return 操作结果
     */
    Result flushdb();
    
    /**
     * @brief 设置验证码
     * @param email 邮箱地址
     * @param code 验证码
     * @param expireMinutes 过期时间（分钟）
     * @return 操作结果
     */
    Result setVerificationCode(const QString &email, const QString &code, int expireMinutes = 5);
    
    /**
     * @brief 获取验证码
     * @param email 邮箱地址
     * @param code 输出验证码
     * @return 操作结果
     */
    Result getVerificationCode(const QString &email, QString &code);
    
    /**
     * @brief 删除验证码
     * @param email 邮箱地址
     * @return 操作结果
     */
    Result deleteVerificationCode(const QString &email);
    
    /**
     * @brief 设置会话令牌
     * @param userId 用户ID
     * @param token 会话令牌
     * @param expireHours 过期时间（小时）
     * @return 操作结果
     */
    Result setSessionToken(qint64 userId, const QString &token, int expireHours = 24);
    
    /**
     * @brief 获取会话令牌
     * @param userId 用户ID
     * @param token 输出会话令牌
     * @return 操作结果
     */
    Result getSessionToken(qint64 userId, QString &token);
    
    /**
     * @brief 删除会话令牌
     * @param userId 用户ID
     * @return 操作结果
     */
    Result deleteSessionToken(qint64 userId);
    
    /**
     * @brief 获取匹配模式的键列表
     * @param pattern 匹配模式，如 "session:*"
     * @return 键列表
     */
    QStringList keys(const QString &pattern);

signals:
    /**
     * @brief 连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);
    
    /**
     * @brief Redis错误信号
     * @param error 错误信息
     */
    void redisError(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimer();

private:
    /**
     * @brief 重新连接Redis
     * @return 重连是否成功
     */
    bool reconnect();
    
    /**
     * @brief 发送Redis命令
     * @param command 命令
     * @param args 参数列表
     * @return 操作结果
     */
    Result sendCommand(const QString &command, const QStringList &args = QStringList());
    
    /**
     * @brief 读取Redis响应
     * @param response 输出响应
     * @param timeout 超时时间（毫秒）
     * @return 操作结果
     */
    Result readResponse(QString &response, int timeout = 5000);
    
    /**
     * @brief 解析Redis响应
     * @param data 响应数据
     * @param result 输出结果
     * @return 解析是否成功
     */
    bool parseResponse(const QByteArray &data, QString &result);
    
    /**
     * @brief 格式化Redis命令
     * @param command 命令
     * @param args 参数列表
     * @return 格式化后的命令
     */
    QByteArray formatCommand(const QString &command, const QStringList &args);
    
    /**
     * @brief 记录错误
     * @param error 错误信息
     */
    void logError(const QString &error);

private:
    static RedisClient* s_instance;
    static QMutex s_mutex;
    
    QTcpSocket* _socket;
    QTimer* _reconnectTimer;
    
    QString _host;
    int _port;
    QString _password;
    int _database;
    bool _isConnected;
    
    QString _lastError;
    mutable QMutex _commandMutex;
    
    // 连接配置
    int _connectTimeout;
    int _commandTimeout;
    int _reconnectInterval;
};

#endif // REDISCLIENT_H
