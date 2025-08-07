#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <QMutex>
#include <QDebug>

/**
 * @brief 日志记录工具类
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
        DEBUG = 0,
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
     * @brief 记录调试信息
     * @param message 日志消息
     * @param function 函数名
     * @param line 行号
     */
    static void debug(const QString &message, const QString &function = "", int line = 0);
    
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
     * @param message 详细信息
     */
    static void logAuth(const QString &operation, const QString &username, 
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

private:
    static bool s_initialized;
    static QString s_logDir;
    static QString s_moduleName;
    static LogLevel s_logLevel;
    static bool s_consoleOutput;
    static QMutex s_mutex;
    static QFile* s_logFile;
    static QTextStream* s_logStream;
};

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::debug(msg, __FUNCTION__, __LINE__)
#define LOG_INFO(msg) Logger::info(msg, __FUNCTION__, __LINE__)
#define LOG_WARNING(msg) Logger::warning(msg, __FUNCTION__, __LINE__)
#define LOG_ERROR(msg) Logger::error(msg, __FUNCTION__, __LINE__)
#define LOG_CRITICAL(msg) Logger::critical(msg, __FUNCTION__, __LINE__)

#endif // LOGGER_H
