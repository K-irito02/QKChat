#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTimer>
#include <QLoggingCategory>

#include <exception>

#include "src/auth/AuthManager.h"
#include "src/auth/SessionManager.h"
#include "src/models/User.h"
#include "src/models/AuthResponse.h"
#include "src/models/FriendGroupManager.h"
#include "src/utils/Logger.h"
#include "src/DatabaseManager.h"
#include "src/chat/ChatNetworkClient.h"

int main(int argc, char *argv[])
{
    try {
        // 设置Qt Quick Controls样式环境变量（在QGuiApplication创建之前）
        qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

        // 禁用QML调试信息
        QLoggingCategory::setFilterRules("qt.qml.debug=false");

        QGuiApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("QKChat Client");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("QKChat");
    app.setOrganizationDomain("qkchat.com");

    // 禁用自动退出，手动控制应用程序生命周期
    app.setQuitOnLastWindowClosed(false);

    // 初始化日志系统
    QString logDir = "D:/QT_Learn/Projects/QKChat/Client/logs";
    QDir().mkpath(logDir); // 确保目录存在

    if (!Logger::initialize(logDir, "Client")) {
        // 尝试使用相对路径
        QString relativeLogDir = "logs";
        if (!Logger::initialize(relativeLogDir, "Client")) {
            // 继续运行，但记录警告
        }
    }

    // 异步初始化数据库，避免阻塞主线程
    DatabaseManager* dbManager = DatabaseManager::instance();
    
    // 使用QTimer延迟初始化数据库，给UI更多时间渲染
    QTimer::singleShot(100, [dbManager]() {
        try {
            if (!dbManager->initialize()) {
                LOG_ERROR("Failed to initialize database");
            } else {
            
            }
        } catch (const std::exception& e) {
            LOG_ERROR(QString("Exception during database initialization: %1").arg(e.what()));
        } catch (...) {
            LOG_ERROR("Unknown exception during database initialization");
        }
    });

    // 注册QML类型
    qmlRegisterType<User>("QKChat", 1, 0, "User");
    qmlRegisterType<AuthResponse>("QKChat", 1, 0, "AuthResponse");
    qmlRegisterType<AuthManager>("QKChat", 1, 0, "AuthManager");
    qmlRegisterType<SessionManager>("QKChat", 1, 0, "SessionManager");
    qmlRegisterType<ChatNetworkClient>("QKChat", 1, 0, "ChatNetworkClient");

    // 创建QML引擎
    QQmlApplicationEngine engine;
    
    // 添加QML导入路径
    engine.addImportPath("qrc:/");
    engine.addImportPath(QCoreApplication::applicationDirPath());
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");

    // 获取管理器实例（确保初始化顺序）
    AuthManager* authManager = nullptr;
    SessionManager* sessionManager = nullptr;
    ChatNetworkClient* chatNetworkClient = nullptr;
    FriendGroupManager* friendGroupManager = nullptr;

    try {
        // 首先创建管理器实例
        authManager = AuthManager::instance();
        sessionManager = SessionManager::instance();
        chatNetworkClient = ChatNetworkClient::instance();
        friendGroupManager = new FriendGroupManager(&app);

        // 将管理器实例暴露给QML（先暴露，后初始化）
        engine.rootContext()->setContextProperty("authManager", authManager);
        engine.rootContext()->setContextProperty("sessionManager", sessionManager);
        engine.rootContext()->setContextProperty("ChatNetworkClient", chatNetworkClient);
        engine.rootContext()->setContextProperty("FriendGroupManager", friendGroupManager);

        // 异步初始化认证管理器，避免阻塞UI
        QTimer::singleShot(50, [authManager, chatNetworkClient]() {
            if (!authManager->initialize("localhost", 8080, false)) {  // 禁用SSL
                LOG_ERROR("Failed to initialize AuthManager");
            } else {
                // 初始化ChatNetworkClient
                if (chatNetworkClient && !chatNetworkClient->initialize()) {
                    LOG_ERROR("Failed to initialize ChatNetworkClient");
                } else {
                    LOG_INFO("ChatNetworkClient initialized successfully");
                }
            }
        });

    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception while creating managers: %1").arg(e.what()));
        return -1;
    } catch (...) {
        LOG_ERROR("Unknown exception while creating managers");
        return -1;
    }

    // 连接引擎错误信号
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl &url) {
            LOG_CRITICAL(QString("QML object creation failed for URL: %1").arg(url.toString()));
            QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

    // 加载主QML文件
    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    LOG_INFO(QString("Attempting to load QML file from: %1").arg(url.toString()));

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        LOG_ERROR(QString("Failed to load QML file from: %1").arg(url.toString()));

        // 尝试列出可用的资源
        LOG_ERROR("Available QML resources:");
        QDirIterator it(":", QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString path = it.next();
            if (path.endsWith(".qml")) {
                LOG_ERROR(QString("  - %1").arg(path));
            }
        }

        return -1;
    }

    

        // 运行应用程序
        int result = app.exec();

        // 安全的资源清理
        try {
            if (authManager && authManager->isConnected()) {
                authManager->disconnectFromServer();
            }
            if (dbManager) {
                dbManager->close();
            }
        } catch (...) {
            // 忽略清理错误
        }
        return result;

    } catch (...) {
        return -1;
    }
}
