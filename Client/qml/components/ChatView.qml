import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects

/**
 * @brief 聊天界面组件
 * 显示聊天消息列表和消息输入框
 */
Rectangle {
    id: root
    
    // 公共属性
    property var chatUser: ({})
    property alias messageModel: messageListView.model
    property bool isTyping: false
    
    // 信号
    signal sendMessage(string content, string type)
    signal loadMoreMessages()
    signal messageContextMenu(var messageInfo, point position)
    
    color: ThemeManager.backgroundColor
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // 聊天头部
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: ThemeManager.headerBackgroundColor
            border.color: ThemeManager.borderColor
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 12
                
                // 好友头像
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: 20
                    color: ThemeManager.avatarBackgroundColor
                    border.color: ThemeManager.borderColor
                    border.width: 1
                    
                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: chatUser.avatar_url || ""
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
                        text: chatUser.display_name ? chatUser.display_name.charAt(0).toUpperCase() : 
                              chatUser.username ? chatUser.username.charAt(0).toUpperCase() : "?"
                        color: ThemeManager.textColor
                        font.pixelSize: 16
                        font.bold: true
                    }
                }
                
                // 好友信息
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    
                    Text {
                        text: chatUser.display_name || chatUser.username || ""
                        color: ThemeManager.textColor
                        font.pixelSize: 16
                        font.bold: true
                    }
                    
                    Text {
                        text: {
                            if (root.isTyping) return qsTr("正在输入...")
                            switch (chatUser.online_status) {
                                case "online": return qsTr("在线")
                                case "away": return qsTr("离开")
                                case "busy": return qsTr("忙碌")
                                default: return qsTr("离线")
                            }
                        }
                        color: root.isTyping ? ThemeManager.accentColor : ThemeManager.secondaryTextColor
                        font.pixelSize: 12
                    }
                }
                
                // 更多操作按钮
                Button {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    text: "⋮"
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(ThemeManager.buttonColor, 1.2) : 
                               parent.hovered ? Qt.lighter(ThemeManager.buttonColor, 1.1) : "transparent"
                        radius: 15
                    }
                    
                    onClicked: chatOptionsMenu.open()
                    
                    Menu {
                        id: chatOptionsMenu
                        
                        MenuItem {
                            text: qsTr("查看资料")
                            onTriggered: {
                                // 显示用户资料
                            }
                        }
                        
                        MenuItem {
                            text: qsTr("聊天记录")
                            onTriggered: {
                                // 显示聊天记录
                            }
                        }
                        
                        MenuSeparator {}
                        
                        MenuItem {
                            text: qsTr("清空聊天记录")
                            onTriggered: {
                                // 清空聊天记录
                            }
                        }
                    }
                }
            }
        }
        
        // 消息列表
        ListView {
            id: messageListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            clip: true
            spacing: 8
            verticalLayoutDirection: ListView.BottomToTop
            
            // 自定义滚动条
            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
                
                background: Rectangle {
                    color: ThemeManager.scrollBarBackgroundColor
                    radius: 4
                }
                
                contentItem: Rectangle {
                    color: ThemeManager.scrollBarColor
                    radius: 4
                }
            }
            
            delegate: MessageBubble {
                width: messageListView.width
                messageInfo: model
                
                onContextMenu: root.messageContextMenu(messageInfo, position)
            }
            
            // 加载更多消息
            header: Rectangle {
                width: messageListView.width
                height: 40
                color: "transparent"
                visible: messageListView.count > 0
                
                Button {
                    anchors.centerIn: parent
                    text: qsTr("加载更多消息")
                    
                    onClicked: root.loadMoreMessages()
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(ThemeManager.buttonColor, 1.2) : 
                               parent.hovered ? Qt.lighter(ThemeManager.buttonColor, 1.1) : ThemeManager.buttonColor
                        border.color: ThemeManager.borderColor
                        border.width: 1
                        radius: 6
                    }
                }
            }
            
            // 空状态提示
            Label {
                anchors.centerIn: parent
                visible: messageListView.count === 0
                text: qsTr("暂无聊天记录，开始聊天吧！")
                color: ThemeManager.secondaryTextColor
                font.pixelSize: 16
            }
        }
        
        // 消息输入区域
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(60, messageInput.implicitHeight + 20)
            color: ThemeManager.inputAreaBackgroundColor
            border.color: ThemeManager.borderColor
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10
                
                // 表情按钮
                Button {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    text: "😊"
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(ThemeManager.buttonColor, 1.2) : 
                               parent.hovered ? Qt.lighter(ThemeManager.buttonColor, 1.1) : "transparent"
                        radius: 15
                    }
                    
                    onClicked: {
                        // 显示表情选择器
                    }
                }
                
                // 消息输入框
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    TextArea {
                        id: messageInput
                        placeholderText: qsTr("输入消息...")
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        
                        background: Rectangle {
                            color: ThemeManager.inputBackgroundColor
                            border.color: messageInput.activeFocus ? ThemeManager.accentColor : ThemeManager.borderColor
                            border.width: 1
                            radius: 6
                        }
                        
                        Keys.onPressed: {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (event.modifiers & Qt.ControlModifier) {
                                    // Ctrl+Enter 换行
                                    insert(cursorPosition, "\n")
                                } else {
                                    // Enter 发送消息
                                    sendCurrentMessage()
                                }
                                event.accepted = true
                            }
                        }
                    }
                }
                
                // 文件按钮
                Button {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    text: "📎"
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(ThemeManager.buttonColor, 1.2) : 
                               parent.hovered ? Qt.lighter(ThemeManager.buttonColor, 1.1) : "transparent"
                        radius: 15
                    }
                    
                    onClicked: {
                        // 选择文件发送
                    }
                }
                
                // 发送按钮
                Button {
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 30
                    text: qsTr("发送")
                    enabled: messageInput.text.trim().length > 0
                    
                    onClicked: sendCurrentMessage()
                    
                    background: Rectangle {
                        color: parent.enabled ? 
                               (parent.pressed ? Qt.darker(ThemeManager.accentColor, 1.2) : 
                                parent.hovered ? Qt.lighter(ThemeManager.accentColor, 1.1) : ThemeManager.accentColor) :
                               ThemeManager.disabledButtonColor
                        radius: 6
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : ThemeManager.disabledTextColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                    }
                }
            }
        }
    }
    
    // 发送当前消息
    function sendCurrentMessage() {
        var content = messageInput.text.trim()
        if (content.length > 0) {
            root.sendMessage(content, "text")
            messageInput.clear()
        }
    }
    
    // 添加消息到列表
    function addMessage(messageData) {
        messageModel.insert(0, messageData)
    }
    
    // 滚动到底部
    function scrollToBottom() {
        messageListView.positionViewAtBeginning()
    }
}
