import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../components" as Components

/**
 * @brief 登录窗口
 * 
 * 独立的登录/注册窗口，包含登录页面和注册页面的切换
 */
ApplicationWindow {
    id: loginWindow
    
    width: 450
    height: 600
    minimumWidth: 400
    minimumHeight: 500
    
    title: qsTr("QKChat - 登录")
    
    // 窗口居中显示
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    
    // 公共属性
    property var themeManager
    property var authManager
    property var sessionManager
    property var settingsManager
    
    // 信号
    signal loginSucceeded(var user)
    signal windowClosed()
    
    // 窗口关闭事件
    onClosing: {
        windowClosed()
    }
    
    // 主题管理器
    Components.ThemeManager {
        id: defaultThemeManager
    }
    
    // 如果没有外部传入themeManager，使用默认的
    property var currentThemeManager: themeManager || defaultThemeManager
    
    // 设置窗口背景色
    color: currentThemeManager.currentTheme.backgroundColor
    
    // 加载对话框
    Components.LoadingDialog {
        id: loadingDialog
        themeManager: currentThemeManager
        parent: Overlay.overlay
    }
    
    // 消息对话框
    Components.MessageDialog {
        id: messageDialog
        themeManager: currentThemeManager
        parent: Overlay.overlay
    }
    
    // 页面栈
    StackView {
        id: stackView
        anchors.fill: parent
        
        // 使用简单的淡入淡出动画
        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 200
                easing.type: Easing.OutQuad
            }
        }
        
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 150
                easing.type: Easing.InQuad
            }
        }
        
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 200
                easing.type: Easing.OutQuad
            }
        }
        
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 150
                easing.type: Easing.InQuad
            }
        }
    }
    
    // 登录页面组件
    Component {
        id: loginPageComponent
        
        Loader {
            source: "../LoginPage.qml"
            onLoaded: {
                // 设置属性和连接信号
                item.themeManager = currentThemeManager
                item.authManager = loginWindow.authManager
                item.sessionManager = loginWindow.sessionManager
                item.loadingDialog = loadingDialog
                item.messageDialog = messageDialog
                
                item.navigateToRegister.connect(function() {
                    stackView.push(registerPageComponent)
                })
                
                item.loginSucceeded.connect(function(user) {
                    loginWindow.loginSucceeded(user)
                })
            }
        }
    }
    
    // 注册页面组件
    Component {
        id: registerPageComponent
        
        Loader {
            source: "../RegisterPage.qml"
            onLoaded: {
                item.themeManager = currentThemeManager
                item.authManager = loginWindow.authManager
                item.loadingDialog = loadingDialog
                item.messageDialog = messageDialog
                
                item.navigateToLogin.connect(function() {
                    stackView.pop()
                })
                
                item.registerSucceeded.connect(function() {
                    stackView.pop()
                    messageDialog.showSuccess("注册成功", "账号创建成功，请登录")
                })
            }
        }
    }
    
    // 窗口初始化
    Component.onCompleted: {
        stackView.push(loginPageComponent)
    }
}
