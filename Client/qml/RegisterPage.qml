import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QKChat 1.0

Rectangle {
    id: registerPage

    // 公共属性
    property var themeManager
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
        var email = emailInput.text.trim()
        if (authManager.sendVerificationCode(email)) {
            errorText.visible = false
        }
    }

    // 执行注册
    function performRegister() {
        var username = usernameInput.text.trim()
        var email = emailInput.text.trim()
        var password = passwordInput.text.trim()
        var verificationCode = verificationCodeInput.text.trim()

        // 使用认证管理器进行注册
        if (authManager.registerUser(username, email, password, verificationCode)) {
            errorText.visible = false
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: registerLayout.height + 80

        ColumnLayout {
            id: registerLayout
            width: parent.width
            anchors.margins: 40
            spacing: 20
                
            // 标题
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("创建账号")
                font.pixelSize: 32
                font.weight: Font.Bold
                color: themeManager.currentTheme.textPrimaryColor
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("加入QKChat聊天")
                font.pixelSize: 18
                color: themeManager.currentTheme.textSecondaryColor
            }

            // 用户名输入
            TextField {
                id: usernameInput
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                placeholderText: qsTr("用户名")
                font.pixelSize: 16
                color: themeManager.currentTheme.textPrimaryColor
                background: Rectangle {
                    color: themeManager.currentTheme.inputBackgroundColor
                    border.color: usernameInput.focus ?
                                 themeManager.currentTheme.inputFocusColor :
                                 themeManager.currentTheme.inputBorderColor
                    border.width: 1
                    radius: 8
                }
            }

            // 邮箱输入
            TextField {
                id: emailInput
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                placeholderText: qsTr("邮箱地址")
                font.pixelSize: 16
                color: themeManager.currentTheme.textPrimaryColor
                background: Rectangle {
                    color: themeManager.currentTheme.inputBackgroundColor
                    border.color: emailInput.focus ?
                                 themeManager.currentTheme.inputFocusColor :
                                 themeManager.currentTheme.inputBorderColor
                    border.width: 1
                    radius: 8
                }
            }

            // 密码输入
            TextField {
                id: passwordInput
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                placeholderText: qsTr("密码")
                echoMode: TextInput.Password
                font.pixelSize: 16
                color: themeManager.currentTheme.textPrimaryColor
                background: Rectangle {
                    color: themeManager.currentTheme.inputBackgroundColor
                    border.color: passwordInput.focus ?
                                 themeManager.currentTheme.inputFocusColor :
                                 themeManager.currentTheme.inputBorderColor
                    border.width: 1
                    radius: 8
                }
            }

            // 确认密码输入
            TextField {
                id: confirmPasswordInput
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                placeholderText: qsTr("确认密码")
                echoMode: TextInput.Password
                font.pixelSize: 16
                color: themeManager.currentTheme.textPrimaryColor
                background: Rectangle {
                    color: themeManager.currentTheme.inputBackgroundColor
                    border.color: confirmPasswordInput.focus ?
                                 themeManager.currentTheme.inputFocusColor :
                                 themeManager.currentTheme.inputBorderColor
                    border.width: 1
                    radius: 8
                }
            }
                
            // 验证码输入
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                TextField {
                    id: verificationCodeInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    placeholderText: qsTr("验证码")
                    font.pixelSize: 16
                    color: themeManager.currentTheme.textPrimaryColor
                    background: Rectangle {
                        color: themeManager.currentTheme.inputBackgroundColor
                        border.color: verificationCodeInput.focus ?
                                     themeManager.currentTheme.inputFocusColor :
                                     themeManager.currentTheme.inputBorderColor
                        border.width: 1
                        radius: 8
                    }
                }

                Button {
                    id: sendCodeButton
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 50
                    text: countdown > 0 ? countdown + "s" : qsTr("发送验证码")
                    enabled: !isSendingCode && countdown === 0 && !authManager.isLoading
                    onClicked: sendVerificationCode()

                    background: Rectangle {
                        color: sendCodeButton.enabled ?
                               themeManager.currentTheme.primaryColor :
                               themeManager.currentTheme.buttonDisabledColor
                        radius: 8
                    }

                    contentItem: Text {
                        text: sendCodeButton.text
                        color: "white"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
                
            // 主题切换
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("主题:")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: themeManager.getOppositeThemeName()
                    onClicked: themeManager.toggleTheme()
                    background: Rectangle {
                        color: themeManager.currentTheme.primaryColor
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
                
            // 错误信息
            Text {
                id: errorText
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                text: ""
                color: themeManager.currentTheme.errorColor
                font.pixelSize: 14
                visible: false
                horizontalAlignment: Text.AlignHCenter
            }

            // 注册按钮
            Button {
                id: registerButton
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                text: authManager.isLoading ? qsTr("注册中...") : qsTr("注册")
                enabled: !authManager.isLoading && authManager.isConnected
                onClicked: performRegister()

                background: Rectangle {
                    color: registerButton.enabled ?
                           themeManager.currentTheme.primaryColor :
                           themeManager.currentTheme.buttonDisabledColor
                    radius: 8
                }

                contentItem: Text {
                    text: registerButton.text
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
                
            // 返回登录链接
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("已有账号？")
                color: themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 14
            }

            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("返回登录")
                flat: true
                onClicked: navigateToLogin()

                contentItem: Text {
                    text: parent.text
                    color: themeManager.currentTheme.primaryColor
                    font.pixelSize: 14
                    font.underline: true
                }
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
            }
        }
    }

    // 连接认证管理器信号
    Connections {
        target: authManager

        function onRegisterSucceeded(user) {
            console.log("Register succeeded for user:", user.username)
            registerSucceeded()
        }

        function onRegisterFailed(error) {
            console.log("Register failed:", error)
            errorText.text = error
            errorText.visible = true
        }

        function onVerificationCodeSent() {
            console.log("Verification code sent")
            isSendingCode = false
            countdown = 60
            countdownTimer.start()
            messageDialog.showSuccess("验证码已发送", "请查看您的邮箱并输入验证码")
        }

        function onVerificationCodeFailed(error) {
            console.log("Verification code failed:", error)
            isSendingCode = false
            errorText.text = error
            errorText.visible = true
        }
    }

    // 初始化
    Component.onCompleted: {
        usernameInput.focus = true
    }
}