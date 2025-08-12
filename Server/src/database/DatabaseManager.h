#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QMutex>
#include <QRecursiveMutex>
#include "DatabaseConnectionPool.h"

/**
 * @brief 数据库管理器类（服务器端）
 * 
 * 负责管理与MySQL数据库的连接，提供数据库操作的基础功能。
 * 现在使用连接池提供高性能的并发数据库访问。
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
     * @brief 初始化数据库连接池
     * @param host 数据库主机
     * @param port 数据库端口
     * @param database 数据库名称
     * @param username 用户名
     * @param password 密码
     * @param minConnections 最小连接数
     * @param maxConnections 最大连接数
     * @return 初始化是否成功
     */
    bool initialize(const QString &host = "localhost", 
                   int port = 3306,
                   const QString &database = "qkchat",
                   const QString &username = "root",
                   const QString &password = "",
                   int minConnections = 5,
                   int maxConnections = 20);
    
    /**
     * @brief 关闭数据库连接池
     */
    void close();
    
    /**
     * @brief 检查数据库连接是否有效
     * @return 连接是否有效
     */
    bool isConnected() const;
    
    /**
     * @brief 执行SQL查询（使用连接池）
     * @param sql SQL语句
     * @param params 参数列表
     * @return 查询结果
     */
    QSqlQuery executeQuery(const QString &sql, const QVariantList &params = QVariantList());
    
    /**
     * @brief 执行SQL更新操作（使用连接池）
     * @param sql SQL语句
     * @param params 参数列表
     * @return 影响的行数，-1表示失败
     */
    int executeUpdate(const QString &sql, const QVariantList &params = QVariantList());
    
    /**
     * @brief 执行事务操作
     * @param operations 事务操作函数
     * @return 事务是否成功
     */
    bool executeTransaction(std::function<bool(DatabaseConnection&)> operations);
    
    /**
     * @brief 获取最后插入的ID
     * @param connection 数据库连接
     * @return 最后插入的ID
     */
    qint64 lastInsertId(const QSqlDatabase& connection) const;
    
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
     * @brief 获取数据库连接
     * @return 数据库连接
     */
    QSqlDatabase getConnection();
    
    /**
     * @brief 优化数据库
     * @return 优化是否成功
     */
    bool optimizeDatabase();
    
    /**
     * @brief 获取连接池统计信息
     * @return 统计信息JSON对象
     */
    QJsonObject getConnectionPoolStatistics() const;

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
     * @brief 记录数据库错误
     * @param error 错误信息
     */
    void logError(const QString &error);

private:
    static DatabaseManager* s_instance;
    static QMutex s_mutex;
    
    DatabaseConnectionPool* _connectionPool;
    
    bool _isConnected;
    QString _lastError;
    
    mutable QMutex _errorMutex;
};

#endif // DATABASEMANAGER_H