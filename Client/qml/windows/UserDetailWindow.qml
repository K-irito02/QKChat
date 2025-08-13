import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

/**
 * @brief 用户详细信息窗口
 * 
 * 显示用户的完整资料信息，包括头像、基本信息、个人简介等
 * 提供添加好友的操作按钮
 */
Window {
    id: root
    
    // 公共属性
    property var userInfo: ({})
    property var themeManager
    property var messageDialog
    
    // 信号
    signal addFriendClicked()
    signal windowClosed()
    
    title: qsTr("用户详情")
    width: 450
    height: 600
    minimumWidth: 400
    minimumHeight: 500
    visible: false
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    modality: Qt.NonModal
    
    Component.onCompleted: {
        // UserDetailWindow component completed
    }
    
    // 窗口关闭事件
    onClosing: {
        windowClosed()
    }
    
    // 窗口可见性变化事件 - 子组件方式，Qt自动处理层级
    onVisibleChanged: {
        // 子组件方式，Qt自动处理层级，无需手动管理
    }
    
    // 窗口激活状态变化事件 - 子组件方式，Qt自动处理层级
    onActiveChanged: {
        // 子组件方式，Qt自动处理层级，无需手动管理
    }
    
    // 设置窗口背景色
    color: themeManager.currentTheme.backgroundColor
    
    // 主内容容器
    Rectangle {
        anchors.fill: parent
        color: themeManager.currentTheme.backgroundColor
        
        ScrollView {
            anchors.fill: parent
            anchors.margins: 20
            clip: true
        
        ColumnLayout {
            width: parent.width
            spacing: 24
            
            // 用户头像区域
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 16
                
                // 头像容器
                Rectangle {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 120
                    radius: 60
                    color: themeManager.currentTheme.primaryColor
                    border.color: "#4CAF50"
                    border.width: 3
                    
                    // 头像图片（如果有的话）
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: parent.radius - 3
                        visible: userInfo && userInfo.avatar_url && typeof userInfo.avatar_url === "string" && userInfo.avatar_url.length > 0
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: (userInfo && userInfo.avatar_url && typeof userInfo.avatar_url === "string") ? userInfo.avatar_url : ""
                            fillMode: Image.PreserveAspectCrop

                            onStatusChanged: {
                                if (status === Image.Error) {
                                    parent.visible = false
                                }
                            }
                        }
                    }
                    
                    // 默认头像文字
                    Text {
                        anchors.centerIn: parent
                        text: (userInfo && (userInfo.display_name || userInfo.username) ? (userInfo.display_name || userInfo.username) : "U").charAt(0).toUpperCase()
                        color: "white"
                        font.pixelSize: 48
                        font.weight: Font.Bold
                        visible: !(userInfo && userInfo.avatar_url && typeof userInfo.avatar_url === "string" && userInfo.avatar_url.length > 0)
                    }
                }
                
                // 用户名
                Text {
                    text: userInfo && (userInfo.display_name || userInfo.username) ? (userInfo.display_name || userInfo.username) : qsTr("未知用户")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    Layout.alignment: Qt.AlignHCenter
                }
                
                // 用户名标识符
                Text {
                                            text: "@" + (userInfo && userInfo.username ? userInfo.username : "")
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 16
                    Layout.alignment: Qt.AlignHCenter
                }
            }
            
            // 用户信息区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 16
                
                // 用户ID
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    
                    Text {
                        text: qsTr("用户ID:")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        Layout.preferredWidth: 80
                    }
                    
                    Text {
                        text: userInfo && (userInfo.user_id || userInfo.id) ? (userInfo.user_id || userInfo.id) : qsTr("未知")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.family: "Consolas, Monaco, monospace"
                        Layout.fillWidth: true
                    }
                }
                
                // 个人简介
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Text {
                        text: qsTr("个人简介:")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        color: "transparent"
                        border.color: themeManager.currentTheme.borderColor
                        border.width: 1
                        radius: 8
                        
                        Text {
                            anchors.fill: parent
                            anchors.margins: 12
                            text: userInfo && (userInfo.bio || userInfo.description) ? (userInfo.bio || userInfo.description) : qsTr("该用户还没有设置个人简介")
                            color: userInfo && (userInfo.bio || userInfo.description) ? 
                                   themeManager.currentTheme.textPrimaryColor : 
                                   themeManager.currentTheme.textSecondaryColor
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignTop
                        }
                    }
                }
                
                // 注册时间（如果有的话）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: userInfo && (userInfo.created_at || userInfo.registration_date)
                    
                    Text {
                        text: qsTr("注册时间:")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        Layout.preferredWidth: 80
                    }
                    
                    Text {
                        text: userInfo && (userInfo.created_at || userInfo.registration_date) ? (userInfo.created_at || userInfo.registration_date) : ""
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                }
                
                // 在线状态（如果有的话）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: userInfo && typeof userInfo.online_status !== "undefined"
                    
                    Text {
                        text: qsTr("在线状态:")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        Layout.preferredWidth: 80
                    }
                    
                    Rectangle {
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 12
                        radius: 6
                        color: userInfo && userInfo.online_status === "online" ? "#4CAF50" : "#9E9E9E"
                    }
                    
                    Text {
                        text: userInfo && userInfo.online_status === "online" ? qsTr("在线") : qsTr("离线")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                }
            }
            
            // 操作按钮区域
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 16
                
                // 关闭按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 40
                    text: qsTr("关闭")
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(themeManager.currentTheme.surfaceColor, 1.1) :
                               parent.hovered ? Qt.lighter(themeManager.currentTheme.surfaceColor, 1.05) :
                               themeManager.currentTheme.surfaceColor
                        radius: 8
                        border.color: themeManager.currentTheme.borderColor
                        border.width: 1
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        root.close()
                    }
                }
                
                // 添加好友按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 40
                    text: {
                        switch(userInfo && userInfo.friendship_status ? userInfo.friendship_status : "") {
                            case "friends": return qsTr("已是好友")
                            case "pending": return qsTr("已发送")
                            case "blocked": return qsTr("已屏蔽")
                            default: return qsTr("加为好友")
                        }
                    }
                    enabled: !userInfo || !userInfo.friendship_status || (typeof userInfo.friendship_status === "string" && userInfo.friendship_status === "none")
                    
                    background: Rectangle {
                        color: parent.enabled ? 
                               (parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                                parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                                themeManager.currentTheme.primaryColor) :
                               Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                       themeManager.currentTheme.primaryColor.g,
                                       themeManager.currentTheme.primaryColor.b, 0.3)
                        radius: 8
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : Qt.rgba(255, 255, 255, 0.6)
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        addFriendClicked()
                        // 不立即关闭窗口，让调用者决定何时关闭
                    }
                }
            }
        }
        }
    }
    
    // 窗口显示函数
    function showUserDetail(user) {
        userInfo = user || {}
        show()
        // 子组件方式，Qt自动处理层级，无需手动管理
    }
}
