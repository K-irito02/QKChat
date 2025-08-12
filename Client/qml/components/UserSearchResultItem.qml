import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 用户搜索结果项组件
 * 
 * 显示单个用户的基本信息，包括头像、用户名、用户ID等
 * 提供查看详情和添加好友的操作按钮
 */
Rectangle {
    id: root
    
    // 公共属性
    property var userInfo: ({})
    property var themeManager
    property bool isHovered: false
    
    // 信号
    signal viewDetailsClicked()
    signal addFriendClicked()
    
    height: 80
    color: isHovered ? Qt.rgba(themeManager.currentTheme.primaryColor.r,
                               themeManager.currentTheme.primaryColor.g,
                               themeManager.currentTheme.primaryColor.b, 0.05) : "transparent"
    radius: 8
    border.color: isHovered ? Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                      themeManager.currentTheme.primaryColor.g,
                                      themeManager.currentTheme.primaryColor.b, 0.2) : "transparent"
    border.width: 1
    
    Behavior on color {
        ColorAnimation { duration: 150 }
    }
    
    Behavior on border.color {
        ColorAnimation { duration: 150 }
    }
    
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        
        onEntered: isHovered = true
        onExited: isHovered = false
        onClicked: viewDetailsClicked()
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        
        // 用户头像
        Rectangle {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            Layout.alignment: Qt.AlignVCenter
            
            color: themeManager.currentTheme.primaryColor
            radius: 28
            border.color: themeManager.currentTheme.successColor
            border.width: 2
            
            Text {
                anchors.centerIn: parent
                text: userInfo.username ? userInfo.username.charAt(0).toUpperCase() : "U"
                color: "white"
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            
            // 头像图片（如果有的话）
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: parent.radius - 2
                visible: userInfo.avatar_url && userInfo.avatar_url.toString().length > 0
                clip: true

                Image {
                    anchors.fill: parent
                    source: userInfo.avatar_url || ""
                    fillMode: Image.PreserveAspectCrop

                    onStatusChanged: {
                        if (status === Image.Error) {
                            parent.visible = false
                        }
                    }
                }
            }
        }
        
        // 用户信息
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 4
            
            // 用户名和显示名
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: userInfo.display_name || userInfo.username || qsTr("未知用户")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                
                // 好友状态标识
                Rectangle {
                    Layout.preferredWidth: statusText.contentWidth + 12
                    Layout.preferredHeight: 20
                    color: {
                        switch(userInfo.friendship_status) {
                            case "friends": return themeManager.currentTheme.successColor
                            case "pending": return themeManager.currentTheme.warningColor
                            case "blocked": return themeManager.currentTheme.errorColor
                            default: return "transparent"
                        }
                    }
                    radius: 10
                    visible: userInfo.friendship_status && userInfo.friendship_status !== "none"
                    
                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        text: {
                            switch(userInfo.friendship_status) {
                                case "friends": return qsTr("已是好友")
                                case "pending": return qsTr("待处理")
                                case "blocked": return qsTr("已屏蔽")
                                default: return ""
                            }
                        }
                        color: "white"
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }
                }
            }
            
            // 用户ID和用户名
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                
                Text {
                    text: qsTr("ID: %1").arg(userInfo.user_id || "")
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 12
                }
                
                Text {
                    text: userInfo.username ? qsTr("@%1").arg(userInfo.username) : ""
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 12
                    visible: text.length > 0
                }
            }
        }
        
        // 操作按钮区域
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 8
            
            // 查看详情按钮
            Button {
                Layout.preferredWidth: 60
                Layout.preferredHeight: 32
                text: qsTr("详情")
                
                background: Rectangle {
                    color: parent.pressed ? Qt.rgba(themeManager.currentTheme.borderColor.r,
                                                   themeManager.currentTheme.borderColor.g,
                                                   themeManager.currentTheme.borderColor.b, 0.3) :
                           parent.hovered ? Qt.rgba(themeManager.currentTheme.borderColor.r,
                                                   themeManager.currentTheme.borderColor.g,
                                                   themeManager.currentTheme.borderColor.b, 0.1) : "transparent"
                    border.color: themeManager.currentTheme.borderColor
                    border.width: 1
                    radius: 6
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    viewDetailsClicked()
                }
            }
            
            // 添加好友按钮
            Button {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 32
                text: {
                    switch(userInfo.friendship_status) {
                        case "friends": return qsTr("已添加")
                        case "pending": return qsTr("已发送")
                        case "blocked": return qsTr("已屏蔽")
                        default: return qsTr("加好友")
                    }
                }
                enabled: !userInfo.friendship_status || userInfo.friendship_status === "none"
                
                background: Rectangle {
                    color: parent.enabled ? 
                           (parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                            parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                            themeManager.currentTheme.primaryColor) :
                           Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                   themeManager.currentTheme.primaryColor.g,
                                   themeManager.currentTheme.primaryColor.b, 0.3)
                    radius: 6
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "white" : Qt.rgba(255, 255, 255, 0.6)
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    addFriendClicked()
                }
            }
        }
    }
}
