import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects

/**
 * @brief 好友列表项组件
 * 显示单个好友的信息，包括头像、昵称、在线状态等
 */
Rectangle {
    id: root
    
    // 公共属性
    property var friendInfo: ({})
    property bool isSelected: false
    property bool isHovered: false
    
    // 信号
    signal clicked()
    signal doubleClicked()
    signal rightClicked(point position)
    
    height: 60
    color: isSelected ? ThemeManager.selectedItemColor : 
           isHovered ? ThemeManager.hoveredItemColor : "transparent"
    
    // 鼠标区域
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        onEntered: root.isHovered = true
        onExited: root.isHovered = false
        
        onClicked: {
            if (mouse.button === Qt.LeftButton) {
                root.clicked()
            } else if (mouse.button === Qt.RightButton) {
                root.rightClicked(Qt.point(mouse.x, mouse.y))
            }
        }
        
        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                root.doubleClicked()
            }
        }
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12
        
        // 头像和在线状态指示器
        Item {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            
            // 头像
            Rectangle {
                id: avatarContainer
                anchors.fill: parent
                radius: 20
                color: ThemeManager.avatarBackgroundColor
                border.color: ThemeManager.borderColor
                border.width: 1
                
                Image {
                    id: avatarImage
                    anchors.fill: parent
                    anchors.margins: 2
                    source: friendInfo.avatar_url || ""
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    visible: source != ""
                    
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            width: avatarImage.width
                            height: avatarImage.height
                            radius: width / 2
                        }
                    }
                }
                
                // 默认头像文字
                Text {
                    anchors.centerIn: parent
                    visible: avatarImage.source == ""
                    text: friendInfo.display_name ? friendInfo.display_name.charAt(0).toUpperCase() : 
                          friendInfo.username ? friendInfo.username.charAt(0).toUpperCase() : "?"
                    color: ThemeManager.textColor
                    font.pixelSize: 16
                    font.bold: true
                }
            }
            
            // 在线状态指示器
            Rectangle {
                id: statusIndicator
                width: 12
                height: 12
                radius: 6
                anchors.right: avatarContainer.right
                anchors.bottom: avatarContainer.bottom
                anchors.rightMargin: -2
                anchors.bottomMargin: -2
                
                color: {
                    switch (friendInfo.online_status) {
                        case "online": return "#4CAF50"  // 绿色
                        case "away": return "#FF9800"    // 橙色
                        case "busy": return "#F44336"    // 红色
                        case "invisible": return "#9E9E9E" // 灰色
                        default: return "#9E9E9E"        // 离线 - 灰色
                    }
                }
                
                border.color: ThemeManager.backgroundColor
                border.width: 2
                
                // 在线状态动画
                SequentialAnimation {
                    running: friendInfo.online_status === "online"
                    loops: Animation.Infinite
                    
                    PropertyAnimation {
                        target: statusIndicator
                        property: "scale"
                        from: 1.0
                        to: 1.2
                        duration: 1000
                        easing.type: Easing.InOutQuad
                    }
                    
                    PropertyAnimation {
                        target: statusIndicator
                        property: "scale"
                        from: 1.2
                        to: 1.0
                        duration: 1000
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }
        
        // 好友信息
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2
            
            // 显示名称
            Text {
                id: displayNameText
                Layout.fillWidth: true
                text: friendInfo.display_name || friendInfo.username || ""
                color: ThemeManager.textColor
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
                maximumLineCount: 1
            }
            
            // 在线状态文本和最后在线时间
            Text {
                id: statusText
                Layout.fillWidth: true
                text: {
                    switch (friendInfo.online_status) {
                        case "online": return qsTr("在线")
                        case "away": return qsTr("离开")
                        case "busy": return qsTr("忙碌")
                        case "invisible": return qsTr("隐身")
                        default: {
                            if (friendInfo.last_seen) {
                                var lastSeen = new Date(friendInfo.last_seen)
                                var now = new Date()
                                var diffMs = now - lastSeen
                                var diffMins = Math.floor(diffMs / 60000)
                                var diffHours = Math.floor(diffMins / 60)
                                var diffDays = Math.floor(diffHours / 24)
                                
                                if (diffMins < 1) return qsTr("刚刚在线")
                                if (diffMins < 60) return qsTr("%1分钟前在线").arg(diffMins)
                                if (diffHours < 24) return qsTr("%1小时前在线").arg(diffHours)
                                if (diffDays < 7) return qsTr("%1天前在线").arg(diffDays)
                                return qsTr("很久未在线")
                            }
                            return qsTr("离线")
                        }
                    }
                }
                color: ThemeManager.secondaryTextColor
                font.pixelSize: 12
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
        
        // 未读消息数量指示器
        Rectangle {
            id: unreadBadge
            Layout.preferredWidth: Math.max(20, unreadText.implicitWidth + 8)
            Layout.preferredHeight: 20
            radius: 10
            color: ThemeManager.accentColor
            visible: friendInfo.unread_count > 0
            
            Text {
                id: unreadText
                anchors.centerIn: parent
                text: friendInfo.unread_count > 99 ? "99+" : friendInfo.unread_count.toString()
                color: "white"
                font.pixelSize: 10
                font.bold: true
            }
        }
    }
    
    // 分隔线
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 62 // 头像宽度 + 间距
        height: 1
        color: ThemeManager.separatorColor
        opacity: 0.3
    }
    
    // 工具提示
    ToolTip {
        id: toolTip
        visible: mouseArea.containsMouse && friendInfo.note
        text: qsTr("备注: %1").arg(friendInfo.note || "")
        delay: 1000
        timeout: 5000
    }
}
