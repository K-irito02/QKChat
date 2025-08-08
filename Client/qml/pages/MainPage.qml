import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 主页面组件
 * 
 * 用户登录成功后的主界面，显示用户信息和聊天功能。
 * 提供注销、设置等功能入口。
 */
Rectangle {
    id: mainPage
    
    // 公共属性
    property var themeManager
    property var loadingDialog
    property var messageDialog
    
    // 信号
    signal logout()
    
    color: themeManager.currentTheme.backgroundColor
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20
        
        // 标题栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 15
                
                // 应用标题
                Text {
                    Layout.fillWidth: true
                    text: "QKChat"
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    verticalAlignment: Text.AlignVCenter
                }
                
                // 主题切换按钮
                Button {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    
                    background: Rectangle {
                        color: parent.pressed ? themeManager.currentTheme.borderColor : "transparent"
                        radius: 20
                        
                        // 悬停效果
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: Text {
                        text: themeManager.isDarkTheme ? "☀" : "🌙"
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        themeManager.toggleTheme()
                    }
                }
                
                // 注销按钮
                Button {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    
                    background: Rectangle {
                        color: parent.pressed ? themeManager.currentTheme.errorColor : "transparent"
                        radius: 20
                        
                        // 悬停效果
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: Text {
                        text: "⏻"
                        color: themeManager.currentTheme.errorColor
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        logoutConfirmDialog.open()
                    }
                }
            }
        }
        
        // 用户信息卡片
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15
                
                // 用户头像
                Rectangle {
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 80
                    color: themeManager.currentTheme.primaryColor
                    radius: 40
                    
                    Text {
                        anchors.centerIn: parent
                        text: sessionManager.currentUser ? 
                              sessionManager.currentUser.username.charAt(0).toUpperCase() : "U"
                        color: "white"
                        font.pixelSize: 32
                        font.weight: Font.Bold
                    }
                }
                
                // 用户信息
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    
                    Text {
                        text: sessionManager.currentUser ? 
                              sessionManager.currentUser.username : "未知用户"
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 18
                        font.weight: Font.Medium
                    }
                    
                    Text {
                        text: sessionManager.currentUser ? 
                              sessionManager.currentUser.email : ""
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                    }
                    
                    RowLayout {
                        spacing: 8
                        
                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: themeManager.currentTheme.successColor
                        }
                        
                        Text {
                            text: "在线"
                            color: themeManager.currentTheme.successColor
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
        
        // 功能区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20
                
                Text {
                    text: "聊天功能"
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 20
                    font.weight: Font.Medium
                }
                
                Text {
                    Layout.fillWidth: true
                    text: "欢迎使用QKChat！\n\n聊天功能正在开发中，敬请期待。\n\n当前版本主要实现了用户认证系统，包括：\n• 用户注册和邮箱验证\n• 安全登录和会话管理\n• 主题切换功能\n• 记住密码功能"
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                }
                
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
    
    // 注销确认对话框
    Dialog {
        id: logoutConfirmDialog
        anchors.centerIn: parent
        modal: true
        title: "确认注销"
        
        background: Rectangle {
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
        }
        
        contentItem: Text {
            text: "确定要注销登录吗？"
            color: themeManager.currentTheme.textPrimaryColor
            font.pixelSize: 14
        }
        
        footer: DialogButtonBox {
            Button {
                text: "取消"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                
                background: Rectangle {
                    color: parent.pressed ? themeManager.currentTheme.borderColor : "transparent"
                    border.color: themeManager.currentTheme.borderColor
                    border.width: 1
                    radius: 6
                }
                
                contentItem: Text {
                    text: parent.text
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            Button {
                text: "注销"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                
                background: Rectangle {
                    color: parent.pressed ? 
                           Qt.darker(themeManager.currentTheme.errorColor, 1.1) : 
                           themeManager.currentTheme.errorColor
                    radius: 6
                }
                
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        
        onAccepted: {
            authManager.logout()
            mainPage.logout()
        }
    }
}
