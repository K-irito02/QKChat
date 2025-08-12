import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects

/**
 * @brief 消息气泡组件
 * 显示单条聊天消息，支持文本、图片、文件等类型
 */
Item {
    id: root
    
    // 公共属性
    property var messageInfo: ({})
    property bool isOwnMessage: messageInfo.is_own || false
    property bool showAvatar: true
    property bool showTime: true
    
    // 信号
    signal contextMenu(point position)
    
    height: messageContainer.height + 16
    
    RowLayout {
        id: messageContainer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 8
        
        layoutDirection: isOwnMessage ? Qt.RightToLeft : Qt.LeftToRight
        
        // 头像（对方消息显示）
        Rectangle {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignTop
            radius: 16
            color: ThemeManager.avatarBackgroundColor
            border.color: ThemeManager.borderColor
            border.width: 1
            visible: showAvatar && !isOwnMessage
            
            Image {
                anchors.fill: parent
                anchors.margins: 2
                source: messageInfo.sender_avatar || ""
                fillMode: Image.PreserveAspectCrop
                smooth: true
                visible: source != ""
                
                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: parent.width
                        height: parent.height
                        radius: width / 2
                    }
                }
            }
            
            Text {
                anchors.centerIn: parent
                visible: parent.children[0].source == ""
                text: messageInfo.sender_name ? messageInfo.sender_name.charAt(0).toUpperCase() : "?"
                color: ThemeManager.textColor
                font.pixelSize: 12
                font.bold: true
            }
        }
        
        // 消息内容区域
        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: parent.width * 0.7
            spacing: 4
            
            // 发送者名称（群聊中显示）
            Text {
                Layout.fillWidth: true
                text: messageInfo.sender_name || ""
                color: ThemeManager.secondaryTextColor
                font.pixelSize: 12
                visible: !isOwnMessage && messageInfo.show_sender_name
                elide: Text.ElideRight
            }
            
            // 消息气泡
            Rectangle {
                id: messageBubble
                Layout.fillWidth: true
                Layout.preferredHeight: messageContent.height + 16
                
                color: isOwnMessage ? ThemeManager.ownMessageBubbleColor : ThemeManager.otherMessageBubbleColor
                border.color: isOwnMessage ? ThemeManager.ownMessageBorderColor : ThemeManager.otherMessageBorderColor
                border.width: 1
                radius: 12
                
                // 消息内容
                Item {
                    id: messageContent
                    anchors.fill: parent
                    anchors.margins: 8
                    
                    // 文本消息
                    Text {
                        id: textContent
                        anchors.fill: parent
                        text: messageInfo.content || ""
                        color: isOwnMessage ? ThemeManager.ownMessageTextColor : ThemeManager.otherMessageTextColor
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                        visible: messageInfo.type === "text"
                        
                        // 支持链接点击
                        onLinkActivated: Qt.openUrlExternally(link)
                        
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.RightButton) {
                                    root.contextMenu(Qt.point(mouse.x, mouse.y))
                                }
                            }
                        }
                    }
                    
                    // 图片消息
                    Image {
                        id: imageContent
                        anchors.centerIn: parent
                        source: messageInfo.file_url || ""
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: messageInfo.type === "image"
                        
                        property real maxWidth: 200
                        property real maxHeight: 200
                        
                        width: Math.min(implicitWidth, maxWidth)
                        height: Math.min(implicitHeight, maxHeight)
                        
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    // 预览图片
                                } else if (mouse.button === Qt.RightButton) {
                                    root.contextMenu(Qt.point(mouse.x, mouse.y))
                                }
                            }
                        }
                        
                        // 加载指示器
                        BusyIndicator {
                            anchors.centerIn: parent
                            visible: imageContent.status === Image.Loading
                        }
                        
                        // 加载失败提示
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("图片加载失败")
                            color: ThemeManager.errorColor
                            visible: imageContent.status === Image.Error
                        }
                    }
                    
                    // 文件消息
                    Rectangle {
                        id: fileContent
                        anchors.fill: parent
                        color: "transparent"
                        visible: messageInfo.type === "file"
                        
                        RowLayout {
                            anchors.fill: parent
                            spacing: 8
                            
                            // 文件图标
                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                color: ThemeManager.fileIconBackgroundColor
                                radius: 8
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: "📄"
                                    font.pixelSize: 20
                                }
                            }
                            
                            // 文件信息
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                
                                Text {
                                    Layout.fillWidth: true
                                    text: messageInfo.file_name || qsTr("未知文件")
                                    color: isOwnMessage ? ThemeManager.ownMessageTextColor : ThemeManager.otherMessageTextColor
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }
                                
                                Text {
                                    text: formatFileSize(messageInfo.file_size || 0)
                                    color: isOwnMessage ? ThemeManager.ownMessageSecondaryTextColor : ThemeManager.otherMessageSecondaryTextColor
                                    font.pixelSize: 12
                                }
                            }
                            
                            // 下载按钮
                            Button {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                text: "⬇"
                                
                                onClicked: {
                                    // 下载文件
                                }
                                
                                background: Rectangle {
                                    color: parent.pressed ? Qt.darker(ThemeManager.accentColor, 1.2) : 
                                           parent.hovered ? Qt.lighter(ThemeManager.accentColor, 1.1) : ThemeManager.accentColor
                                    radius: 15
                                }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.RightButton) {
                                    root.contextMenu(Qt.point(mouse.x, mouse.y))
                                }
                            }
                        }
                    }
                }
                
                // 消息状态指示器（自己的消息）
                Row {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 4
                    spacing: 2
                    visible: isOwnMessage
                    
                    Text {
                        text: {
                            switch (messageInfo.status) {
                                case "sent": return "✓"
                                case "delivered": return "✓✓"
                                case "read": return "✓✓"
                                case "failed": return "✗"
                                default: return "○"
                            }
                        }
                        color: messageInfo.status === "read" ? ThemeManager.accentColor : 
                               messageInfo.status === "failed" ? ThemeManager.errorColor : ThemeManager.secondaryTextColor
                        font.pixelSize: 10
                    }
                }
            }
            
            // 时间戳
            Text {
                Layout.fillWidth: true
                text: formatMessageTime(messageInfo.created_at)
                color: ThemeManager.secondaryTextColor
                font.pixelSize: 10
                visible: showTime
                horizontalAlignment: isOwnMessage ? Text.AlignRight : Text.AlignLeft
            }
        }
        
        // 占位符（保持布局平衡）
        Item {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            visible: showAvatar && isOwnMessage
        }
    }
    
    // 格式化文件大小
    function formatFileSize(bytes) {
        if (bytes === 0) return "0 B"
        var k = 1024
        var sizes = ["B", "KB", "MB", "GB"]
        var i = Math.floor(Math.log(bytes) / Math.log(k))
        return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i]
    }
    
    // 格式化消息时间
    function formatMessageTime(timestamp) {
        if (!timestamp) return ""
        
        var messageTime = new Date(timestamp)
        var now = new Date()
        var diffMs = now - messageTime
        var diffMins = Math.floor(diffMs / 60000)
        var diffHours = Math.floor(diffMins / 60)
        var diffDays = Math.floor(diffHours / 24)
        
        if (diffMins < 1) return qsTr("刚刚")
        if (diffMins < 60) return qsTr("%1分钟前").arg(diffMins)
        if (diffHours < 24) return messageTime.toLocaleTimeString(Qt.locale(), "hh:mm")
        if (diffDays < 7) return messageTime.toLocaleDateString(Qt.locale(), "MM-dd hh:mm")
        return messageTime.toLocaleDateString(Qt.locale(), "yyyy-MM-dd hh:mm")
    }
}
