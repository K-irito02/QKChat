import QtQuick
import QtQuick.Controls
import QKChat 1.0

ApplicationWindow {
    id: mainWindow
    width: 400
    height: 600
    visible: true
    title: qsTr("QKChat")

    // 简化的背景色
    color: "#FFFFFF"

    // 简化的主题管理器
    QtObject {
        id: themeManager
        readonly property QtObject currentTheme: QtObject {
            readonly property color backgroundColor: "#FFFFFF"
            readonly property color surfaceColor: "#F8F9FA"
            readonly property color primaryColor: "#007AFF"
            readonly property color textPrimaryColor: "#000000"
            readonly property color textSecondaryColor: "#666666"
            readonly property color errorColor: "#FF3B30"
            readonly property color inputBackgroundColor: "#FFFFFF"
            readonly property color inputBorderColor: "#E0E0E0"
            readonly property color inputFocusColor: "#007AFF"
            readonly property color buttonDisabledColor: "#CCCCCC"
        }

        function toggleTheme() {
            console.log("Theme toggle requested")
        }

        function getOppositeThemeName() {
            return "深色"
        }
    }

    // 简化的加载对话框
    Dialog {
        id: loadingDialog
        modal: true
        anchors.centerIn: parent

        function show(text) {
            console.log("Loading:", text)
            open()
        }

        function hide() {
            close()
        }

        Text {
            text: "加载中..."
            anchors.centerIn: parent
        }
    }

    // 简化的消息对话框
    Dialog {
        id: messageDialog
        modal: true
        anchors.centerIn: parent

        property string messageText: ""

        function showError(title, message) {
            console.log("Error:", title, message)
            messageText = title + ": " + message
            open()
        }

        function showSuccess(title, message) {
            console.log("Success:", title, message)
            messageText = title + ": " + message
            open()
        }

        function showWarning(title, message) {
            console.log("Warning:", title, message)
            messageText = title + ": " + message
            open()
        }

        Text {
            text: messageDialog.messageText
            anchors.centerIn: parent
            wrapMode: Text.WordWrap
            width: 200
        }
    }

    // 简化的页面栈
    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: loginPageComponent
    }

    // 简化的登录页面组件
    Component {
        id: loginPageComponent
        Rectangle {
            color: themeManager.currentTheme.backgroundColor

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: "QKChat 登录"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: themeManager.currentTheme.textPrimaryColor
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                TextField {
                    id: usernameInput
                    width: 250
                    placeholderText: "用户名或邮箱"
                    text: "test@example.com"
                }

                TextField {
                    id: passwordInput
                    width: 250
                    placeholderText: "密码"
                    echoMode: TextInput.Password
                    text: "123456"
                }

                Button {
                    text: "登录"
                    width: 250
                    enabled: typeof authManager !== "undefined" && authManager !== null && authManager.isConnected

                    onClicked: {
                        console.log("Login button clicked")
                        if (typeof authManager !== "undefined" && authManager !== null) {
                            console.log("Calling authManager.login...")
                            authManager.login(usernameInput.text, passwordInput.text, false)
                        } else {
                            console.error("authManager is not available!")
                            messageDialog.showError("错误", "认证管理器不可用")
                        }
                    }
                }

                Text {
                    text: typeof authManager !== "undefined" && authManager !== null ?
                          (authManager.isConnected ? "已连接到服务器" : "未连接到服务器") :
                          "认证管理器不可用"
                    color: typeof authManager !== "undefined" && authManager !== null && authManager.isConnected ?
                           "green" : "red"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    // 简化的主页面组件
    Component {
        id: mainPageComponent
        Rectangle {
            color: themeManager.currentTheme.backgroundColor

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: "登录成功！"
                    font.pixelSize: 24
                    color: themeManager.currentTheme.textPrimaryColor
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Button {
                    text: "注销"
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: {
                        stackView.clear()
                        stackView.push(loginPageComponent)
                    }
                }
            }
        }
    }

    // 连接认证管理器信号
    Connections {
        target: typeof authManager !== "undefined" ? authManager : null

        function onConnectionStateChanged(connected) {
            console.log("Connection state changed:", connected)
            if (!connected) {
                messageDialog.showError("连接错误", "与服务器的连接已断开")
            }
        }

        function onNetworkError(error) {
            console.log("Network error:", error)
            messageDialog.showError("网络错误", error)
        }

        function onLoginSucceeded(user) {
            console.log("Login succeeded for user:", user ? user.username : "unknown")
            stackView.push(mainPageComponent)
        }

        function onLoginFailed(error) {
            console.log("Login failed:", error)
            messageDialog.showError("登录失败", error)
        }

        function onLoadingStateChanged(loading) {
            console.log("Loading state changed:", loading)
            if (loading) {
                loadingDialog.show("正在处理请求...")
            } else {
                loadingDialog.hide()
            }
        }
    }

    // 应用程序启动时的初始化
    Component.onCompleted: {
        console.log("=== Main.qml Component.onCompleted ===")
        console.log("authManager:", typeof authManager, authManager)
        console.log("sessionManager:", typeof sessionManager, sessionManager)
        console.log("themeManager:", typeof themeManager, themeManager)

        // 检查authManager是否可用
        if (typeof authManager === "undefined" || authManager === null) {
            console.error("ERROR: authManager is not available!")
            messageDialog.showError("初始化错误", "认证管理器不可用")
            return
        }

        console.log("authManager.isConnected:", authManager.isConnected)

        // 尝试自动连接到服务器
        if (!authManager.isConnected) {
            console.log("Attempting to connect to server...")
            try {
                authManager.connectToServer()
            } catch (e) {
                console.error("Error connecting to server:", e)
                messageDialog.showError("连接错误", "无法连接到服务器: " + e.toString())
            }
        } else {
            console.log("Already connected to server")
        }

        // 尝试自动登录
        console.log("Attempting auto login...")
        try {
            authManager.tryAutoLogin()
        } catch (e) {
            console.error("Error during auto login:", e)
        }

        console.log("=== Main.qml initialization complete ===")
    }

    // 应用程序关闭时的清理
    onClosing: {
        if (typeof authManager !== "undefined" && authManager !== null) {
            authManager.disconnectFromServer()
        }
    }
}
