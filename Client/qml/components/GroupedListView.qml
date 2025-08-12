import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 分组列表视图组件
 * 
 * 支持分组显示、拖拽、右键菜单等功能的通用列表组件
 */
Rectangle {
    id: root
    
    // 公共属性
    property var themeManager
    property var groupsModel: []  // 分组数据模型
    property string searchText: ""
    property bool allowDragDrop: true
    property string listType: "friends"  // friends, recent, groups
    
    // 信号
    signal itemClicked(var itemData)
    signal itemDoubleClicked(var itemData)
    signal itemContextMenu(var itemData, point position)
    signal itemDropped(var itemData, string targetGroupId)
    signal groupExpanded(string groupId, bool expanded)
    signal groupManageRequested(string action, var groupData)
    
    color: "transparent"
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4
        
        // 工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            // 搜索框
            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                placeholderText: {
                    switch(listType) {
                        case "friends": return qsTr("搜索好友...")
                        case "recent": return qsTr("搜索最近联系...")
                        case "groups": return qsTr("搜索群组...")
                        default: return qsTr("搜索...")
                    }
                }
                text: searchText
                font.pixelSize: 12
                
                background: Rectangle {
                    color: themeManager.currentTheme.inputBackgroundColor
                    border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor :
                                 parent.hovered ? Qt.lighter(themeManager.currentTheme.borderColor, 1.2) :
                                 themeManager.currentTheme.borderColor
                    border.width: parent.activeFocus ? 2 : 1
                    radius: 6
                    
                    Behavior on border.color {
                        ColorAnimation { duration: 200 }
                    }
                }
                
                leftPadding: 12
                rightPadding: 12
                
                onTextChanged: {
                    root.searchText = text
                }
            }
            
            // 添加分组按钮
            Button {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                           parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                           themeManager.currentTheme.primaryColor
                    radius: 6
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: "+"
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    groupManageRequested("create", {})
                }
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("创建新分组")
                ToolTip.delay: 1000
            }
        }
        
        // 分组列表
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            ListView {
                id: groupListView
                model: groupsModel
                spacing: 2
                
                delegate: Column {
                    width: groupListView.width
                    
                    // 分组头部
                    GroupHeader {
                        width: parent.width
                        groupName: modelData.name || ""
                        memberCount: modelData.members ? modelData.members.length : 0
                        isExpanded: modelData.expanded !== undefined ? modelData.expanded : true
                        isDefault: modelData.isDefault || false
                        themeManager: root.themeManager
                        
                        onExpandToggled: {
                            modelData.expanded = !modelData.expanded
                            groupExpanded(modelData.id, modelData.expanded)
                        }
                        
                        onRenameRequested: {
                            groupManageRequested("rename", modelData)
                        }
                        
                        onDeleteRequested: {
                            groupManageRequested("delete", modelData)
                        }
                        
                        onAddMemberRequested: {
                            groupManageRequested("addMember", modelData)
                        }
                    }
                    
                    // 分组成员列表
                    // 分组成员列表
                    Column {
                        width: parent.width
                        visible: modelData.expanded === true
                        
                        Repeater {
                            model: {
                                if (!modelData.members) return []
                                if (searchText.length === 0) return modelData.members
                                
                                // 过滤搜索结果
                                return modelData.members.filter(function(member) {
                                    var searchLower = searchText.toLowerCase()
                                    return (member.name && member.name.toLowerCase().includes(searchLower)) ||
                                           (member.username && member.username.toLowerCase().includes(searchLower)) ||
                                           (member.displayName && member.displayName.toLowerCase().includes(searchLower))
                                })
                            }
                            
                            delegate: Rectangle {
                                width: parent.width
                                height: 48
                                color: itemMouseArea.containsMouse ? 
                                       Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                               themeManager.currentTheme.primaryColor.g,
                                               themeManager.currentTheme.primaryColor.b, 0.1) : "transparent"
                                radius: 6
                                
                                // 拖拽支持
                                Drag.active: itemMouseArea.drag.active && allowDragDrop
                                Drag.source: modelData
                                Drag.mimeData: {
                                    "application/x-qkchat-item": JSON.stringify(modelData)
                                }
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 24  // 缩进显示层级
                                    anchors.rightMargin: 8
                                    spacing: 12
                                    
                                    // 头像
                                    Rectangle {
                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        Layout.alignment: Qt.AlignVCenter
                                        
                                        color: themeManager.currentTheme.primaryColor
                                        radius: 16
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.name ? modelData.name.charAt(0).toUpperCase() : "?"
                                            color: "white"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                        }
                                        
                                        // 头像图片
                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            radius: parent.radius - 2
                                            visible: modelData.avatar && modelData.avatar && modelData.avatar.toString && modelData.avatar.toString().length > 0
                                            clip: true

                                            Image {
                                                anchors.fill: parent
                                                source: (modelData.avatar && typeof modelData.avatar === "string") ? modelData.avatar : ""
                                                fillMode: Image.PreserveAspectCrop

                                                onStatusChanged: {
                                                    if (status === Image.Error) {
                                                        parent.visible = false
                                                    }
                                                }
                                            }
                                        }
                                        
                                        // 在线状态指示器
                                        Rectangle {
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: {
                                                switch(modelData.status) {
                                                    case "online": return themeManager.currentTheme.successColor
                                                    case "away": return themeManager.currentTheme.warningColor
                                                    case "busy": return themeManager.currentTheme.errorColor
                                                    default: return themeManager.currentTheme.borderColor
                                                }
                                            }
                                            border.color: themeManager.currentTheme.surfaceColor
                                            border.width: 2
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            anchors.rightMargin: -2
                                            anchors.bottomMargin: -2
                                            visible: listType === "friends" || listType === "recent"
                                        }
                                    }
                                    
                                    // 信息区域
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 2
                                        
                                        Text {
                                            text: modelData.displayName || modelData.name || modelData.username || qsTr("未知")
                                            color: themeManager.currentTheme.textPrimaryColor
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                        
                                        Text {
                                            text: {
                                                if (listType === "recent" && modelData.lastMessage) {
                                                    return modelData.lastMessage
                                                } else if (listType === "friends" && modelData.signature) {
                                                    return modelData.signature
                                                } else if (listType === "groups" && modelData.description) {
                                                    return modelData.description
                                                }
                                                return ""
                                            }
                                            color: themeManager.currentTheme.textSecondaryColor
                                            font.pixelSize: 11
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            visible: text.length > 0
                                        }
                                    }
                                    
                                    // 时间/未读消息
                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignTop
                                        spacing: 4
                                        visible: listType === "recent"
                                        
                                        Text {
                                            text: modelData.lastTime || ""
                                            color: themeManager.currentTheme.textSecondaryColor
                                            font.pixelSize: 10
                                            visible: text.length > 0
                                        }
                                        
                                        Rectangle {
                                            Layout.preferredWidth: Math.max(16, unreadText.contentWidth + 8)
                                            Layout.preferredHeight: 16
                                            color: themeManager.currentTheme.errorColor
                                            radius: 8
                                            visible: modelData.unreadCount > 0
                                            
                                            Text {
                                                id: unreadText
                                                anchors.centerIn: parent
                                                text: modelData.unreadCount > 99 ? "99+" : (modelData.unreadCount ? modelData.unreadCount.toString() : "0")
                                                color: "white"
                                                font.pixelSize: 9
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }
                                }
                                
                                MouseArea {
                                    id: itemMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    drag.target: allowDragDrop ? parent : null
                                    
                                    onClicked: function(mouse) {
                                        if (mouse.button === Qt.LeftButton) {
                                            itemClicked(modelData)
                                        } else if (mouse.button === Qt.RightButton) {
                                            itemContextMenu(modelData, Qt.point(mouse.x, mouse.y))
                                        }
                                    }
                                    
                                    onDoubleClicked: {
                                        itemDoubleClicked(modelData)
                                    }
                                }
                                
                                // 拖拽目标区域
                                DropArea {
                                    anchors.fill: parent
                                    enabled: allowDragDrop
                                    
                                    onDropped: function(drop) {
                                        if (drop.hasUrls || !drop.hasText) {
                                            drop.ignore()
                                            return
                                        }
                                        
                                        try {
                                            var draggedItem = JSON.parse(drop.getDataAsString("application/x-qkchat-item"))
                                            itemDropped(draggedItem, modelData.groupId || modelData.id)
                                            drop.accept()
                                        } catch (e) {
                                            drop.ignore()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
