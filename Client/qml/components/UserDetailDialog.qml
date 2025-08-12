import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 用户详细信息对话框
 * 
 * 显示用户的完整资料信息，包括头像、基本信息、个人简介等
 * 提供添加好友的操作按钮
 */
Dialog {
    id: root
    
    // 公共属性
    property var userInfo: ({})
    property var themeManager
    property var messageDialog
    
    // 信号
    signal addFriendClicked()
    
    title: qsTr("用户详情")
    modal: true
    anchors.centerIn: parent
    width: 400
    height: 500
    
    // 现代化背景样式
    background: Rectangle {
        color: themeManager.currentTheme.surfaceColor
        radius: 16
        border.color: Qt.rgba(themeManager.currentTheme.borderColor.r,
                              themeManager.currentTheme.borderColor.g,
                              themeManager.currentTheme.borderColor.b, 0.3)
        border.width: 1
        
        // 添加阴影效果
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 4
            anchors.leftMargin: 4
            color: Qt.rgba(0, 0, 0, 0.15)
            radius: parent.radius
            z: -1
        }
    }
    
    // 添加打开动画
    enter: Transition {
        NumberAnimation {
            property: "scale"
            from: 0.8
            to: 1.0
            duration: 200
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 200
        }
    }
    
    // 添加关闭动画
    exit: Transition {
        NumberAnimation {
            property: "scale"
            from: 1.0
            to: 0.8
            duration: 150
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 150
        }
    }
    
    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        clip: true
        
        ColumnLayout {
            width: parent.width
            spacing: 24
            
            // 用户头像和基本信息
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 16
                
                // 头像
                Rectangle {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 120
                    Layout.alignment: Qt.AlignHCenter
                    
                    color: themeManager.currentTheme.primaryColor
                    radius: 60
                    border.color: themeManager.currentTheme.successColor
                    border.width: 3
                    
                    Text {
                        anchors.centerIn: parent
                        text: userInfo.username ? userInfo.username.charAt(0).toUpperCase() : "U"
                        color: "white"
                        font.pixelSize: 48
                        font.weight: Font.Bold
                    }
                    
                    // 头像图片（如果有的话）
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: parent.radius - 3
                        visible: userInfo.avatar_url && typeof userInfo.avatar_url === "string" && userInfo.avatar_url.length > 0
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: (userInfo.avatar_url && typeof userInfo.avatar_url === "string") ? userInfo.avatar_url : ""
                            fillMode: Image.PreserveAspectCrop

                            onStatusChanged: {
                                if (status === Image.Error) {
                                    parent.visible = false
                                }
                            }
                        }
                    }
                }
                
                // 用户名和显示名
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Text {
                        text: userInfo.display_name || userInfo.username || qsTr("未知用户")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    Text {
                        text: userInfo.username ? qsTr("@%1").arg(userInfo.username) : ""
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 16
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                        visible: text.length > 0
                    }
                }
            }
            
            // 分割线
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: themeManager.currentTheme.borderColor
                opacity: 0.3
            }
            
            // 详细信息
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
                        font.weight: Font.Medium
                        Layout.preferredWidth: 80
                    }
                    
                    Text {
                        text: userInfo.user_id || qsTr("未知")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        Layout.fillWidth: true
                    }
                }
                
                // 好友状态
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: userInfo.friendship_status && typeof userInfo.friendship_status === "string" && userInfo.friendship_status !== "none"
                    
                    Text {
                        text: qsTr("好友状态:")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        Layout.preferredWidth: 80
                    }
                    
                    Rectangle {
                        Layout.preferredWidth: statusText.contentWidth + 16
                        Layout.preferredHeight: 24
                        color: {
                            switch(userInfo.friendship_status) {
                                case "friends": return themeManager.currentTheme.successColor
                                case "pending": return themeManager.currentTheme.warningColor
                                case "blocked": return themeManager.currentTheme.errorColor
                                default: return "transparent"
                            }
                        }
                        radius: 12
                        
                        Text {
                            id: statusText
                            anchors.centerIn: parent
                            text: {
                                switch(userInfo.friendship_status) {
                                    case "friends": return qsTr("已是好友")
                                    case "pending": return qsTr("好友请求待处理")
                                    case "blocked": return qsTr("已屏蔽")
                                    default: return ""
                                }
                            }
                            color: "white"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }
                }
                
                // 个人简介
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: userInfo.bio && typeof userInfo.bio === "string" && userInfo.bio.length > 0
                    
                    Text {
                        text: qsTr("个人简介:")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: bioText.contentHeight + 16
                        color: Qt.rgba(themeManager.currentTheme.borderColor.r,
                                       themeManager.currentTheme.borderColor.g,
                                       themeManager.currentTheme.borderColor.b, 0.1)
                        radius: 8
                        border.color: themeManager.currentTheme.borderColor
                        border.width: 1
                        
                        Text {
                            id: bioText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: userInfo.bio || ""
                            color: themeManager.currentTheme.textPrimaryColor
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignTop
                        }
                    }
                }
            }
        }
    }
    
    footer: Rectangle {
        color: "transparent"
        height: 80
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12
            
            // 弹性空间
            Item {
                Layout.fillWidth: true
            }
            
            // 关闭按钮
            Button {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 40
                text: qsTr("关闭")
                
                background: Rectangle {
                    color: parent.pressed ? Qt.rgba(themeManager.currentTheme.borderColor.r,
                                                   themeManager.currentTheme.borderColor.g,
                                                   themeManager.currentTheme.borderColor.b, 0.3) :
                           parent.hovered ? Qt.rgba(themeManager.currentTheme.borderColor.r,
                                                   themeManager.currentTheme.borderColor.g,
                                                   themeManager.currentTheme.borderColor.b, 0.1) : "transparent"
                    border.color: themeManager.currentTheme.borderColor
                    border.width: 1.5
                    radius: 8
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: root.reject()
            }
            
            // 添加好友按钮
            Button {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                text: {
                    switch(userInfo.friendship_status) {
                        case "friends": return qsTr("已是好友")
                        case "pending": return qsTr("已发送")
                        case "blocked": return qsTr("已屏蔽")
                        default: return qsTr("加为好友")
                    }
                }
                enabled: !userInfo.friendship_status || (typeof userInfo.friendship_status === "string" && userInfo.friendship_status === "none")
                
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
                    root.reject()
                }
            }
        }
    }
}
