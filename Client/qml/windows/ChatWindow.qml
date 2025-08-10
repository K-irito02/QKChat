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
    
    // 窗口居中显示
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    
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
    
    // 聊天主界面
    Loader {
        id: mainPageLoader
        anchors.fill: parent
        source: "../pages/MainPage.qml"

        onLoaded: {
            item.themeManager = currentThemeManager
            item.authManager = chatWindow.authManager
            item.sessionManager = chatWindow.sessionManager
            item.loadingDialog = loadingDialog
            item.messageDialog = messageDialog

            // 连接登出信号
            item.logout.connect(function() {
                chatWindow.logout()
            })
        }
    }

    // 注释掉原来的MainPage加载器，用于调试
    /*
    Loader {
        id: mainPageLoader
        anchors.fill: parent
        source: "../pages/MainPage.qml"

        onLoaded: {
            console.log("MainPage加载成功")
            try {
                console.log("设置MainPage属性...")
                item.themeManager = currentThemeManager
                item.authManager = chatWindow.authManager
                item.sessionManager = chatWindow.sessionManager
                item.loadingDialog = loadingDialog
                item.messageDialog = messageDialog
                console.log("MainPage属性设置完成")

                // 连接登出信号
                console.log("连接MainPage登出信号...")
                item.logout.connect(function() {
                    console.log("MainPage触发登出")
                    chatWindow.logout()
                })
                console.log("MainPage信号连接完成")
            } catch (error) {
                console.error("MainPage初始化出错:", error)
            }
        }

        onStatusChanged: {
            console.log("MainPage加载状态变化:", status)
            if (status === Loader.Error) {
                console.error("MainPage加载失败:", sourceComponent.errorString())
            } else if (status === Loader.Ready) {
                console.log("MainPage组件就绪")
            } else if (status === Loader.Loading) {
                console.log("MainPage正在加载...")
            }
        }

        // 监控MainPage的可见性
        onItemChanged: {
            if (item) {
                console.log("MainPage item创建成功")
            } else {
                console.log("MainPage item为null")
            }
        }
    }
    */
    
    // 窗口初始化
    Component.onCompleted: {
        // 确保窗口保持可见
        if (!visible) {
            visible = true
        }
    }
}
