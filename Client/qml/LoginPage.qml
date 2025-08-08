import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QKChat 1.0

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
        var username = usernameInput.text.trim()
        var password = passwordInput.text.trim()
        var rememberMe = rememberCheckBox.checked

        // 使用认证管理器进行登录
        if (authManager.login(username, password, rememberMe)) {
            errorText.visible = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 20
        
        // 标题
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("QKChat")
            font.pixelSize: 32
            font.weight: Font.Bold
            color: themeManager.currentTheme.textPrimaryColor
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("欢迎回来")
            font.pixelSize: 18
            color: themeManager.currentTheme.textSecondaryColor
        }
            
        // 用户名/邮箱输入
        TextField {
            id: usernameInput
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            placeholderText: qsTr("用户名或邮箱")
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

            onAccepted: passwordInput.focus = true
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

            onAccepted: performLogin()
        }
            
        // 记住密码和主题切换
        RowLayout {
            Layout.fillWidth: true

            CheckBox {
                id: rememberCheckBox
                text: qsTr("记住密码")

                contentItem: Text {
                    text: parent.text
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                    leftPadding: parent.indicator.width + parent.spacing
                    verticalAlignment: Text.AlignVCenter
                }
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
            
        // 登录按钮
        Button {
            id: loginButton
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            text: authManager.isLoading ? qsTr("登录中...") : qsTr("登录")
            enabled: !authManager.isLoading && authManager.isConnected
            onClicked: performLogin()

            background: Rectangle {
                color: loginButton.enabled ?
                       themeManager.currentTheme.primaryColor :
                       themeManager.currentTheme.buttonDisabledColor
                radius: 8
            }

            contentItem: Text {
                text: loginButton.text
                color: "white"
                font.pixelSize: 16
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
            
        // 注册链接
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("还没有账号？")
            color: themeManager.currentTheme.textSecondaryColor
            font.pixelSize: 14
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("立即注册")
            flat: true
            onClicked: navigateToRegister()

            contentItem: Text {
                text: parent.text
                color: themeManager.currentTheme.primaryColor
                font.pixelSize: 14
                font.underline: true
            }
        }

        Item { Layout.fillHeight: true }
    }

    // 连接认证管理器信号
    Connections {
        target: authManager

        function onLoginSucceeded(user) {
            loginSucceeded()
        }

        function onLoginFailed(error) {
            errorText.text = error
            errorText.visible = true
        }
    }

    // 初始化
    Component.onCompleted: {
        usernameInput.focus = true

        // 检查sessionManager是否可用
        if (typeof sessionManager !== "undefined" && sessionManager !== null) {
            // 加载保存的登录信息
            if (sessionManager.isRememberMeEnabled) {
                var loginInfo = sessionManager.getSavedLoginInfo()
                if (loginInfo.first) {
                    usernameInput.text = loginInfo.first
                    rememberCheckBox.checked = true
                }
            }
        }
    }
}