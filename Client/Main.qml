import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QKChat 1.0
import "qml/components" as Components

ApplicationWindow {
    id: mainWindow
    width: settingsManager.windowWidth
    height: settingsManager.windowHeight
    visible: true
    title: qsTr("QKChat")



    // 连接状态管理
    property bool lastConnectionState: true
    property bool connectionErrorShown: false
    property bool appInitialized: false

    // 防抖定时器
    Timer {
        id: connectionCheckTimer
        interval: 1000  // 1秒延迟
        repeat: false
        onTriggered: {
            if (typeof authManager !== "undefined" && authManager && !authManager.isConnected && !connectionErrorShown && appInitialized) {
                messageDialog.showError("连接错误", "与服务器的连接已断开")
                connectionErrorShown = true
            }
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

    // 加载对话框
    Components.LoadingDialog {
        id: loadingDialog
        themeManager: themeManager
        parent: Overlay.overlay
    }

    // 消息对话框
    Components.MessageDialog {
        id: messageDialog
        themeManager: themeManager
        parent: Overlay.overlay
    }

    // 页面栈
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: loginPageComponent

        // 使用自定义转换动画
        pushEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: stackView.width
                to: 0
                duration: 300
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        pushExit: Transition {
            PropertyAnimation {
                property: "x"
                from: 0
                to: -stackView.width * 0.3
                duration: 300
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.7
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        popEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: -stackView.width * 0.3
                to: 0
                duration: 300
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                property: "opacity"
                from: 0.7
                to: 1.0
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        popExit: Transition {
            PropertyAnimation {
                property: "x"
                from: 0
                to: stackView.width
                duration: 300
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
    }

    // 登录页面组件
    Component {
        id: loginPageComponent
        Loader {
            source: "qml/LoginPage.qml"
            asynchronous: false  // 同步加载避免显示问题
            onLoaded: {
                item.themeManager = themeManager
                item.loadingDialog = loadingDialog
                item.messageDialog = messageDialog
                item.navigateToRegister.connect(function() {
                    stackView.push(registerPageComponent)
                })
                item.loginSucceeded.connect(function() {
                    stackView.push(mainPageComponent)
                })
            }
        }
    }



    // 注册页面组件
    Component {
        id: registerPageComponent
        Loader {
            source: "qml/RegisterPage.qml"
            asynchronous: false  // 同步加载避免显示问题
            onLoaded: {
                item.themeManager = themeManager
                item.loadingDialog = loadingDialog
                item.messageDialog = messageDialog
                item.navigateToLogin.connect(function() {
                    stackView.pop()
                })
                item.registerSucceeded.connect(function() {
                    stackView.push(mainPageComponent)
                })
            }
        }
    }


    // 主页面组件
    Component {
        id: mainPageComponent
        Loader {
            source: "qml/pages/MainPage.qml"
            onLoaded: {
                item.themeManager = themeManager
                item.logout.connect(function() {
                    stackView.clear()
                    stackView.push(loginPageComponent)
                })
            }
        }
    }

    // 连接认证管理器信号
    Connections {
        target: typeof authManager !== "undefined" ? authManager : null

        function onConnectionStateChanged(connected) {
            console.log("Connection state changed:", connected)

            // 更新连接状态
            if (connected) {
                // 连接成功，停止定时器并重置错误标志
                connectionCheckTimer.stop()
                connectionErrorShown = false
                lastConnectionState = true
            } else {
                // 连接断开，但只在应用初始化完成且之前是连接状态时才显示错误
                if (appInitialized && lastConnectionState && !connectionErrorShown) {
                    // 启动定时器进行延迟检查
                    connectionCheckTimer.restart()
                }
                lastConnectionState = false
            }
        }

        function onNetworkError(error) {
            messageDialog.showError("网络错误", error)
        }

        function onLoginSucceeded(user) {
            // 保存登录信息（如果选择了记住密码）
            if (typeof sessionManager !== "undefined" && sessionManager !== null) {
                var currentPage = stackView.currentItem
                if (currentPage && currentPage.item) {
                    var loginPage = currentPage.item
                    if (loginPage.rememberCheckBox && loginPage.rememberCheckBox.checked) {
                        settingsManager.saveLoginInfo(
                            loginPage.usernameInput.text,
                            "", // 密码哈希由SessionManager处理
                            true
                        )
                    }
                }
            }
            stackView.push(mainPageComponent)
        }

        function onLoginFailed(error) {
            messageDialog.showError("登录失败", error)
        }

        function onRegisterSucceeded(user) {
            stackView.pop()
            messageDialog.showSuccess("注册成功", "账号创建成功，请登录")
        }

        function onRegisterFailed(error) {
            messageDialog.showError("注册失败", error)
        }

        function onVerificationCodeSent() {
            messageDialog.showSuccess("验证码已发送", "验证码已发送到您的邮箱，请查收")
        }

        function onVerificationCodeFailed(error) {
            messageDialog.showError("发送失败", error)
        }

        function onLoadingStateChanged(loading) {
            if (loading) {
                loadingDialog.show("正在处理请求...")
            } else {
                loadingDialog.hide()
            }
        }
    }

    // 连接会话管理器信号
    Connections {
        target: typeof sessionManager !== "undefined" ? sessionManager : null

        function onSessionExpired() {
            messageDialog.showWarning("会话过期", "您的登录会话已过期，请重新登录")
            stackView.clear()
            stackView.push(loginPageComponent)
        }

        function onAutoLoginRequested(username, passwordHash) {
            // 自动登录逻辑已在LoginPage中处理
        }
    }

    // 应用程序启动时的初始化
    Component.onCompleted: {
        // 检查authManager是否可用（延迟检查，给C++对象时间初始化）
        Qt.callLater(function() {
            if (typeof authManager === "undefined" || authManager === null) {
                console.warn("AuthManager not available during initialization")
                messageDialog.showError("初始化错误", "认证管理器不可用，请重启应用程序")
                return
            }

            // AuthManager可用，继续初始化
            initializeWithAuthManager()
        })
    }

    // 使用AuthManager进行初始化的函数
    function initializeWithAuthManager() {
        // 标记应用已初始化
        appInitialized = true

        // 恢复窗口位置（避免重复定义多个 Component.onCompleted）
        if (settingsManager.windowX >= 0 && settingsManager.windowY >= 0) {
            x = settingsManager.windowX
            y = settingsManager.windowY
        }

        // 延迟初始化，避免阻塞UI线程
        Qt.callLater(function() {
            // 尝试自动连接到服务器
            if (settingsManager.autoConnect && !authManager.isConnected) {
                try {
                    var result = authManager.connectToServer()
                    if (!result) {
                        console.warn("Failed to connect to server automatically")
                    }
                } catch (e) {
                    console.error("Exception during auto-connect:", e.toString())
                    messageDialog.showError("连接错误", "无法连接到服务器: " + e.toString())
                }
            }

            // 延迟尝试自动登录
            Qt.callLater(function() {
                if (settingsManager.rememberPassword && settingsManager.savedUsername !== "") {
                    try {
                        var autoLoginResult = authManager.tryAutoLogin()
                        if (!autoLoginResult) {
                            console.log("Auto-login not available or failed")
                        }
                    } catch (e) {
                        // 静默处理自动登录错误
                        console.warn("Auto-login failed:", e.toString())
                    }
                }
            })
        })
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
