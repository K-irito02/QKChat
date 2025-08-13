import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../components" as Components

/**
 * @brief 聊天窗口
 * 
 * 独立的聊天主界面窗口，包含完整的聊天功能
 */
ApplicationWindow {
    id: chatWindow
    
    width: 1440
    height: 900
    minimumWidth: 800
    minimumHeight: 600
    visible: true

    title: qsTr("QKChat")
    
    // 公共属性
    property var themeManager
    property var authManager
    property var sessionManager
    property var settingsManager
    
    // 信号
    signal logout()
    signal windowClosed()
    
    // 窗口关闭事件
    onClosing: function(close) {
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
    
    // 聊天主界面 - 使用MainPage
    Loader {
        id: mainPageLoader
        anchors.fill: parent
        source: "../pages/MainPage.qml"

        onLoaded: {
            try {
                // 设置MainPage属性
                item.themeManager = currentThemeManager
                item.authManager = chatWindow.authManager
                item.sessionManager = chatWindow.sessionManager
                item.loadingDialog = loadingDialog
                item.messageDialog = messageDialog

                // 连接登出信号
                item.logout.connect(function() {
                    chatWindow.logout()
                })
            } catch (error) {
                // MainPage initialization error
            }
        }

        onStatusChanged: {
            if (status === Loader.Error) {
                // MainPage loading failed
            }
        }
    }


    
    // 窗口初始化
    Component.onCompleted: {
        // 确保窗口保持可见
        if (!visible) {
            visible = true
        }
    }
}
