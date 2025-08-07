#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QMutex>
#include <QRecursiveMutex>

/**
 * @brief 数据库管理器类（服务器端）
 * 
 * 负责管理与MySQL数据库的连接，提供数据库操作的基础功能。
 * 支持连接池、事务处理、错误处理等高级功能。
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 数据库管理器实例
     */
    static DatabaseManager* instance();
    
    /**
     * @brief 初始化数据库连接
     * @param host 数据库主机
     * @param port 数据库端口
     * @param database 数据库名称
     * @param username 用户名
     * @param password 密码
     * @return 初始化是否成功
     */
    bool initialize(const QString &host = "localhost", 
                   int port = 3306,
                   const QString &database = "qkchat",
                   const QString &username = "root",
                   const QString &password = "");
    
    /**
     * @brief 关闭数据库连接
     */
    void close();
    
    /**
     * @brief 检查数据库连接是否有效
     * @return 连接是否有效
     */
    bool isConnected() const;
    
    /**
     * @brief 执行SQL查询
     * @param sql SQL语句
     * @param params 参数列表
     * @return 查询结果
     */
    QSqlQuery executeQuery(const QString &sql, const QVariantList &params = QVariantList());
    
    /**
     * @brief 执行SQL更新操作
     * @param sql SQL语句
     * @param params 参数列表
     * @return 影响的行数，-1表示失败
     */
    int executeUpdate(const QString &sql, const QVariantList &params = QVariantList());
    
    /**
     * @brief 开始事务
     * @return 是否成功
     */
    bool beginTransaction();
    
    /**
     * @brief 提交事务
     * @return 是否成功
     */
    bool commitTransaction();
    
    /**
     * @brief 回滚事务
     * @return 是否成功
     */
    bool rollbackTransaction();
    
    /**
     * @brief 获取最后插入的ID
     * @return 最后插入的ID
     */
    qint64 lastInsertId() const;
    
    /**
     * @brief 获取最后的错误信息
     * @return 错误信息
     */
    QString lastError() const;
    
    /**
     * @brief 创建数据库表
     * @return 创建是否成功
     */
    bool createTables();
    
    /**
     * @brief 检查表是否存在
     * @param tableName 表名
     * @return 表是否存在
     */
    bool tableExists(const QString &tableName);
    
    /**
     * @brief 获取数据库版本
     * @return 数据库版本
     */
    QString getDatabaseVersion();
    
    /**
     * @brief 测试数据库连接
     * @return 连接测试是否成功
     */
    bool testConnection();
    
    /**
     * @brief 优化数据库
     * @return 优化是否成功
     */
    bool optimizeDatabase();

signals:
    /**
     * @brief 数据库连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);
    
    /**
     * @brief 数据库错误信号
     * @param error 错误信息
     */
    void databaseError(const QString &error);

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();
    
    /**
     * @brief 重新连接数据库
     * @return 重连是否成功
     */
    bool reconnect();
    
    /**
     * @brief 设置数据库连接参数
     */
    void setupConnection();
    
    /**
     * @brief 记录数据库错误
     * @param error 错误信息
     */
    void logError(const QString &error);

private:
    static DatabaseManager* s_instance;
    static QMutex s_mutex;
    
    QSqlDatabase _database;
    QString _connectionName;
    QString _host;
    int _port;
    QString _databaseName;
    QString _username;
    QString _password;
    
    bool _isConnected;
    QString _lastError;
    
    mutable QRecursiveMutex _queryMutex;
};

#endif // DATABASEMANAGER_H
