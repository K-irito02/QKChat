#include "mainwindow.h"
#include "src/ServerManager.h"
#include "src/utils/Logger.h"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QDir>

#include <QNetworkAccessManager>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QFileInfo>

#include <signal.h>  // 添加信号处理支持

// 全局异常处理函数
void globalExceptionHandler()
{
    LOG_CRITICAL("Unhandled exception caught in global handler");
    
    // 尝试优雅地关闭应用程序
    if (QApplication::instance()) {
        QApplication::quit();
    }
}

// 信号处理函数（用于捕获崩溃信号）
void signalHandler(int signal)
{
    LOG_CRITICAL(QString("Received signal %1, shutting down gracefully").arg(signal));
    
    // 记录崩溃信息
    QString crashInfo = QString("Server crashed with signal: %1").arg(signal);
    LOG_CRITICAL(crashInfo);
    
    // 尝试优雅地关闭应用程序
    if (QApplication::instance()) {
        QApplication::quit();
    }
}

// 自定义消息处理器
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logMessage;
    
    switch (type) {
    case QtDebugMsg:
        logMessage = QString("[%1][DEBUG]: %2").arg(timestamp).arg(msg);
        break;
    case QtInfoMsg:
        logMessage = QString("[%1][INFO]: %2").arg(timestamp).arg(msg);
        break;
    case QtWarningMsg:
        logMessage = QString("[%1][WARNING]: %2").arg(timestamp).arg(msg);
        break;
    case QtCriticalMsg:
        logMessage = QString("[%1][CRITICAL]: %2").arg(timestamp).arg(msg);
        break;
    case QtFatalMsg:
        logMessage = QString("[%1][FATAL]: %2").arg(timestamp).arg(msg);
        LOG_CRITICAL("Fatal error detected, application will terminate");
        break;
    }
    
    // 输出到控制台
    fprintf(stderr, "%s\n", qPrintable(logMessage));
    
    // 如果是致命错误，记录详细信息
    if (type == QtFatalMsg) {
        LOG_CRITICAL(QString("Fatal error context - File: %1, Line: %2, Function: %3")
                     .arg(context.file).arg(context.line).arg(context.function));
    }
}

int main(int argc, char *argv[])
{
    // 设置全局异常处理
    std::set_terminate(globalExceptionHandler);
    
    // 设置信号处理器
    signal(SIGSEGV, signalHandler);  // 段错误
    signal(SIGABRT, signalHandler);  // 中止信号
    signal(SIGFPE, signalHandler);   // 浮点异常
    signal(SIGILL, signalHandler);   // 非法指令
    signal(SIGTERM, signalHandler);  // 终止信号
    
    // 设置自定义消息处理器
    qInstallMessageHandler(customMessageHandler);
    
    QApplication a(argc, argv);

    // 设置应用程序信息
    a.setApplicationName("QKChat Server");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("QKChat");
    a.setOrganizationDomain("qkchat.com");

    // 注意：日志系统将在ServerManager::initialize()中初始化
    // 这里不再重复初始化，避免潜在的多线程问题

    // 初始化服务器管理器
    ServerManager* serverManager = ServerManager::instance();

    try {
        if (!serverManager->initialize()) {
            LOG_ERROR("Server initialization failed");
            QMessageBox::critical(nullptr, "启动失败", "服务器初始化失败，请检查配置和依赖服务。");
            return -1;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Server initialization exception: %1").arg(e.what()));
        QMessageBox::critical(nullptr, "启动失败", QString("服务器初始化异常: %1").arg(e.what()));
        return -1;
    } catch (...) {
        LOG_ERROR("Unknown exception during server initialization");
        QMessageBox::critical(nullptr, "启动失败", "服务器初始化时发生未知异常");
        return -1;
    }

    // 启动服务器（使用配置文件中的端口）
    if (!serverManager->startServer(0)) {
        LOG_ERROR("Failed to start TCP server");
        QMessageBox::critical(nullptr, "启动失败", "无法启动TCP服务器，请检查端口是否被占用。");
        return -1;
    }

    // 显示主窗口
    MainWindow w;
    w.show();
    
    // 连接服务器管理器信号到主窗口（如果需要）
    QObject::connect(serverManager, &ServerManager::serverStateChanged, [&w](ServerManager::ServerState state) {
        // 可以在这里更新主窗口状态
    });
    
    QObject::connect(serverManager, &ServerManager::clientConnected, [](int count) {
        LOG_INFO(QString("Client connected, total: %1").arg(count));
    });
    
    QObject::connect(serverManager, &ServerManager::userLoggedIn, [](qint64 userId, const QString &username) {
        LOG_INFO(QString("User logged in: %1 (ID: %2)").arg(username).arg(userId));
    });

    // 添加应用程序退出处理
    QObject::connect(&a, &QApplication::aboutToQuit, [serverManager]() {
        LOG_INFO("Application about to quit, cleaning up...");
        if (serverManager) {
            serverManager->stopServer();
        }
        LOG_INFO("Cleanup completed");
    });

    return a.exec();
}
