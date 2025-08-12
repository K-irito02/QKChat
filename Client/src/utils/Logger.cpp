#include "Logger.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QStringConverter>
#include <iostream>

// 静态成员初始化
bool Logger::s_initialized = false;
QString Logger::s_logDir;
QString Logger::s_moduleName;
Logger::LogLevel Logger::s_logLevel = Logger::INFO;
bool Logger::s_consoleOutput = true;
QMutex Logger::s_mutex;
QFile* Logger::s_logFile = nullptr;
QTextStream* Logger::s_logStream = nullptr;

bool Logger::initialize(const QString &logDir, const QString &moduleName)
{
    QMutexLocker locker(&s_mutex);

    if (s_initialized) {
        return true;
    }

    s_logDir = logDir;
    s_moduleName = moduleName;

    // 确保日志目录存在
    ensureLogDirectory();

    // 清空现有日志文件
    clearLogFiles();

    // 创建控制台日志文件
    QString consoleLogPath = QDir(s_logDir).absoluteFilePath("控制台.log");
    s_logFile = new QFile(consoleLogPath);

    if (!s_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete s_logFile;
        s_logFile = nullptr;
        return false;
    }

    s_logStream = new QTextStream(s_logFile);
    s_logStream->setEncoding(QStringConverter::Utf8);

    s_initialized = true;

    // 记录初始化日志（避免死锁，直接输出到控制台）
    QString initMsg = QString("Logger initialized for module: %1, log file: %2").arg(s_moduleName).arg(consoleLogPath);

    return true;
}

void Logger::shutdown()
{
    QMutexLocker locker(&s_mutex);
    
    if (!s_initialized) {
        return;
    }
    
    info("Logger shutting down");
    
    if (s_logStream) {
        s_logStream->flush();
        delete s_logStream;
        s_logStream = nullptr;
    }
    
    if (s_logFile) {
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }
    
    s_initialized = false;
}



void Logger::info(const QString &message, const QString &function, int line)
{
    writeLog(INFO, message, function, line);
}

void Logger::warning(const QString &message, const QString &function, int line)
{
    writeLog(WARNING, message, function, line);
}

void Logger::error(const QString &message, const QString &function, int line)
{
    writeLog(ERROR, message, function, line);
}

void Logger::critical(const QString &message, const QString &function, int line)
{
    writeLog(CRITICAL, message, function, line);
}

void Logger::logAuth(const QString &operation, const QString &username, 
                    bool success, const QString &message)
{
    QString logMsg = QString("AUTH [%1] User: %2, Success: %3")
                    .arg(operation)
                    .arg(username)
                    .arg(success ? "YES" : "NO");
    
    if (!message.isEmpty()) {
        logMsg += QString(", Details: %1").arg(message);
    }
    
    if (success) {
        info(logMsg);
    } else {
        warning(logMsg);
    }
}

void Logger::logNetwork(const QString &operation, const QString &endpoint, 
                       bool success, const QString &message)
{
    QString logMsg = QString("NETWORK [%1] Endpoint: %2, Success: %3")
                    .arg(operation)
                    .arg(endpoint)
                    .arg(success ? "YES" : "NO");
    
    if (!message.isEmpty()) {
        logMsg += QString(", Details: %1").arg(message);
    }
    
    if (success) {
        info(logMsg);
    } else {
        error(logMsg);
    }
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&s_mutex);
    s_logLevel = level;
}

void Logger::setConsoleOutput(bool enabled)
{
    QMutexLocker locker(&s_mutex);
    s_consoleOutput = enabled;
}

void Logger::clearLogFiles()
{
    QDir logDir(s_logDir);
    if (!logDir.exists()) {
        logDir.mkpath(s_logDir);
        return;
    }
    
    // 删除所有日志文件，为新的运行做准备
    QStringList filters;
    filters << "*.log";
    QFileInfoList files = logDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo &fileInfo : files) {
        QString filePath = fileInfo.absoluteFilePath();
        QFile::remove(filePath);
    }
    
    // 注意：这里不能调用LOG_INFO，因为initialize()已经持有锁
    // 会导致嵌套锁死锁问题
}

QString Logger::levelToString(LogLevel level)
{
    switch (level) {

        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        case CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void Logger::writeLog(LogLevel level, const QString &message, 
                     const QString &function, int line)
{
    QMutexLocker locker(&s_mutex);
    
    if (!s_initialized || level < s_logLevel) {
        return;
    }
    
    QString formattedMessage = formatLogMessage(level, message, function, line);
    
    // 输出到文件
    if (s_logStream) {
        *s_logStream << formattedMessage << Qt::endl;
        s_logStream->flush();
    }
    
    // 输出到控制台
    if (s_consoleOutput) {
        std::cout << formattedMessage.toStdString() << std::endl;
    }
}

QString Logger::formatLogMessage(LogLevel level, const QString &message, 
                                const QString &function, int line)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    
    QString formatted = QString("[%1][%2]: %3")
                       .arg(timestamp)
                       .arg(levelStr)
                       .arg(message);
    
    if (!function.isEmpty() && line > 0) {
        formatted += QString(" [%1:%2]").arg(function).arg(line);
    }
    
    return formatted;
}

void Logger::ensureLogDirectory()
{
    QDir dir;
    if (!dir.exists(s_logDir)) {
        dir.mkpath(s_logDir);
    }
}

QString Logger::getCurrentLogFilePath()
{
    // 直接返回控制台.log文件路径
    return QDir(s_logDir).absoluteFilePath("控制台.log");
}
