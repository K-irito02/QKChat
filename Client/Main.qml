import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "qml/components" as Components

/**
 * @brief 主应用程序窗口管理器
 *
 * 负责管理登录窗口和聊天窗口的创建、切换和销毁
 */
ApplicationWindow {
    id: mainWindow
    width: 1
    height: 1
    visible: false  // 主窗口不可见，只作为管理器
    title: qsTr("QKChat Manager")

    // 窗口管理
    property var loginWindow: null
    property var chatWindow: null
    property bool appInitialized: false



    // 创建登录窗口
    function createLoginWindow() {
        if (loginWindow) {
            loginWindow.destroy()
        }

        var component = Qt.createComponent("qml/windows/LoginWindow.qml")
        if (component.status === Component.Ready) {
            loginWindow = component.createObject(null, {
                "themeManager": themeManager,
                "authManager": authManager,
                "sessionManager": sessionManager,
                "settingsManager": settingsManager
            })

            if (loginWindow) {
                // 连接信号
                loginWindow.loginSucceeded.connect(onLoginSucceeded)
                loginWindow.windowClosed.connect(onLoginWindowClosed)

                // 显示窗口
                loginWindow.show()
                loginWindow.raise()
                loginWindow.requestActivate()
            }
        }
    }

    // 创建聊天窗口
    function createChatWindow() {
        if (chatWindow) {
            chatWindow.destroy()
        }

        var component = Qt.createComponent("qml/windows/ChatWindow.qml")
        if (component.status === Component.Ready) {
            createChatWindowObject(component)
        } else if (component.status === Component.Loading) {
            component.statusChanged.connect(function() {
                if (component.status === Component.Ready) {
                    createChatWindowObject(component)
                }
            })
        }
    }

    // 创建聊天窗口对象的辅助函数
    function createChatWindowObject(component) {
        chatWindow = component.createObject(null, {
            "themeManager": themeManager,
            "authManager": authManager,
            "sessionManager": sessionManager,
            "settingsManager": settingsManager
        })

        if (chatWindow) {
            // 连接信号
            chatWindow.logout.connect(onLogout)
            chatWindow.windowClosed.connect(onChatWindowClosed)

            // 显示窗口
            chatWindow.show()
            chatWindow.raise()
            chatWindow.requestActivate()
        }
    }

    // 登录成功处理
    function onLoginSucceeded(user) {
        // 关闭登录窗口（先断开信号连接，避免触发退出）
        if (loginWindow) {
            // 断开信号连接，避免触发windowClosed导致退出
            try {
                loginWindow.loginSucceeded.disconnect(onLoginSucceeded)
                loginWindow.windowClosed.disconnect(onLoginWindowClosed)
            } catch (error) {
                // 忽略断开连接的错误
            }

            loginWindow.close()
            loginWindow.destroy()
            loginWindow = null
        }

        // 延迟创建聊天窗口，确保登录窗口完全销毁
        Qt.callLater(function() {
            createChatWindow()
        })
    }

    // 登出处理
    function onLogout() {
        // 关闭聊天窗口（先断开信号连接）
        if (chatWindow) {
            try {
                chatWindow.logout.disconnect(onLogout)
                chatWindow.windowClosed.disconnect(onChatWindowClosed)
            } catch (error) {
                // 忽略断开连接的错误
            }

            chatWindow.close()
            chatWindow.destroy()
            chatWindow = null
        }

        // 创建并显示登录窗口
        createLoginWindow()
    }

    // 登录窗口关闭处理
    function onLoginWindowClosed() {
        Qt.quit()
    }

    // 聊天窗口关闭处理
    function onChatWindowClosed() {
        Qt.quit()
    }

    // 连接状态管理
    property bool lastConnectionState: true
    property bool connectionErrorShown: false
    property bool startupCompleted: false  // 启动是否完全完成

    // 防抖定时器
    Timer {
        id: connectionCheckTimer
        interval: 1000  // 1秒延迟
        repeat: false
        onTriggered: {
            if (typeof authManager !== "undefined" && authManager && !authManager.isConnected && !connectionErrorShown && appInitialized) {
                connectionErrorShown = true
            }
        }
    }

    // 自动连接定时器
    Timer {
        id: autoConnectTimer
        interval: 500  // 延迟500ms
        repeat: false
        onTriggered: {
            // 尝试自动连接到服务器
            if (settingsManager.autoConnect && authManager && !authManager.isConnected) {
                try {
                    authManager.connectToServer()
                } catch (e) {
                    // 启动期间静默处理连接错误
                }
            }
            
            // 启动自动登录定时器
            autoLoginTimer.start()
        }
    }

    // 自动登录定时器
    Timer {
        id: autoLoginTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (settingsManager.rememberPassword && settingsManager.savedUsername !== "") {
                try {
                    authManager.tryAutoLogin()
                } catch (e) {
                    // 静默处理自动登录错误
                }
            }
            
            // 完成启动流程，显示登录界面
            completeStartup()
        }
    }

    // 窗口关闭时保存设置
    onClosing: {
        settingsManager.saveWindowSettings(width, height, x, y)
        if (typeof authManager !== "undefined" && authManager !== null) {
            authManager.disconnectFromServer()
        }
    }

    // 设置管理器
    Components.SettingsManager {
        id: settingsManager
    }

    // 主题管理器
    Components.ThemeManager {
        id: themeManager
        isDarkTheme: settingsManager.isDarkTheme

        Component.onCompleted: {
            if (settingsManager.settings) {
                initialize(settingsManager.settings)
            }
        }

        onIsDarkThemeChanged: {
            settingsManager.saveTheme(isDarkTheme)
        }
    }

    // 连接认证管理器信号
    Connections {
        target: typeof authManager !== "undefined" ? authManager : null

        function onConnectionStateChanged(connected) {
            // 更新连接状态
            if (connected) {
                connectionCheckTimer.stop()
                connectionErrorShown = false
                lastConnectionState = true
            } else {
                if (appInitialized && lastConnectionState && !connectionErrorShown) {
                    connectionCheckTimer.restart()
                }
                lastConnectionState = false
            }
        }

        function onNetworkError(error) {
            // 网络错误处理
        }

        function onLoginSucceeded(user) {
            // AuthManager登录成功，由LoginWindow处理
        }

        function onLoginFailed(error) {
            // 登录失败处理
        }

        function onRegisterSucceeded(user) {
            // 注册成功处理
        }

        function onRegisterFailed(error) {
            // 注册失败处理
        }

        function onVerificationCodeSent() {
            // 验证码发送成功
        }

        function onVerificationCodeFailed(error) {
            // 验证码发送失败
        }

        function onLoadingStateChanged(loading) {
            // 加载状态变化
        }
    }

    // 连接会话管理器信号
    Connections {
        target: typeof sessionManager !== "undefined" ? sessionManager : null

        function onSessionExpired() {
            onLogout()
        }

        function onAutoLoginRequested(username, passwordHash) {
            // 自动登录请求处理
        }
    }

    // 应用程序启动时的初始化
    Component.onCompleted: {
        Qt.callLater(function() {
            if (typeof authManager === "undefined" || authManager === null) {
                return
            }
            initializeWithAuthManager()
        })
    }

    // 使用AuthManager进行初始化的函数
    function initializeWithAuthManager() {
        // 标记应用已初始化
        appInitialized = true

        // 延迟初始化，避免阻塞UI线程
        autoConnectTimer.start()
    }

    // 完成启动流程的函数
    function completeStartup() {
        // 等待一小段时间确保所有异步操作完成
        Qt.callLater(function() {
            startupCompleted = true
            // 创建并显示登录窗口
            createLoginWindow()
        })
    }

    // 处理启动超时的定时器
    Timer {
        id: startupTimeoutTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (!startupCompleted) {
                completeStartup()
            }
        }
        Component.onCompleted: start()
    }

    // 键盘快捷键处理
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Ctrl+T"
        onActivated: themeManager.toggleTheme()
    }

    Shortcut {
        sequence: "F11"
        onActivated: {
            if (visibility === Window.FullScreen) {
                visibility = Window.Windowed
            } else {
                visibility = Window.FullScreen
            }
        }
    }

    // 错误处理
    function handleCriticalError(title, message) {
        messageDialog.showError(title, message)
        console.error("Critical error:", title, message)
    }

    // 显示通知
    function showNotification(title, message, type) {
        type = type || "info"
        switch (type) {
        case "success":
            messageDialog.showSuccess(title, message)
            break
        case "warning":
            messageDialog.showWarning(title, message)
            break
        case "error":
            messageDialog.showError(title, message)
            break
        default:
            messageDialog.showInfo(title, message)
            break
        }
    }
}
