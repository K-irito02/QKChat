#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <QMutex>

#include <QJsonObject>
#include <QJsonDocument>
#include <QThread>
#include <QCoreApplication>

/**
 * @brief 日志记录工具类（服务器端）
 * 
 * 提供统一的日志记录功能，支持不同级别的日志输出到文件和控制台。
 * 按照项目规范实现日志格式和文件管理。
 */
class Logger
{
public:
    /**
     * @brief 日志级别枚举
     */
    enum LogLevel {
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };
    
    /**
     * @brief 初始化日志系统
     * @param logDir 日志目录路径
     * @param moduleName 模块名称
     * @return 初始化是否成功
     */
    static bool initialize(const QString &logDir, const QString &moduleName);
    
    /**
     * @brief 关闭日志系统
     */
    static void shutdown();
    


    
    /**
     * @brief 记录一般信息
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     */
    static void info(const QString &message, const QString &function = "", int line = 0);
    
    /**
     * @brief 记录警告信息
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     */
    static void warning(const QString &message, const QString &function = "", int line = 0);
    
    /**
     * @brief 记录错误信息
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     */
    static void error(const QString &message, const QString &function = "", int line = 0);
    
    /**
     * @brief 记录严重错误信息
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     */
    static void critical(const QString &message, const QString &function = "", int line = 0);
    
    /**
     * @brief 记录认证相关日志
     * @param operation 操作类型
     * @param username 用户名
     * @param success 是否成功
     * @param ipAddress IP地址
     * @param message 详细信息
     */
    static void logAuth(const QString &operation, const QString &username, 
                       bool success, const QString &ipAddress = "", const QString &message = "");
    
    /**
     * @brief 记录数据库相关日志
     * @param operation 操作类型
     * @param table 表名
     * @param success 是否成功
     * @param message 详细信息
     */
    static void logDatabase(const QString &operation, const QString &table, 
                           bool success, const QString &message = "");
    
    /**
     * @brief 记录网络相关日志
     * @param operation 操作类型
     * @param endpoint 端点
     * @param success 是否成功
     * @param message 详细信息
     */
    static void logNetwork(const QString &operation, const QString &endpoint, 
                          bool success, const QString &message = "");
    
    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    static void setLogLevel(LogLevel level);
    
    /**
     * @brief 设置是否输出到控制台
     * @param enabled 是否启用
     */
    static void setConsoleOutput(bool enabled);
    
    /**
     * @brief 清空日志文件
     */
    static void clearLogFiles();

    /**
     * @brief 获取日志级别字符串
     * @param level 日志级别
     * @return 级别字符串
     */
    static QString levelToString(LogLevel level);

    /**
     * @brief 设置日志文件最大大小（字节）
     * @param maxSize 最大大小，0表示不限制
     */
    static void setMaxFileSize(qint64 maxSize);

    /**
     * @brief 设置日志文件保留天数
     * @param days 保留天数
     */
    static void setRetentionDays(int days);

    /**
     * @brief 启用/禁用JSON格式日志
     * @param enabled 是否启用
     */
    static void setJsonFormat(bool enabled);

    /**
     * @brief 创建模块专用日志记录器
     * @param moduleName 模块名称
     * @return 是否成功
     */
    static bool createModuleLogger(const QString &moduleName);

    /**
     * @brief 记录性能日志
     * @param operation 操作名称
     * @param duration 耗时（毫秒）
     * @param details 详细信息
     */
    static void logPerformance(const QString &operation, qint64 duration, const QString &details = "");

    /**
     * @brief 记录系统监控信息
     * @param metric 指标名称
     * @param value 指标值
     * @param unit 单位
     */
    static void logMetric(const QString &metric, double value, const QString &unit = "");

    /**
     * @brief 触发告警
     * @param level 告警级别
     * @param message 告警消息
     * @param source 告警源
     */
    static void triggerAlert(LogLevel level, const QString &message, const QString &source = "");

    /**
     * @brief 轮转日志文件
     */
    static void rotateLogFiles();

    /**
     * @brief 清理过期日志文件
     */
    static void cleanupOldLogs();

private:
    // 私有构造函数，防止实例化
    Logger() = default;
    
    /**
     * @brief 写入日志
     * @param level 日志级别
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     */
    static void writeLog(LogLevel level, const QString &message, 
                        const QString &function = "", int line = 0);
    
    /**
     * @brief 格式化日志消息
     * @param level 日志级别
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     * @return 格式化后的消息
     */
    static QString formatLogMessage(LogLevel level, const QString &message, 
                                   const QString &function, int line);
    
    /**
     * @brief 确保日志目录存在
     */
    static void ensureLogDirectory();
    
    /**
     * @brief 获取当前日志文件路径
     * @return 日志文件路径
     */
    static QString getCurrentLogFilePath();

    /**
     * @brief 获取模块日志文件路径
     * @param moduleName 模块名称
     * @return 日志文件路径
     */
    static QString getModuleLogFilePath(const QString &moduleName);

    /**
     * @brief 检查并轮转日志文件
     */
    static void checkAndRotateLog();

    /**
     * @brief 写入JSON格式日志
     * @param level 日志级别
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     * @param moduleName 模块名称
     */
    static void writeJsonLog(LogLevel level, const QString &message,
                            const QString &function, int line, const QString &moduleName = "");

private:
    static bool s_initialized;
    static QString s_logDir;
    static QString s_moduleName;
    static LogLevel s_logLevel;
    static bool s_consoleOutput;
    static bool s_jsonFormat;
    static qint64 s_maxFileSize;
    static int s_retentionDays;
    static QMutex s_mutex;
    static QFile* s_logFile;
    static QTextStream* s_logStream;
    static QMap<QString, QFile*> s_moduleLogFiles;
    static QMap<QString, QTextStream*> s_moduleLogStreams;
};

// 便捷宏定义


#define LOG_INFO(msg) Logger::info(msg, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) Logger::warning(msg, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) Logger::error(msg, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) Logger::critical(msg, __FUNCTION__, __LINE__)
#define LOG_DEBUG(msg) Logger::info(msg, __FUNCTION__, __LINE__)  // 服务器端使用INFO级别作为DEBUG

#endif // LOGGER_H
