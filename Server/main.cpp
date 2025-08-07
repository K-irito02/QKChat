#include "mainwindow.h"
#include "src/ServerManager.h"

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序信息
    a.setApplicationName("QKChat Server");
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName("QKChat");
    a.setOrganizationDomain("qkchat.com");

    // 初始化服务器管理器
    ServerManager* serverManager = ServerManager::instance();

    try {
        if (!serverManager->initialize()) {
            QMessageBox::critical(nullptr, "启动失败", "服务器初始化失败，请检查配置和依赖服务。");
            return -1;
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "启动失败", QString("服务器初始化异常: %1").arg(e.what()));
        return -1;
    } catch (...) {
        QMessageBox::critical(nullptr, "启动失败", "服务器初始化时发生未知异常");
        return -1;
    }

    // 启动服务器（使用配置文件中的端口）
    if (!serverManager->startServer(0)) {
        QMessageBox::critical(nullptr, "启动失败", "无法启动TCP服务器，请检查端口是否被占用。");
        return -1;
    }

    // 显示主窗口
    MainWindow w;
    w.show();
    
    // 连接服务器管理器信号到主窗口（如果需要）
    QObject::connect(serverManager, &ServerManager::serverStateChanged, [&w](ServerManager::ServerState state) {
        // 状态变化处理
    });

    QObject::connect(serverManager, &ServerManager::clientConnected, [](int count) {
        // 客户端连接处理
    });

    QObject::connect(serverManager, &ServerManager::userLoggedIn, [](qint64 userId, const QString &username) {
        // 用户登录处理
    });

    int result = a.exec();

    // 清理资源
    serverManager->stopServer();

    return result;
}
