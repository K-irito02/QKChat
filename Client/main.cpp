#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>

#include "src/auth/AuthManager.h"
#include "src/auth/SessionManager.h"
#include "src/models/User.h"
#include "src/models/AuthResponse.h"
#include "src/utils/Logger.h"
#include "src/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("QKChat Client");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("QKChat");
    app.setOrganizationDomain("qkchat.com");

    // 初始化日志系统
    QString logDir = "D:/QT_Learn/Projects/QKChat/Client/logs";
    QDir().mkpath(logDir); // 确保目录存在

    if (!Logger::initialize(logDir, "Client")) {
        qWarning() << "Failed to initialize logger";
        qWarning() << "Log directory:" << logDir;
        // 尝试使用相对路径
        QString relativeLogDir = "logs";
        if (!Logger::initialize(relativeLogDir, "Client")) {
            qWarning() << "Failed to initialize logger with relative path";
            // 继续运行，但记录警告
        } else {
            qDebug() << "Logger initialized with relative path";
        }
    } else {
        qDebug() << "Logger initialized successfully";
    }

    // 初始化数据库
    DatabaseManager* dbManager = DatabaseManager::instance();
    
    try {
        if (!dbManager->initialize()) {
            LOG_ERROR("Failed to initialize database");
            return -1;
        }
        LOG_INFO("Database initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during database initialization: %1").arg(e.what()));
        return -1;
    } catch (...) {
        LOG_ERROR("Unknown exception during database initialization");
        return -1;
    }

    // 注册QML类型
    qmlRegisterType<User>("QKChat", 1, 0, "User");
    qmlRegisterType<AuthResponse>("QKChat", 1, 0, "AuthResponse");
    qmlRegisterUncreatableType<AuthManager>("QKChat", 1, 0, "AuthManager",
                                           "AuthManager is a singleton");
    qmlRegisterUncreatableType<SessionManager>("QKChat", 1, 0, "SessionManager",
                                              "SessionManager is a singleton");

    // 创建QML引擎
    QQmlApplicationEngine engine;

    // 获取管理器实例（确保初始化顺序）
    AuthManager* authManager = nullptr;
    SessionManager* sessionManager = nullptr;

    try {
        // 首先创建AuthManager实例
        authManager = AuthManager::instance();

        // 初始化认证管理器
        if (!authManager->initialize("localhost", 8080, true)) {
            LOG_ERROR("Failed to initialize AuthManager");
            return -1;
        }

        // 然后创建SessionManager实例
        sessionManager = SessionManager::instance();

        // 将管理器实例暴露给QML
        engine.rootContext()->setContextProperty("authManager", authManager);
        engine.rootContext()->setContextProperty("sessionManager", sessionManager);

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
    const QUrl url(QStringLiteral("qrc:/qt/qml/Client/Main.qml"));
    
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        LOG_ERROR("Failed to load QML file");
        return -1;
    }

    LOG_INFO("QKChat Client started successfully");

    // 运行应用程序
    int result = app.exec();

    // 清理资源
    LOG_INFO("QKChat Client shutting down");
    dbManager->close();
    Logger::shutdown();

    return result;
}
