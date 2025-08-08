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
            messageText = title + ": " + message
            open()
        }

        function showSuccess(title, message) {
            messageText = title + ": " + message
            open()
        }

        function showWarning(title, message) {
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
                        if (typeof authManager !== "undefined" && authManager !== null) {
                            authManager.login(usernameInput.text, passwordInput.text, false)
                        } else {
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

                Text {
                    text: "点击登录按钮测试连接"
                    color: "gray"
                    font.pixelSize: 12
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
            if (!connected) {
                messageDialog.showError("连接错误", "与服务器的连接已断开")
            }
        }

        function onNetworkError(error) {
            messageDialog.showError("网络错误", error)
        }

        function onLoginSucceeded(user) {
            stackView.push(mainPageComponent)
        }

        function onLoginFailed(error) {
            messageDialog.showError("登录失败", error)
        }

        function onLoadingStateChanged(loading) {
            if (loading) {
                loadingDialog.show("正在处理请求...")
            } else {
                loadingDialog.hide()
            }
        }
    }

    // 应用程序启动时的初始化
    Component.onCompleted: {
        // 检查authManager是否可用
        if (typeof authManager === "undefined" || authManager === null) {
            messageDialog.showError("初始化错误", "认证管理器不可用")
            return
        }

        // 延迟初始化，避免阻塞UI线程
        Qt.callLater(function() {
            // 尝试自动连接到服务器
            if (!authManager.isConnected) {
                try {
                    var result = authManager.connectToServer()
                } catch (e) {
                    messageDialog.showError("连接错误", "无法连接到服务器: " + e.toString())
                }
            }

            // 延迟尝试自动登录
            Qt.callLater(function() {
                try {
                    var autoLoginResult = authManager.tryAutoLogin()
                } catch (e) {
                    // 静默处理自动登录错误
                }
            })
        })
    }

    // 应用程序关闭时的清理
    onClosing: {
        if (typeof authManager !== "undefined" && authManager !== null) {
            authManager.disconnectFromServer()
        }
    }
}
