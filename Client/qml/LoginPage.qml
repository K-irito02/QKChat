import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QKChat 1.0
import "components" as Components

/**
 * @brief 登录页面
 *
 * 提供用户登录功能，包含用户名/邮箱输入、密码输入、
 * 记住密码选项、表单验证和错误提示。
 */
Rectangle {
    id: loginPage

    // 公共属性
    property var themeManager
    property var loadingDialog
    property var messageDialog

    // 信号
    signal navigateToRegister()
    signal loginSucceeded()

    color: themeManager.currentTheme.backgroundColor

    // 执行登录
    function performLogin() {
        // 清除之前的错误
        usernameInput.clearError()
        passwordInput.clearError()

        var username = usernameInput.text.trim()
        var password = passwordInput.text.trim()
        var rememberMe = rememberCheckBox.checked

        // 表单验证
        var hasError = false

        if (username === "") {
            usernameInput.setError("请输入用户名或邮箱")
            hasError = true
        } else if (!validateUsernameOrEmail(username)) {
            usernameInput.setError("请输入有效的用户名或邮箱")
            hasError = true
        }

        if (password === "") {
            passwordInput.setError("请输入密码")
            hasError = true
        } else if (password.length < 6) {
            passwordInput.setError("密码长度至少6位")
            hasError = true
        }

        if (hasError) {
            return
        }

        // 检查认证管理器是否可用
        if (typeof authManager === "undefined" || authManager === null) {
            messageDialog.showError("系统错误", "认证管理器不可用")
            return
        }

        // 检查是否连接到服务器
        if (typeof authManager === "undefined" || !authManager || !authManager.isConnected) {
            messageDialog.showError("连接错误", "请先连接到服务器")
            return
        }

        // 使用认证管理器进行登录
        if (authManager.login(username, password, rememberMe)) {
            // 登录请求已发送，等待响应
        } else {
            messageDialog.showError("登录失败", "无法发送登录请求")
        }
    }

    // 验证用户名或邮箱格式
    function validateUsernameOrEmail(input) {
        // 简单的邮箱正则表达式
        var emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/
        // 用户名规则：3-20位字母数字下划线
        var usernameRegex = /^[a-zA-Z0-9_]{3,20}$/

        return emailRegex.test(input) || usernameRegex.test(input)
    }

    // 主布局
    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: loginLayout.height + 80

        ColumnLayout {
            id: loginLayout
            width: parent.width
            anchors.margins: 40
            spacing: 24

            // 顶部间距
            Item {
                Layout.preferredHeight: 40
            }

            // Logo和标题区域
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16

                // Logo图标
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 100
                    radius: 25
                    color: themeManager.currentTheme.primaryColor

                    // 渐变效果
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: themeManager.currentTheme.primaryColor }
                        GradientStop { position: 1.0; color: themeManager.currentTheme.primaryLightColor }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "QK"
                        color: "white"
                        font.pixelSize: 40
                        font.weight: Font.Bold
                        font.family: "Microsoft YaHei UI"
                    }

                    // 阴影效果
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -3
                        color: "transparent"
                        border.color: themeManager.currentTheme.shadowColor
                        border.width: 1
                        radius: parent.radius + 3
                        z: -1
                    }
                }

                // 标题
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("欢迎回来 QKChat")
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                    font.family: "Microsoft YaHei UI"
                    color: themeManager.currentTheme.textPrimaryColor
                    font.letterSpacing: 1.0
                }
            }

            // 输入表单区域
            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 16
                spacing: 16

                // 用户名/邮箱输入
                Components.CustomTextField {
                    id: usernameInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    themeManager: loginPage.themeManager
                    placeholderText: qsTr("用户名或邮箱")
                    showClearButton: true

                    onAccepted: passwordInput.focus = true
                    onTextChanged: clearError()
                }

                // 密码输入
                Components.CustomTextField {
                    id: passwordInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    themeManager: loginPage.themeManager
                    placeholderText: qsTr("密码")
                    isPassword: true

                    onAccepted: performLogin()
                    onTextChanged: clearError()
                }
            }

            // 底部状态栏：记住密码、连接状态、主题切换
            RowLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                spacing: 0

                // 左侧：记住密码复选框
                CheckBox {
                    id: rememberCheckBox
                    text: qsTr("记住密码")
                    spacing: 6
                    Layout.alignment: Qt.AlignLeft

                    // 移除背景悬停效果，只在indicator上应用
                    background: Rectangle {
                        color: "transparent"
                    }

                    contentItem: Text {
                        text: parent.text
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.family: "Microsoft YaHei UI"
                        leftPadding: parent.indicator.width + parent.spacing
                        verticalAlignment: Text.AlignVCenter
                    }

                    indicator: Rectangle {
                        implicitWidth: 18
                        implicitHeight: 18
                        radius: 4
                        // 垂直居中对齐，与文字基线对齐
                        anchors.verticalCenter: parent.verticalCenter
                        border.color: {
                            if (rememberCheckBox.checked) return themeManager.currentTheme.primaryColor
                            if (rememberCheckBox.hovered) return themeManager.currentTheme.primaryColor
                            return themeManager.currentTheme.inputBorderColor
                        }
                        border.width: rememberCheckBox.checked || rememberCheckBox.hovered ? 2 : 1
                        color: rememberCheckBox.checked ?
                               themeManager.currentTheme.primaryColor :
                               "transparent"

                        // 只在勾选框上应用悬停效果
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: 6
                            color: rememberCheckBox.hovered && !rememberCheckBox.checked ?
                                   Qt.lighter(themeManager.currentTheme.primaryColor, 1.9) :
                                   "transparent"
                            z: -1

                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                        }

                        // 勾选标记
                        Text {
                            anchors.centerIn: parent
                            text: "✓"
                            color: "white"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            visible: rememberCheckBox.checked
                        }

                        Behavior on border.color {
                            ColorAnimation { duration: 150 }
                        }

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }

                        Behavior on border.width {
                            NumberAnimation { duration: 150 }
                        }
                    }
                }

                // 中央：弹性空间
                Item { Layout.fillWidth: true }

                // 中央：连接状态显示
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 6

                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: (typeof authManager !== "undefined" && authManager && authManager.isConnected) ?
                               themeManager.currentTheme.successColor :
                               themeManager.currentTheme.errorColor

                        // 优化的闪烁动画（未连接时）
                        SequentialAnimation {
                            running: (typeof authManager !== "undefined" && authManager && !authManager.isConnected)
                            loops: Animation.Infinite
                            OpacityAnimator {
                                target: parent
                                from: 1.0
                                to: 0.6
                                duration: 2500
                                easing.type: Easing.InOutSine
                            }
                            OpacityAnimator {
                                target: parent
                                from: 0.6
                                to: 1.0
                                duration: 2500
                                easing.type: Easing.InOutSine
                            }
                        }

                        // 优化的连接成功脉冲动画
                        SequentialAnimation {
                            running: (typeof authManager !== "undefined" && authManager && authManager.isConnected)
                            loops: Animation.Infinite
                            ScaleAnimator {
                                target: parent
                                from: 1.0
                                to: 1.03
                                duration: 3000
                                easing.type: Easing.InOutSine
                            }
                            ScaleAnimator {
                                target: parent
                                from: 1.03
                                to: 1.0
                                duration: 3000
                                easing.type: Easing.InOutSine
                            }
                        }
                    }

                    Text {
                        text: (typeof authManager !== "undefined" && authManager && authManager.isConnected) ?
                              qsTr("已连接到服务器") :
                              qsTr("未连接到服务器")
                        color: themeManager.currentTheme.textTertiaryColor
                        font.pixelSize: 14
                        font.family: "Microsoft YaHei UI"
                    }
                }

                // 右侧：弹性空间
                Item { Layout.fillWidth: true }

                // 右侧：主题切换按钮
                Button {
                    text: themeManager.getOppositeThemeName()
                    onClicked: themeManager.toggleTheme()
                    Layout.alignment: Qt.AlignRight

                    background: Rectangle {
                        color: {
                            if (parent.pressed) return Qt.darker(themeManager.currentTheme.primaryColor, 1.1)
                            if (parent.hovered) return Qt.lighter(themeManager.currentTheme.primaryColor, 1.1)
                            return themeManager.currentTheme.primaryColor
                        }
                        radius: 8
                        border.color: themeManager.currentTheme.primaryColor
                        border.width: 1

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 14
                        font.family: "Microsoft YaHei UI"
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 登录按钮
            Components.CustomButton {
                id: loginButton
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 48
                Layout.topMargin: 12
                themeManager: loginPage.themeManager
                text: (typeof authManager !== "undefined" && authManager && authManager.isLoading) ? qsTr("登录中...") : qsTr("登录")
                buttonType: "primary"
                isLoading: (typeof authManager !== "undefined" && authManager && authManager.isLoading)
                enabled: !isLoading
                onClicked: performLogin()
            }



            // 注册链接
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 20
                spacing: 14

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 1
                    color: themeManager.currentTheme.dividerColor
                    opacity: 0.6
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Text {
                        text: qsTr("还没有账号？")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.family: "Microsoft YaHei UI"
                    }

                    Button {
                        text: qsTr("立即注册")
                        onClicked: navigateToRegister()
                        flat: true

                        background: Rectangle {
                            color: parent.pressed ?
                                   Qt.lighter(themeManager.currentTheme.primaryColor, 1.8) :
                                   parent.hovered ?
                                   Qt.lighter(themeManager.currentTheme.primaryColor, 1.9) :
                                   "transparent"
                            radius: 6

                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                        }

                        contentItem: Text {
                            text: parent.text
                            color: themeManager.currentTheme.primaryColor
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            font.family: "Microsoft YaHei UI"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // 底部间距
            Item {
                Layout.preferredHeight: 40
            }
        }
    }

    // 连接认证管理器信号
    Connections {
        target: authManager

        function onLoginSucceeded(user) {
            loginSucceeded()
        }

        function onLoginFailed(error) {
            messageDialog.showError("登录失败", error)
        }
    }

    // 连接会话管理器信号
    Connections {
        target: typeof sessionManager !== "undefined" ? sessionManager : null

        function onAutoLoginRequested(username, passwordHash) {
            // 处理自动登录请求
            usernameInput.text = username
            rememberCheckBox.checked = true
        }
    }

    // 初始化
    Component.onCompleted: {
        // 设置初始焦点
        usernameInput.focus = true

        // 检查会话管理器是否可用并加载保存的登录信息
        if (typeof sessionManager !== "undefined" && sessionManager !== null) {
            if (sessionManager.isRememberMeEnabled) {
                var loginInfo = sessionManager.getSavedLoginInfo()
                if (loginInfo.first) {
                    usernameInput.text = loginInfo.first
                    rememberCheckBox.checked = true
                    passwordInput.focus = true
                }
            }
        }
    }
}