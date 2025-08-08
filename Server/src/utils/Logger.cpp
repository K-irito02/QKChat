#include "Logger.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMutexLocker>
#include <iostream>

// 静态成员初始化
bool Logger::s_initialized = false;
QString Logger::s_logDir;
QString Logger::s_moduleName;
Logger::LogLevel Logger::s_logLevel = Logger::INFO;
bool Logger::s_consoleOutput = true;
bool Logger::s_jsonFormat = false;
qint64 Logger::s_maxFileSize = 100 * 1024 * 1024; // 100MB
int Logger::s_retentionDays = 30;
QMutex Logger::s_mutex;
QFile* Logger::s_logFile = nullptr;
QTextStream* Logger::s_logStream = nullptr;
QMap<QString, QFile*> Logger::s_moduleLogFiles;
QMap<QString, QTextStream*> Logger::s_moduleLogStreams;

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
    
    // 创建日志文件
    QString logFilePath = getCurrentLogFilePath();
    s_logFile = new QFile(logFilePath);
    
    if (!s_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        delete s_logFile;
        s_logFile = nullptr;
        return false;
    }
    
    s_logStream = new QTextStream(s_logFile);
    s_logStream->setEncoding(QStringConverter::Utf8);
    
    s_initialized = true;
    
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
                    bool success, const QString &ipAddress, const QString &message)
{
    QString logMsg = QString("AUTH [%1] User: %2, Success: %3")
                    .arg(operation)
                    .arg(username)
                    .arg(success ? "YES" : "NO");
    
    if (!ipAddress.isEmpty()) {
        logMsg += QString(", IP: %1").arg(ipAddress);
    }
    
    if (!message.isEmpty()) {
        logMsg += QString(", Details: %1").arg(message);
    }
    
    if (success) {
        info(logMsg);
    } else {
        warning(logMsg);
    }
}

void Logger::logDatabase(const QString &operation, const QString &table, 
                        bool success, const QString &message)
{
    QString logMsg = QString("DATABASE [%1] Table: %2, Success: %3")
                    .arg(operation)
                    .arg(table)
                    .arg(success ? "YES" : "NO");
    
    if (!message.isEmpty()) {
        logMsg += QString(", Details: %1").arg(message);
    }
    
    if (success) {
    
    } else {
        error(logMsg);
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

    // 检查并轮转日志文件
    checkAndRotateLog();

    if (s_jsonFormat) {
        writeJsonLog(level, message, function, line);
    } else {
        QString formattedMessage = formatLogMessage(level, message, function, line);

        // 输出到文件
        if (s_logStream) {
            *s_logStream << formattedMessage << Qt::endl;
            s_logStream->flush();
        }

        // 输出到控制台
        if (s_consoleOutput) {
            // 控制台输出已禁用，避免调试信息干扰
        }
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
    QString fileName = QString("%1_%2.log")
                      .arg(s_moduleName)
                      .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));

    return QDir(s_logDir).absoluteFilePath(fileName);
}

QString Logger::getModuleLogFilePath(const QString &moduleName)
{
    QString fileName = QString("%1_%2.log")
                      .arg(moduleName)
                      .arg(QDateTime::currentDateTime().toString("yyyyMMdd"));

    return QDir(s_logDir).absoluteFilePath(fileName);
}

void Logger::setMaxFileSize(qint64 maxSize)
{
    QMutexLocker locker(&s_mutex);
    s_maxFileSize = maxSize;
}

void Logger::setRetentionDays(int days)
{
    QMutexLocker locker(&s_mutex);
    s_retentionDays = days;
}

void Logger::setJsonFormat(bool enabled)
{
    QMutexLocker locker(&s_mutex);
    s_jsonFormat = enabled;
}

bool Logger::createModuleLogger(const QString &moduleName)
{
    QMutexLocker locker(&s_mutex);

    if (s_moduleLogFiles.contains(moduleName)) {
        return true; // 已存在
    }

    QString logFilePath = getModuleLogFilePath(moduleName);
    QFile* logFile = new QFile(logFilePath);

    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        delete logFile;
        return false;
    }

    QTextStream* logStream = new QTextStream(logFile);
    logStream->setEncoding(QStringConverter::Utf8);

    s_moduleLogFiles[moduleName] = logFile;
    s_moduleLogStreams[moduleName] = logStream;

    return true;
}

void Logger::logPerformance(const QString &operation, qint64 duration, const QString &details)
{
    QString message = QString("PERFORMANCE [%1] Duration: %2ms")
                     .arg(operation)
                     .arg(duration);

    if (!details.isEmpty()) {
        message += QString(" Details: %1").arg(details);
    }

    info(message, "Performance", 0);
}

void Logger::logMetric(const QString &metric, double value, const QString &unit)
{
    QString message = QString("METRIC [%1] Value: %2")
                     .arg(metric)
                     .arg(value);

    if (!unit.isEmpty()) {
        message += QString(" %1").arg(unit);
    }

    info(message, "Metrics", 0);
}

void Logger::triggerAlert(LogLevel level, const QString &message, const QString &source)
{
    QString alertMessage = QString("ALERT [%1] %2")
                          .arg(source.isEmpty() ? "System" : source)
                          .arg(message);

    writeLog(level, alertMessage, "Alert", 0);

    // 这里可以添加额外的告警处理逻辑，如发送邮件、推送通知等
}

void Logger::rotateLogFiles()
{
    QMutexLocker locker(&s_mutex);

    // 轮转主日志文件
    if (s_logFile && s_logFile->size() > s_maxFileSize) {
        s_logStream->flush();
        s_logFile->close();

        QString currentPath = s_logFile->fileName();
        QString backupPath = currentPath + "." + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

        QFile::rename(currentPath, backupPath);

        // 重新打开日志文件
        if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            info("Log file rotated");
        }
    }

    // 轮转模块日志文件
    for (auto it = s_moduleLogFiles.begin(); it != s_moduleLogFiles.end(); ++it) {
        QFile* file = it.value();
        if (file && file->size() > s_maxFileSize) {
            QTextStream* stream = s_moduleLogStreams[it.key()];
            stream->flush();
            file->close();

            QString currentPath = file->fileName();
            QString backupPath = currentPath + "." + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

            QFile::rename(currentPath, backupPath);

            // 重新打开文件
            file->open(QIODevice::WriteOnly | QIODevice::Append);
        }
    }
}

void Logger::cleanupOldLogs()
{
    if (s_retentionDays <= 0) {
        return;
    }

    QDir logDir(s_logDir);
    if (!logDir.exists()) {
        return;
    }

    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-s_retentionDays);

    QStringList filters;
    filters << "*.log" << "*.log.*";
    QFileInfoList files = logDir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fileInfo : files) {
        if (fileInfo.lastModified() < cutoffDate) {
            QFile::remove(fileInfo.absoluteFilePath());
        }
    }
}

void Logger::checkAndRotateLog()
{
    if (s_maxFileSize > 0 && s_logFile && s_logFile->size() > s_maxFileSize) {
        rotateLogFiles();
    }
}

void Logger::writeJsonLog(LogLevel level, const QString &message,
                         const QString &function, int line, const QString &moduleName)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = levelToString(level);
    logEntry["message"] = message;
    logEntry["module"] = moduleName.isEmpty() ? s_moduleName : moduleName;

    if (!function.isEmpty()) {
        logEntry["function"] = function;
    }

    if (line > 0) {
        logEntry["line"] = line;
    }

    logEntry["thread_id"] = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    logEntry["process_id"] = QCoreApplication::applicationPid();

    QJsonDocument doc(logEntry);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    // 输出到文件
    if (s_logStream) {
        *s_logStream << jsonString << Qt::endl;
        s_logStream->flush();
    }

    // 输出到控制台
    if (s_consoleOutput) {
        // 控制台输出已禁用，避免调试信息干扰
    }
}
