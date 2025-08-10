import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components" as Components

/**
 * @brief 注册页面
 *
 * 提供用户注册功能，包含用户名输入、邮箱输入、验证码输入、
 * 密码输入、确认密码输入、表单验证和错误提示。
 */
Rectangle {
    id: registerPage

    // 公共属性
    property var themeManager
    property var authManager
    property var loadingDialog
    property var messageDialog

    // 注册状态
    property bool isSendingCode: false
    property int countdown: 0

    // 信号
    signal navigateToLogin()
    signal registerSucceeded()

    color: themeManager.currentTheme.backgroundColor

    // 发送验证码
    function sendVerificationCode() {
        // 防止重复发送
        if (isSendingCode || countdown > 0) {
            return
        }

        // 清除邮箱错误
        emailInput.clearError()

        var email = emailInput.text.trim()

        if (email === "") {
            emailInput.setError("请输入邮箱地址")
            emailInput.focus = true
            return
        }

        if (!validateEmail(email)) {
            emailInput.setError("请输入有效的邮箱地址")
            emailInput.focus = true
            return
        }

        // 检查认证管理器是否可用
        if (typeof authManager === "undefined" || authManager === null) {
            messageDialog.showError("系统错误", "认证管理器不可用")
            return
        }

        // 检查连接状态
        if (!authManager.isConnected) {
            messageDialog.showError("连接错误", "未连接到服务器，请检查网络连接")
            return
        }

        // 设置发送状态
        isSendingCode = true

        if (authManager.sendVerificationCode(email)) {
            countdown = 60
            countdownTimer.start()
        } else {
            // 发送失败，重置状态
            isSendingCode = false
        }
    }

    // 执行注册
    function performRegister() {
        // 清除之前的错误
        usernameInput.clearError()
        emailInput.clearError()
        passwordInput.clearError()
        confirmPasswordInput.clearError()
        verificationCodeInput.clearError()

        var username = usernameInput.text.trim()
        var email = emailInput.text.trim()
        var password = passwordInput.text.trim()
        var confirmPassword = confirmPasswordInput.text.trim()
        var verificationCode = verificationCodeInput.text.trim()

        // 表单验证
        var hasError = false

        if (username === "") {
            usernameInput.setError("请输入用户名")
            hasError = true
        } else if (!validateUsername(username)) {
            usernameInput.setError("用户名只能包含字母、数字和下划线，长度3-20位")
            hasError = true
        }

        if (email === "") {
            emailInput.setError("请输入邮箱地址")
            hasError = true
        } else if (!validateEmail(email)) {
            emailInput.setError("请输入有效的邮箱地址")
            hasError = true
        }

        if (password === "") {
            passwordInput.setError("请输入密码")
            hasError = true
        } else if (!validatePassword(password)) {
            passwordInput.setError("密码长度至少8位，包含字母和数字")
            hasError = true
        }

        if (confirmPassword === "") {
            confirmPasswordInput.setError("请确认密码")
            hasError = true
        } else if (password !== confirmPassword) {
            confirmPasswordInput.setError("两次输入的密码不一致")
            hasError = true
        }

        if (verificationCode === "") {
            verificationCodeInput.setError("请输入验证码")
            hasError = true
        } else if (!validateVerificationCode(verificationCode)) {
            verificationCodeInput.setError("验证码应为6位数字")
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

        // 使用认证管理器进行注册
        if (authManager.registerUser(username, email, password, verificationCode)) {
            // 注册请求已发送，等待响应
        } else {
            messageDialog.showError("注册失败", "无法发送注册请求")
        }
    }

    // 验证用户名格式
    function validateUsername(username) {
        var usernameRegex = /^[a-zA-Z0-9_]{3,20}$/
        return usernameRegex.test(username)
    }

    // 验证邮箱格式
    function validateEmail(email) {
        var emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/
        return emailRegex.test(email)
    }

    // 验证密码强度
    function validatePassword(password) {
        // 至少8位，包含字母和数字
        if (password.length < 8) return false
        var hasLetter = /[a-zA-Z]/.test(password)
        var hasNumber = /[0-9]/.test(password)
        return hasLetter && hasNumber
    }

    // 验证验证码格式
    function validateVerificationCode(code) {
        // 检查长度是否为6位
        if (code.length !== 6) return false
        // 检查是否全为数字
        return /^[0-9]{6}$/.test(code)
    }

    // 主布局
    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: registerLayout.height + 80

        ColumnLayout {
            id: registerLayout
            width: parent.width
            anchors.margins: 40
            spacing: 20

            // 顶部间距
            Item {
                Layout.preferredHeight: 20
            }

            // 标题区域
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16

                // Logo图标
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 80
                    radius: 20
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
                        font.pixelSize: 32
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
                    text: qsTr("创建账号 加入QKChat聊天")
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    font.family: "Microsoft YaHei UI"
                    color: themeManager.currentTheme.textPrimaryColor
                    font.letterSpacing: 0.8
                }
            }

            // 输入表单区域
            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 16
                spacing: 14

                // 用户名输入
                Components.CustomTextField {
                    id: usernameInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    themeManager: registerPage.themeManager
                    placeholderText: qsTr("用户名")
                    showClearButton: true

                    onTextChanged: clearError()
                }

                // 邮箱输入
                Components.CustomTextField {
                    id: emailInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    themeManager: registerPage.themeManager
                    placeholderText: qsTr("邮箱地址")
                    showClearButton: true

                    onTextChanged: clearError()
                }

                // 密码输入
                Components.CustomTextField {
                    id: passwordInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    themeManager: registerPage.themeManager
                    placeholderText: qsTr("密码")
                    isPassword: true

                    onTextChanged: clearError()
                }

                // 确认密码输入
                Components.CustomTextField {
                    id: confirmPasswordInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    themeManager: registerPage.themeManager
                    placeholderText: qsTr("确认密码")
                    isPassword: true

                    onTextChanged: clearError()
                }

                // 验证码输入
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Components.CustomTextField {
                        id: verificationCodeInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        themeManager: registerPage.themeManager
                        placeholderText: qsTr("验证码")

                        onTextChanged: clearError()
                        onAccepted: performRegister()
                    }

                    Components.CustomButton {
                        id: sendCodeButton
                        Layout.preferredWidth: 110
                        Layout.preferredHeight: 48
                        themeManager: registerPage.themeManager
                        text: countdown > 0 ? countdown + "s" : qsTr("发送验证码")
                        buttonType: "outline"
                        enabled: !isSendingCode && countdown === 0 && 
                                (typeof authManager !== "undefined" && authManager && authManager.isConnected && !authManager.isLoading)
                        onClicked: sendVerificationCode()
                    }
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

            // 注册按钮
            Components.CustomButton {
                id: registerButton
                Layout.fillWidth: true
                Layout.maximumWidth: 320
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 48
                Layout.topMargin: 12
                themeManager: registerPage.themeManager
                text: (typeof authManager !== "undefined" && authManager && authManager.isLoading) ? qsTr("注册中...") : qsTr("注册")
                buttonType: "primary"
                isLoading: (typeof authManager !== "undefined" && authManager && authManager.isLoading)
                enabled: !isLoading
                onClicked: performRegister()
            }



            // 返回登录链接
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 0
                spacing: 0

                Text {
                    text: qsTr("已有账号？")
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                    font.family: "Microsoft YaHei UI"
                }

                Button {
                    text: qsTr("返回登录")
                    onClicked: navigateToLogin()
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

            // 底部间距
            Item {
                Layout.preferredHeight: 20
            }
        }
    }

    // 倒计时定时器
    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            countdown--
            if (countdown <= 0) {
                stop()
                isSendingCode = false
                // 确保按钮状态正确
                sendCodeButton.enabled = true
            }
        }
    }

    // 连接认证管理器信号
    Connections {
        target: typeof authManager !== "undefined" ? authManager : null

        function onRegisterSucceeded(user) {
            registerSucceeded()
        }

        function onRegisterFailed(error) {
            messageDialog.showError("注册失败", error)
        }

        function onVerificationCodeSent() {
            messageDialog.showSuccess("验证码已发送", "验证码已发送到您的邮箱，请查收")
        }

        function onVerificationCodeFailed(error) {
            messageDialog.showError("发送失败", error)
            // 重置发送状态
            isSendingCode = false
            countdownTimer.stop()
            countdown = 0
            // 重新启用发送按钮
            sendCodeButton.enabled = true
        }
    }

    // 初始化
    Component.onCompleted: {
        usernameInput.focus = true
    }
}