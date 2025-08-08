#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVariantMap>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QSettings>
#include <QDebug>

/**
 * @brief 客户端SQLite数据库管理类
 * 
 * 负责客户端的本地数据存储，包括用户信息、聊天记录、设置等。
 * 使用单例模式确保全局唯一实例。
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取数据库管理器实例
     * @return 数据库管理器实例
     */
    static DatabaseManager* instance();
    
    /**
     * @brief 初始化数据库
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 关闭数据库连接
     */
    void close();
    
    // 用户管理
    /**
     * @brief 创建用户
     * @param username 用户名
     * @param email 邮箱
     * @param passwordHash 密码哈希
     * @param salt 盐值
     * @return 用户ID，失败返回-1
     */
    qint64 createUser(const QString &username, const QString &email, 
                      const QString &passwordHash, const QString &salt);
    
    /**
     * @brief 验证用户登录
     * @param username 用户名或邮箱
     * @param passwordHash 密码哈希
     * @return 用户信息JSON对象，失败返回空对象
     */
    QJsonObject authenticateUser(const QString &username, const QString &passwordHash);
    
    /**
     * @brief 更新用户信息
     * @param userId 用户ID
     * @param userData 用户数据
     * @return 更新是否成功
     */
    bool updateUser(qint64 userId, const QJsonObject &userData);
    
    /**
     * @brief 获取用户信息
     * @param userId 用户ID
     * @return 用户信息JSON对象
     */
    QJsonObject getUserInfo(qint64 userId);
    
    /**
     * @brief 更新用户状态
     * @param userId 用户ID
     * @param status 状态
     * @return 更新是否成功
     */
    bool updateUserStatus(qint64 userId, const QString &status);
    
    // 消息管理
    /**
     * @brief 保存消息
     * @param messageData 消息数据
     * @return 消息ID，失败返回-1
     */
    qint64 saveMessage(const QJsonObject &messageData);
    
    /**
     * @brief 获取聊天记录
     * @param userId 用户ID
     * @param friendId 好友ID
     * @param limit 限制数量
     * @param offset 偏移量
     * @return 消息列表
     */
    QJsonArray getChatHistory(qint64 userId, qint64 friendId, int limit = 50, int offset = 0);
    
    /**
     * @brief 标记消息为已读
     * @param messageId 消息ID
     * @param userId 用户ID
     * @return 更新是否成功
     */
    bool markMessageAsRead(qint64 messageId, qint64 userId);
    
    /**
     * @brief 删除消息
     * @param messageId 消息ID
     * @param userId 用户ID
     * @return 删除是否成功
     */
    bool deleteMessage(qint64 messageId, qint64 userId);
    
    // 好友管理
    /**
     * @brief 添加好友关系
     * @param userId 用户ID
     * @param friendId 好友ID
     * @param status 关系状态
     * @return 添加是否成功
     */
    bool addFriendship(qint64 userId, qint64 friendId, const QString &status = "pending");
    
    /**
     * @brief 更新好友关系状态
     * @param userId 用户ID
     * @param friendId 好友ID
     * @param status 新状态
     * @return 更新是否成功
     */
    bool updateFriendshipStatus(qint64 userId, qint64 friendId, const QString &status);
    
    /**
     * @brief 获取好友列表
     * @param userId 用户ID
     * @return 好友列表
     */
    QJsonArray getFriendsList(qint64 userId);
    
    /**
     * @brief 删除好友关系
     * @param userId 用户ID
     * @param friendId 好友ID
     * @return 删除是否成功
     */
    bool removeFriendship(qint64 userId, qint64 friendId);
    
    // 设置管理
    /**
     * @brief 保存设置
     * @param key 设置键
     * @param value 设置值
     * @return 保存是否成功
     */
    bool saveSetting(const QString &key, const QString &value);
    
    /**
     * @brief 获取设置
     * @param key 设置键
     * @param defaultValue 默认值
     * @return 设置值
     */
    QString getSetting(const QString &key, const QString &defaultValue = "");
    
    /**
     * @brief 删除设置
     * @param key 设置键
     * @return 删除是否成功
     */
    bool removeSetting(const QString &key);
    
    // 工具方法
    /**
     * @brief 生成密码哈希
     * @param password 原始密码
     * @param salt 盐值
     * @return 密码哈希
     */
    QString hashPassword(const QString &password, const QString &salt);
    
    /**
     * @brief 生成随机盐值
     * @return 随机盐值
     */
    QString generateSalt();
    
    /**
     * @brief 验证邮箱格式
     * @param email 邮箱地址
     * @return 是否有效
     */
    bool isValidEmail(const QString &email);

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();
    
    // 禁用拷贝构造和赋值操作
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    /**
     * @brief 创建数据库表
     * @return 创建是否成功
     */
    bool createTables();
    
    /**
     * @brief 执行SQL查询
     * @param sql SQL语句
     * @param params 参数
     * @return 查询对象
     */
    QSqlQuery executeQuery(const QString &sql, const QVariantMap &params = QVariantMap());
    
    /**
     * @brief 检查数据库连接
     * @return 连接是否有效
     */
    bool checkConnection();
    
    /**
     * @brief 记录错误日志
     * @param operation 操作名称
     * @param error 错误信息
     */
    void logError(const QString &operation, const QString &error);

private:
    static DatabaseManager* m_instance;
    static QMutex s_mutex; // 添加静态互斥锁
    QSqlDatabase m_database;
    QMutex m_mutex;
    QString m_databasePath;
    bool m_initialized;
    bool m_isConnected; // 添加连接状态成员变量
    
    // 数据库表名常量
    static const QString TABLE_USERS;
    static const QString TABLE_CHAT_MESSAGES;
    static const QString TABLE_FRIENDSHIPS;
    static const QString TABLE_SETTINGS;
};

#endif // DATABASEMANAGER_H