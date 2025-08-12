import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

/**
 * @brief 聊天主界面
 *
 * 用户登录成功后的聊天界面，包含左侧导航栏和右侧聊天区域。
 * 实现了联系人管理、消息展示和发送等核心聊天功能。
 * 支持可拖拽分割线、导航栏收缩等高级功能。
 */
Rectangle {
    id: mainPage

    // 公共属性
    property var themeManager
    property var authManager
    property var sessionManager
    property var loadingDialog
    property var messageDialog

    // 导航栏状态
    property bool isNavCollapsed: false
    property int defaultCollapsedWidth: 80
    property int defaultExpandedWidth: 250
    property int navWidth: isNavCollapsed ? defaultCollapsedWidth : defaultExpandedWidth
    property int minNavWidth: 80
    property int maxNavWidth: 400

    // 分割线位置
    property real verticalSplitPosition: 0.3  // 左侧占比
    property real horizontalSplitPosition: 0.7  // 上方占比

    // 当前导航分类
    property string currentNavCategory: "recent"
    property string currentBottomCategory: "addFriend"

    // 好友管理相关属性
    property var currentChatUser: null
    property bool showFriendList: true

    // 信号
    signal logout()

    // 监听navWidth变化，确保状态同步
    onNavWidthChanged: {
        // 防止递归更新，只在必要时更新状态
        if (Math.abs(navWidth - (isNavCollapsed ? defaultCollapsedWidth : defaultExpandedWidth)) > 30) {
            if (navWidth <= (defaultCollapsedWidth + 20) && !isNavCollapsed) {
                isNavCollapsed = true
            }
            else if (navWidth >= (defaultExpandedWidth - 20) && isNavCollapsed) {
                isNavCollapsed = false
            }
        }
    }

    color: themeManager.currentTheme.backgroundColor

    // 主布局：左侧导航栏 + 垂直分割线 + 右侧聊天区域
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧导航栏
        Rectangle {
            id: leftPanel
            Layout.preferredWidth: navWidth
            Layout.fillHeight: true
            color: themeManager.currentTheme.surfaceColor
            border.color: themeManager.currentTheme.borderColor
            border.width: 1

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 用户信息区域
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60  // 与右侧聊天头部高度保持一致
                    color: themeManager.currentTheme.backgroundColor
                    border.color: themeManager.currentTheme.borderColor
                    border.width: 1

                    // 收缩模式下的布局
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        visible: isNavCollapsed

                        // 头像（居中）
                        Rectangle {
                            id: collapsedAvatar
                            anchors.centerIn: parent
                            width: 36
                            height: 36
                            color: themeManager.currentTheme.primaryColor
                            radius: 18
                            border.color: themeManager.currentTheme.successColor
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: sessionManager && sessionManager.currentUser ?
                                      sessionManager.currentUser.username.charAt(0).toUpperCase() : "U"
                                color: "white"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }

                        // 展开按钮（头像右侧，垂直居中对齐）
                        Button {
                            anchors.left: collapsedAvatar.right
                            anchors.leftMargin: 4
                            anchors.verticalCenter: collapsedAvatar.verticalCenter
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            width: Math.min(20, parent.width - collapsedAvatar.width - 12)
                            height: 20
                            visible: width > 10  // 只有在有足够空间时才显示

                            background: Rectangle {
                                color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                radius: 10

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "▶"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 8
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                isNavCollapsed = false
                                navWidth = defaultExpandedWidth
                                leftPanel.Layout.preferredWidth = defaultExpandedWidth
                            }

                            ToolTip.visible: hovered
                            ToolTip.text: "展开导航栏"
                            ToolTip.delay: 500
                        }
                    }

                    // 正常模式下的均匀分布布局
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4
                        visible: !isNavCollapsed

                        // 1. 用户头像
                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            Layout.maximumWidth: 40
                            color: themeManager.currentTheme.primaryColor
                            radius: 18
                            border.color: themeManager.currentTheme.successColor
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: sessionManager && sessionManager.currentUser ?
                                      sessionManager.currentUser.username.charAt(0).toUpperCase() : "U"
                                color: "white"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                        }

                        // 2. 用户昵称（在线状态）
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.maximumWidth: 80
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: sessionManager.currentUser ?
                                      sessionManager.currentUser.username : "用户"
                                color: themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 3

                                Rectangle {
                                    Layout.preferredWidth: 4
                                    Layout.preferredHeight: 4
                                    radius: 2
                                    color: themeManager.currentTheme.successColor
                                }

                                Text {
                                    text: "在线"
                                    color: themeManager.currentTheme.successColor
                                    font.pixelSize: 9
                                }
                            }
                        }

                        // 3. 主题切换按钮
                        Button {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.maximumWidth: 36

                            background: Rectangle {
                                color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                radius: 16

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: themeManager.isDarkTheme ? "🌙" : "☀"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: themeManager.toggleTheme()

                            ToolTip.visible: hovered
                            ToolTip.text: themeManager.isDarkTheme ? "浅色" : "深色"
                            ToolTip.delay: 500
                        }

                        // 4. 设置按钮
                        Button {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.maximumWidth: 36

                            background: Rectangle {
                                color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                radius: 16

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "⚙"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: settingsDialog.open()

                            ToolTip.visible: hovered
                            ToolTip.text: "设置"
                            ToolTip.delay: 500
                        }

                        // 5. 收缩按钮
                        Button {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.maximumWidth: 36

                            background: Rectangle {
                                color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                radius: 16

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "◀"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                isNavCollapsed = true
                                navWidth = defaultCollapsedWidth
                                leftPanel.Layout.preferredWidth = defaultCollapsedWidth
                            }

                            ToolTip.visible: hovered
                            ToolTip.text: "收缩"
                            ToolTip.delay: 500
                        }
                    }
                }

                // 导航分类
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    color: "transparent"

                    // 正常模式的导航分类
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 0
                        visible: !isNavCollapsed

                        // 最近联系
                        Button {
                            id: recentButton
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30

                            property bool isActive: true

                            background: Rectangle {
                                color: parent.isActive ? themeManager.currentTheme.primaryColor :
                                       (parent.hovered ? themeManager.currentTheme.borderColor : "transparent")
                                radius: 6

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "最近联系"
                                color: parent.isActive ? "white" : themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 13
                                font.weight: parent.isActive ? Font.Medium : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                recentButton.isActive = true
                                friendsButton.isActive = false
                                groupsButton.isActive = false
                                currentNavCategory = "recent"
                            }
                        }

                        // 好友
                        Button {
                            id: friendsButton
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30

                            property bool isActive: false

                            background: Rectangle {
                                color: parent.isActive ? themeManager.currentTheme.primaryColor :
                                       (parent.hovered ? themeManager.currentTheme.borderColor : "transparent")
                                radius: 6

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "好友"
                                color: parent.isActive ? "white" : themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 13
                                font.weight: parent.isActive ? Font.Medium : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                recentButton.isActive = false
                                friendsButton.isActive = true
                                groupsButton.isActive = false
                                currentNavCategory = "friends"
                            }
                        }

                        // 群组
                        Button {
                            id: groupsButton
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30

                            property bool isActive: false

                            background: Rectangle {
                                color: parent.isActive ? themeManager.currentTheme.primaryColor :
                                       (parent.hovered ? themeManager.currentTheme.borderColor : "transparent")
                                radius: 6

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "群组"
                                color: parent.isActive ? "white" : themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 13
                                font.weight: parent.isActive ? Font.Medium : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                recentButton.isActive = false
                                friendsButton.isActive = false
                                groupsButton.isActive = true
                                currentNavCategory = "groups"
                            }
                        }
                    }

                    // 收缩模式的导航分类自定义按钮
                    Components.CustomDropdownButton {
                        id: collapsedNavButton
                        anchors.centerIn: parent
                        visible: isNavCollapsed
                        themeManager: mainPage.themeManager
                        isCollapsed: true
                        currentText: {
                            switch(currentNavCategory) {
                                case "recent": return "最近联系"
                                case "friends": return "好友"
                                case "groups": return "群组"
                                default: return "最近联系"
                            }
                        }
                        menuItems: ["最近联系", "好友", "群组"]
                        buttonType: "navigation"

                        onItemClicked: function(itemName) {
                            // 处理左侧点击事件
                            switch(itemName) {
                                case "最近联系":
                                    currentNavCategory = "recent"
                                    recentButton.isActive = true
                                    friendsButton.isActive = false
                                    groupsButton.isActive = false
                                    break
                                case "好友":
                                    currentNavCategory = "friends"
                                    recentButton.isActive = false
                                    friendsButton.isActive = true
                                    groupsButton.isActive = false
                                    break
                                case "群组":
                                    currentNavCategory = "groups"
                                    recentButton.isActive = false
                                    friendsButton.isActive = false
                                    groupsButton.isActive = true
                                    break
                            }
                        }

                        onMenuItemSelected: function(itemName) {
                            // 处理菜单选择事件
                            switch(itemName) {
                                case "最近联系":
                                    currentNavCategory = "recent"
                                    recentButton.isActive = true
                                    friendsButton.isActive = false
                                    groupsButton.isActive = false
                                    break
                                case "好友":
                                    currentNavCategory = "friends"
                                    recentButton.isActive = false
                                    friendsButton.isActive = true
                                    groupsButton.isActive = false
                                    break
                                case "群组":
                                    currentNavCategory = "groups"
                                    recentButton.isActive = false
                                    friendsButton.isActive = false
                                    groupsButton.isActive = true
                                    break
                            }
                        }
                    }
                }

                // 联系人列表 - 使用StackLayout根据导航分类显示不同内容
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"

                    StackLayout {
                        anchors.fill: parent
                        visible: !isNavCollapsed
                        currentIndex: {
                            switch(currentNavCategory) {
                                case "recent": return 0
                                case "friends": return 1
                                case "groups": return 2
                                default: return 0
                            }
                        }

                        // 最近联系人列表
                        Rectangle {
                            color: "transparent"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                // 分组最近联系人列表
                                Components.GroupedListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    themeManager: mainPage.themeManager
                                    groupsModel: FriendGroupManager.recentContacts
                                    listType: "recent"
                                    allowDragDrop: false  // 最近联系不支持拖拽

                                    onItemClicked: function(itemData) {
                                        // Recent contact clicked
                                        // 打开聊天窗口
                                    }

                                    onItemDoubleClicked: function(itemData) {
                                        // Recent contact double clicked
                                        // 快速发送消息
                                    }

                                    onItemContextMenu: function(itemData, position) {
                                        recentContextMenu.contactData = itemData
                                        recentContextMenu.popup()
                                    }

                                    onGroupExpanded: function(groupId, expanded) {
                                        // TODO: 保存最近联系分组展开状态
                                    }

                                    onGroupManageRequested: function(action, groupData) {
                                        handleRecentContactManagement(action, groupData)
                                    }
                                }

                                // 空状态提示
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    visible: FriendGroupManager.recentContacts.length === 0
                                    text: qsTr("暂无最近联系")
                                    color: themeManager.currentTheme.textSecondaryColor
                                    font.pixelSize: 16
                                }
                            }
                        }

                        // 好友列表
                        Rectangle {
                            color: "transparent"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                // 分组好友列表
                                Components.GroupedListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    themeManager: mainPage.themeManager
                                    groupsModel: FriendGroupManager.friendGroups
                                    listType: "friends"
                                    allowDragDrop: true

                                    onItemClicked: function(itemData) {
                                        // Friend clicked
                                        // 打开聊天窗口
                                    }

                                    onItemDoubleClicked: function(itemData) {
                                        // Friend double clicked
                                        // 快速发送消息
                                    }

                                    onItemContextMenu: function(itemData, position) {
                                        groupedFriendContextMenu.friendData = itemData
                                        groupedFriendContextMenu.popup()
                                    }

                                    onItemDropped: function(itemData, targetGroupId) {
                                        // Moving friend to group
                                        FriendGroupManager.moveFriendToGroup(itemData.id, parseInt(targetGroupId))
                                    }

                                    onGroupExpanded: function(groupId, expanded) {
                                        FriendGroupManager.expandGroup(groupId, expanded)
                                    }

                                    onGroupManageRequested: function(action, groupData) {
                                        handleGroupManagement(action, groupData)
                                    }
                                }

                                // 空状态提示
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    visible: FriendGroupManager.friendGroups.length === 0
                                    text: qsTr("暂无好友")
                                    color: themeManager.currentTheme.textSecondaryColor
                                    font.pixelSize: 16
                                }
                            }
                        }

                        // 群组列表
                        Rectangle {
                            color: "transparent"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                // 分组群组列表
                                Components.GroupedListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    themeManager: mainPage.themeManager
                                    groupsModel: FriendGroupManager.chatGroups
                                    listType: "groups"
                                    allowDragDrop: true

                                    onItemClicked: function(itemData) {
                                        // Group clicked
                                        // 打开群聊窗口
                                    }

                                    onItemDoubleClicked: function(itemData) {
                                        // Group double clicked
                                        // 快速进入群聊
                                    }

                                    onItemContextMenu: function(itemData, position) {
                                        groupContextMenu.groupData = itemData
                                        groupContextMenu.popup()
                                    }

                                    onItemDropped: function(itemData, targetGroupId) {
                                        // Moving group to category
                                        // TODO: 实现群组分类移动
                                    }

                                    onGroupExpanded: function(groupId, expanded) {
                                        // TODO: 保存群组分类展开状态
                                    }

                                    onGroupManageRequested: function(action, groupData) {
                                        handleGroupCategoryManagement(action, groupData)
                                    }
                                }

                                // 空状态提示
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    visible: FriendGroupManager.chatGroups.length === 0
                                    text: qsTr("暂无群组")
                                    color: themeManager.currentTheme.textSecondaryColor
                                    font.pixelSize: 16
                                }
                            }
                        }
                    }

                    // 收缩模式下的简化好友列表
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "transparent"
                        visible: isNavCollapsed

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 2

                            ListView {
                                id: collapsedContactsList
                                model: currentNavCategory === "friends" ? friendsModel :
                                       currentNavCategory === "groups" ? groupsModel : recentContactsModel
                                delegate: collapsedContactDelegate
                                spacing: 4
                            }
                        }
                    }
                }

                // 底部功能按钮
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    color: themeManager.currentTheme.borderColor

                    // 正常模式的底部按钮
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        visible: !isNavCollapsed

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30

                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                                               themeManager.currentTheme.primaryColor.g,
                                                               themeManager.currentTheme.primaryColor.b, 0.1) : "transparent"
                                radius: 6
                                border.color: themeManager.currentTheme.primaryColor
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "添加好友"
                                color: themeManager.currentTheme.primaryColor
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                currentBottomCategory = "addFriend"
                                userSearchDialog.show()
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30

                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                                               themeManager.currentTheme.primaryColor.g,
                                                               themeManager.currentTheme.primaryColor.b, 0.1) : "transparent"
                                radius: 6
                                border.color: themeManager.currentTheme.primaryColor
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "创建群组"
                                color: themeManager.currentTheme.primaryColor
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                currentBottomCategory = "createGroup"
                                messageDialog.showInfo("功能开发中", "创建群组功能正在开发中")
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30

                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                                               themeManager.currentTheme.primaryColor.g,
                                                               themeManager.currentTheme.primaryColor.b, 0.1) : "transparent"
                                radius: 6
                                border.color: themeManager.currentTheme.primaryColor
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "添加群"
                                color: themeManager.currentTheme.primaryColor
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                currentBottomCategory = "addGroup"
                                messageDialog.showInfo("功能开发中", "添加群功能正在开发中")
                            }
                        }
                    }

                    // 收缩模式的底部自定义按钮
                    Components.CustomDropdownButton {
                        id: collapsedBottomButton
                        anchors.centerIn: parent
                        visible: isNavCollapsed
                        themeManager: mainPage.themeManager
                        isCollapsed: true
                        currentText: {
                            switch(currentBottomCategory) {
                                case "addFriend": return "添加好友"
                                case "createGroup": return "创建群组"
                                case "addGroup": return "添加群"
                                default: return "添加好友"
                            }
                        }
                        menuItems: ["添加好友", "创建群组", "添加群"]
                        buttonType: "bottom"

                        onItemClicked: function(itemName) {
                            switch(itemName) {
                                case "添加好友":
                                    currentBottomCategory = "addFriend"
                                    userSearchDialog.show()
                                    break
                                case "创建群组":
                                    currentBottomCategory = "createGroup"
                                    messageDialog.showInfo("功能开发中", "创建群组功能正在开发中")
                                    break
                                case "添加群":
                                    currentBottomCategory = "addGroup"
                                    messageDialog.showInfo("功能开发中", "添加群功能正在开发中")
                                    break
                            }
                        }

                        onMenuItemSelected: function(itemName) {
                            switch(itemName) {
                                case "添加好友":
                                    currentBottomCategory = "addFriend"
                                    userSearchDialog.show()
                                    break
                                case "创建群组":
                                    currentBottomCategory = "createGroup"
                                    messageDialog.showInfo("功能开发中", "创建群组功能正在开发中")
                                    break
                                case "添加群":
                                    currentBottomCategory = "addGroup"
                                    messageDialog.showInfo("功能开发中", "添加群功能正在开发中")
                                    break
                            }
                        }
                    }
                }
            }
        }

        // 垂直分割线
        Rectangle {
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            color: themeManager.currentTheme.borderColor

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor

                property real startX: 0
                property real startWidth: 0

                onPressed: function(mouse) {
                    startX = mouse.x
                    startWidth = leftPanel.Layout.preferredWidth
                }

                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var deltaX = mouse.x - startX
                        var newWidth = Math.max(minNavWidth, Math.min(maxNavWidth, startWidth + deltaX))

                        // 防止频繁更新，只在变化超过阈值时更新
                        if (Math.abs(newWidth - leftPanel.Layout.preferredWidth) > 5) {
                            leftPanel.Layout.preferredWidth = newWidth
                            navWidth = newWidth

                            // 同步收缩状态，使用防抖逻辑
                            var shouldCollapse = newWidth <= (defaultCollapsedWidth + 20)
                            var shouldExpand = newWidth >= (defaultExpandedWidth - 20)
                            
                            if (shouldCollapse && !isNavCollapsed) {
                                isNavCollapsed = true
                            } else if (shouldExpand && isNavCollapsed) {
                                isNavCollapsed = false
                            }
                        }
                    }
                }
            }
        }

        // 右侧聊天区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: themeManager.currentTheme.backgroundColor
            border.color: themeManager.currentTheme.borderColor
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 聊天头部
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: themeManager.currentTheme.surfaceColor
                    border.color: themeManager.currentTheme.borderColor
                    border.width: 1

                    // 使用绝对定位确保精准对齐
                    Item {
                        anchors.fill: parent
                        anchors.margins: 15

                        // 联系人头像 - 左侧对齐
                        Rectangle {
                            id: chatAvatar
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 40
                            height: 40
                            color: themeManager.currentTheme.secondaryColor
                            radius: 20

                            Text {
                                anchors.centerIn: parent
                                text: "产"
                                color: "white"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                            }
                        }

                        // 联系人信息 - 水平居中
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                text: "产品设计小组"
                                color: themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                text: "5名成员"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        // 聊天设置按钮 - 右侧对齐
                        Button {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32
                            height: 32

                            background: Rectangle {
                                color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                radius: 16

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "⚙"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                messageDialog.showInfo("功能开发中", "聊天设置功能正在开发中")
                            }

                            ToolTip.visible: hovered
                            ToolTip.text: "聊天设置"
                            ToolTip.delay: 500
                        }
                    }
                }

                // 消息展示区域
                Rectangle {
                    id: messagesArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: parent.height * horizontalSplitPosition
                    color: themeManager.currentTheme.backgroundColor

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.bottomMargin: 15  // 增加底部间距

                        ListView {
                            id: messagesList
                            model: messagesModel
                            delegate: messageDelegate
                            spacing: 8
                            // 修正消息显示顺序：最新消息在底部
                            verticalLayoutDirection: ListView.TopToBottom

                            // 模拟消息数据 - 按时间顺序排列
                            Component.onCompleted: {
                                // 检查是否已经有数据，避免重复添加
                                if (messagesModel.count === 0) {
                                    messagesModel.append({
                                        "sender": "王芳",
                                        "content": "设计方案的修改已经发送给大家了，大家看看有没有问题",
                                        "time": "10:15",
                                        "isOwn": false,
                                        "avatar": "王"
                                    })
                                    messagesModel.append({
                                        "sender": "戴雷",
                                        "content": "我同意上面的观点，能够更好地满足用户需求。大家有什么其他想法吗？",
                                        "time": "10:22",
                                        "isOwn": false,
                                        "avatar": "戴"
                                    })
                                    messagesModel.append({
                                        "sender": "我",
                                        "content": "我同意了，这个布局确实非常棒！大家主要做的很棒的",
                                        "time": "10:28",
                                        "isOwn": true,
                                        "avatar": sessionManager && sessionManager.currentUser ?
                                                 sessionManager.currentUser.username.charAt(0).toUpperCase() : "我"
                                    })
                                    messagesModel.append({
                                        "sender": "赵雷",
                                        "content": "我觉得这个方案很好，能够更好地满足用户需求。大家有什么其他想法吗？",
                                        "time": "10:38",
                                        "isOwn": false,
                                        "avatar": "赵"
                                    })

                                    // 自动滚动到底部显示最新消息
                                    Qt.callLater(function() {
                                        messagesList.positionViewAtEnd()
                                    })
                                }
                            }
                        }
                    }
                }

                // 水平分割线
                Rectangle {
                    id: horizontalSplitter
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    color: themeManager.currentTheme.borderColor

                    // 添加视觉指示器
                    Rectangle {
                        anchors.centerIn: parent
                        width: 40
                        height: 2
                        radius: 1
                        color: themeManager.currentTheme.textTertiaryColor
                        opacity: parent.parent.hovered ? 0.8 : 0.4

                        Behavior on opacity {
                            NumberAnimation { duration: 150 }
                        }
                    }

                    MouseArea {
                        id: splitterMouseArea
                        anchors.fill: parent
                        anchors.topMargin: -2
                        anchors.bottomMargin: -2
                        cursorShape: Qt.SizeVerCursor
                        hoverEnabled: true

                        property real startY: 0
                        property real startSplitPosition: 0
                        property bool isDragging: false

                        onPressed: function(mouse) {
                            startY = mouse.y
                            startSplitPosition = horizontalSplitPosition
                            isDragging = true
                        }

                        onPositionChanged: function(mouse) {
                            if (pressed && isDragging) {
                                var deltaY = mouse.y - startY
                                var parentHeight = messagesArea.parent.height
                                var splitterHeight = horizontalSplitter.Layout.preferredHeight
                                var inputMinHeight = 120  // 输入区域最小高度
                                var messagesMinHeight = 200  // 消息区域最小高度

                                // 计算可用高度
                                var availableHeight = parentHeight - splitterHeight

                                // 计算新的分割位置
                                var deltaRatio = deltaY / availableHeight
                                var newSplitPosition = startSplitPosition + deltaRatio

                                // 限制分割位置在合理范围内
                                var minSplitPosition = messagesMinHeight / availableHeight
                                var maxSplitPosition = (availableHeight - inputMinHeight) / availableHeight

                                newSplitPosition = Math.max(minSplitPosition, Math.min(maxSplitPosition, newSplitPosition))

                                // 防止频繁更新，只在变化超过阈值时更新
                                if (Math.abs(newSplitPosition - horizontalSplitPosition) > 0.01) {
                                    horizontalSplitPosition = newSplitPosition
                                    // 不直接设置preferredHeight，让布局系统自动计算
                                }
                            }
                        }

                        onReleased: {
                            isDragging = false
                        }
                    }
                }

                // 消息输入区域
                Rectangle {
                    id: inputArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 120
                    color: themeManager.currentTheme.surfaceColor
                    border.color: themeManager.currentTheme.borderColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // 工具栏
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            spacing: 8

                            Button {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30

                                background: Rectangle {
                                    color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                    radius: 15
                                }

                                contentItem: Text {
                                    text: "📎"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    messageDialog.showInfo("功能开发中", "文件发送功能正在开发中")
                                }

                                ToolTip.visible: hovered
                                ToolTip.text: "发送文件"
                                ToolTip.delay: 500
                            }

                            Button {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30

                                background: Rectangle {
                                    color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                    radius: 15
                                }

                                contentItem: Text {
                                    text: "🖼"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    messageDialog.showInfo("功能开发中", "图片发送功能正在开发中")
                                }

                                ToolTip.visible: hovered
                                ToolTip.text: "发送图片"
                                ToolTip.delay: 500
                            }

                            Button {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30

                                background: Rectangle {
                                    color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                    radius: 15
                                }

                                contentItem: Text {
                                    text: "😊"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    messageDialog.showInfo("功能开发中", "表情功能正在开发中")
                                }

                                ToolTip.visible: hovered
                                ToolTip.text: "表情"
                                ToolTip.delay: 500
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "按Enter发送，Shift+Enter换行"
                                color: themeManager.currentTheme.textTertiaryColor
                                font.pixelSize: 11
                            }
                        }

                        // 输入框和发送按钮
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 10

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                TextArea {
                                    id: messageInput
                                    placeholderText: "输入消息..."
                                    wrapMode: TextArea.Wrap
                                    selectByMouse: true

                                    color: themeManager.currentTheme.textPrimaryColor
                                    font.pixelSize: 14

                                    background: Rectangle {
                                        color: themeManager.currentTheme.inputBackgroundColor
                                        border.color: parent.activeFocus ?
                                                     themeManager.currentTheme.inputFocusColor :
                                                     themeManager.currentTheme.inputBorderColor
                                        border.width: 1
                                        radius: 8

                                        Behavior on border.color {
                                            ColorAnimation { duration: 150 }
                                        }
                                    }

                                    Keys.onPressed: function(event) {
                                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                            if (event.modifiers & Qt.ShiftModifier) {
                                                // Shift+Enter: 换行
                                                return
                                            } else {
                                                // Enter: 发送消息
                                                event.accepted = true
                                                sendMessage()
                                            }
                                        }
                                    }
                                }
                            }

                            Button {
                                Layout.preferredWidth: 60
                                Layout.preferredHeight: 40
                                enabled: messageInput.text.trim().length > 0

                                background: Rectangle {
                                    color: parent.enabled ?
                                           (parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.1) :
                                            themeManager.currentTheme.primaryColor) :
                                           themeManager.currentTheme.buttonDisabledColor
                                    radius: 8

                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }
                                }

                                contentItem: Text {
                                    text: "发送"
                                    color: "white"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: sendMessage()
                            }
                        }
                    }
                }
            }
        }
    }

    // 数据模型
    ListModel {
        id: contactsModel  // 保留原有模型以兼容现有代码
    }

    ListModel {
        id: recentContactsModel  // 最近联系人
    }

    ListModel {
        id: friendsModel  // 好友列表
    }

    ListModel {
        id: groupsModel  // 群组列表
    }

    ListModel {
        id: messagesModel
    }

    // 好友列表项委托
    Component {
        id: friendDelegate

        Rectangle {
            width: parent ? parent.width : 200
            height: 60
            color: friendMouseArea.containsMouse ? themeManager.currentTheme.borderColor : "transparent"
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                // 头像
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    color: themeManager.currentTheme.primaryColor
                    radius: 20
                    border.color: model.isOnline ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: model.display_name ? model.display_name.charAt(0).toUpperCase() :
                              model.username ? model.username.charAt(0).toUpperCase() : "?"
                        color: "white"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }

                    // 在线状态指示器
                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: -2
                        anchors.bottomMargin: -2
                        color: model.isOnline ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                        border.color: themeManager.currentTheme.backgroundColor
                        border.width: 2
                    }
                }

                // 好友信息
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: model.display_name || model.username || ""
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    Text {
                        text: model.isOnline ? qsTr("在线") : qsTr("离线")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                // 未读消息数量
                Rectangle {
                    Layout.preferredWidth: Math.max(20, unreadText.implicitWidth + 8)
                    Layout.preferredHeight: 20
                    radius: 10
                    color: themeManager.currentTheme.primaryColor
                    visible: model.unreadCount > 0

                    Text {
                        id: unreadText
                        anchors.centerIn: parent
                        text: model.unreadCount > 99 ? "99+" : model.unreadCount.toString()
                        color: "white"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }
            }

            MouseArea {
                id: friendMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton) {
                        mainPage.currentChatUser = model
                        // Friend selected
                    } else if (mouse.button === Qt.RightButton) {
                        friendContextMenu.friendInfo = model
                        friendContextMenu.popup()
                    }
                }

                onDoubleClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton) {
                        mainPage.currentChatUser = model
                        // Friend double clicked
                    }
                }
            }
        }
    }

    // 联系人列表项委托
    Component {
        id: contactDelegate

        Rectangle {
            width: contactsList.width
            height: 60
            color: mouseArea.containsMouse ? themeManager.currentTheme.borderColor : "transparent"
            radius: 8

            Behavior on color {
                ColorAnimation { duration: 150 }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                anchors.rightMargin: 5  // 减小右边距，让内容更靠近分割线
                spacing: 12

                // 头像（最左侧）
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    color: model.isGroup ? themeManager.currentTheme.secondaryColor : themeManager.currentTheme.primaryColor
                    radius: 20
                    border.color: model.isOnline ? themeManager.currentTheme.successColor : "transparent"
                    border.width: model.isOnline ? 2 : 0

                    Text {
                        anchors.centerIn: parent
                        text: model.name.charAt(0)
                        color: "white"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                }

                // 消息内容（中间）
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: model.lastMessage
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        visible: model.lastMessage !== ""
                    }

                    Text {
                        text: model.time
                        color: themeManager.currentTheme.textTertiaryColor
                        font.pixelSize: 11
                        visible: model.time !== ""
                    }
                }

                // 右侧区域（紧贴右边缘）
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight
                    Layout.preferredWidth: 120
                    Layout.maximumWidth: 120
                    spacing: 2

                    // 名称（最右侧）
                    Text {
                        Layout.fillWidth: true
                        text: model.name
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }

                    // 状态和未读消息行
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignRight
                        spacing: 4

                        // 弹性空间，推动内容到右侧
                        Item {
                            Layout.fillWidth: true
                        }

                        // 状态文本
                        Text {
                            text: model.isGroup ?
                                  (model.onlineCount ? model.onlineCount + "人在线" : "群组") :
                                  (model.isOnline ? "在线" : "离线")
                            color: model.isGroup ?
                                   themeManager.currentTheme.textSecondaryColor :
                                   (model.isOnline ? themeManager.currentTheme.successColor : themeManager.currentTheme.textTertiaryColor)
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignRight
                        }

                        // 未读消息数
                        Rectangle {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            color: themeManager.currentTheme.errorColor
                            radius: 9
                            visible: model.unreadCount > 0

                            Text {
                                anchors.centerIn: parent
                                text: model.unreadCount > 99 ? "99+" : model.unreadCount
                                color: "white"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    // TODO: 切换到选中的联系人聊天
                    console.log("Selected contact:", model.name)
                }
            }
        }
    }

    // 消息列表项委托
    Component {
        id: messageDelegate

        Rectangle {
            width: messagesList.width
            height: messageContent.height + 30
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12
                layoutDirection: model.isOwn ? Qt.RightToLeft : Qt.LeftToRight

                // 头像
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    Layout.alignment: Qt.AlignTop
                    color: model.isOwn ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.secondaryColor
                    radius: 18

                    Text {
                        anchors.centerIn: parent
                        text: model.avatar || (model.isOwn ? "我" : "U")
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }

                // 消息内容区域
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: messagesList.width * 0.65
                    spacing: 6

                    // 发送者和时间信息
                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: model.isOwn ? Qt.RightToLeft : Qt.LeftToRight
                        spacing: 8

                        Text {
                            text: model.sender || "未知用户"
                            color: themeManager.currentTheme.textSecondaryColor
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            Layout.alignment: model.isOwn ? Qt.AlignRight : Qt.AlignLeft
                        }

                        Text {
                            text: model.time || "00:00"
                            color: themeManager.currentTheme.textTertiaryColor
                            font.pixelSize: 11
                            Layout.alignment: model.isOwn ? Qt.AlignRight : Qt.AlignLeft
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    // 消息气泡
                    Rectangle {
                        id: messageContent
                        Layout.fillWidth: true
                        Layout.alignment: model.isOwn ? Qt.AlignRight : Qt.AlignLeft
                        implicitHeight: messageText.implicitHeight + 20
                        implicitWidth: Math.min(messageText.implicitWidth + 20, messagesList.width * 0.65)
                        color: model.isOwn ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.surfaceColor
                        radius: 12
                        border.color: model.isOwn ? "transparent" : themeManager.currentTheme.borderColor
                        border.width: model.isOwn ? 0 : 1

                        // 添加阴影效果
                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 2
                            anchors.leftMargin: 2
                            color: themeManager.currentTheme.shadowColor
                            radius: parent.radius
                            z: -1
                            opacity: 0.1
                        }

                        Text {
                            id: messageText
                            anchors.fill: parent
                            anchors.margins: 10
                            text: model.content || ""
                            color: model.isOwn ? "white" : themeManager.currentTheme.textPrimaryColor
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
                }

                // 占位符，确保消息不会占满整个宽度
                Item {
                    Layout.fillWidth: true
                    Layout.minimumWidth: messagesList.width * 0.2
                }
            }
        }
    }

    // 发送消息函数
    function sendMessage() {
        var messageText = messageInput.text.trim()
        if (messageText.length === 0) {
            return
        }

        var currentTime = new Date()
        var timeString = currentTime.getHours().toString().padStart(2, '0') + ":" +
                        currentTime.getMinutes().toString().padStart(2, '0')

        // 添加到消息列表末尾（最新消息在底部）
        messagesModel.append({
            "sender": "我",
            "content": messageText,
            "time": timeString,
            "isOwn": true,
            "avatar": sessionManager && sessionManager.currentUser ?
                     sessionManager.currentUser.username.charAt(0).toUpperCase() : "我"
        })

        messageInput.text = ""

        // 自动滚动到底部显示新消息
        Qt.callLater(function() {
            messagesList.positionViewAtEnd()
        })

        // TODO: 发送消息到服务器
        // Sending message
    }

    // 收缩模式的联系人委托
    Component {
        id: collapsedContactDelegate

        Rectangle {
            width: collapsedContactsList.width
            height: 56
            color: "transparent"

            // 头像容器，确保完全居中且不被裁切
            Rectangle {
                anchors.centerIn: parent
                width: 36
                height: 36
                color: model.isGroup ? themeManager.currentTheme.secondaryColor : themeManager.currentTheme.primaryColor
                radius: 18
                border.color: model.isOnline ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: model.name.charAt(0)
                    color: "white"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                // 在线状态指示器（小圆点）
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: -2
                    anchors.bottomMargin: -2
                    color: model.isOnline ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                    border.color: themeManager.currentTheme.backgroundColor
                    border.width: 1
                    visible: !model.isGroup
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    // TODO: 选择联系人
                    console.log("Selected contact:", model.name)
                }

                Rectangle {
                    anchors.fill: parent
                    color: parent.containsMouse ? themeManager.currentTheme.borderColor : "transparent"
                    radius: 6
                    opacity: 0.3

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
            }
        }
    }



    // 好友右键菜单
    Menu {
        id: friendContextMenu

        property var friendInfo: ({})

        MenuItem {
            text: qsTr("发送消息")
            onTriggered: {
                mainPage.currentChatUser = friendContextMenu.friendInfo
                // TODO: 切换到聊天界面
                // Sending message to friend
            }
        }

        MenuItem {
            text: qsTr("查看资料")
            onTriggered: {
                messageDialog.showInfo("功能开发中", "查看资料功能正在开发中")
            }
        }

        MenuItem {
            text: qsTr("修改备注")
            onTriggered: {
                editNoteDialog.friendInfo = friendContextMenu.friendInfo
                editNoteDialog.open()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("删除好友")
            onTriggered: {
                deleteFriendDialog.friendInfo = friendContextMenu.friendInfo
                deleteFriendDialog.open()
            }
        }

        MenuItem {
            text: qsTr("屏蔽用户")
            onTriggered: {
                messageDialog.showInfo("功能开发中", "屏蔽用户功能正在开发中")
            }
        }
    }

    // 添加好友对话框
    Dialog {
        id: addFriendDialog

        title: qsTr("添加好友")
        modal: true
        anchors.centerIn: parent
        width: 450
        height: 350

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

            // 添加内部高光效果
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                color: "transparent"
                radius: parent.radius - 1
                border.color: Qt.rgba(255, 255, 255, 0.1)
                border.width: 1
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

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 20

            // 标题区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("添加好友")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 20
                    font.weight: Font.Bold
                }

                Text {
                    text: qsTr("请输入好友的用户名、邮箱或用户ID")
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            // 输入区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 16

                TextField {
                    id: userIdentifierField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    placeholderText: qsTr("用户名/邮箱/用户ID")
                    font.pixelSize: 14

                    background: Rectangle {
                        color: themeManager.currentTheme.inputBackgroundColor
                        border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor :
                                     parent.hovered ? Qt.lighter(themeManager.currentTheme.borderColor, 1.2) :
                                     themeManager.currentTheme.borderColor
                        border.width: parent.activeFocus ? 2 : 1
                        radius: 8

                        Behavior on border.color {
                            ColorAnimation { duration: 200 }
                        }
                        Behavior on border.width {
                            NumberAnimation { duration: 200 }
                        }
                    }

                    leftPadding: 16
                    rightPadding: 16
                    topPadding: 12
                    bottomPadding: 12
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: qsTr("附加消息（可选）")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80

                        TextArea {
                            id: messageField
                            placeholderText: qsTr("我是...")
                            wrapMode: TextArea.Wrap
                            font.pixelSize: 14

                            background: Rectangle {
                                color: themeManager.currentTheme.inputBackgroundColor
                                border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor :
                                             parent.hovered ? Qt.lighter(themeManager.currentTheme.borderColor, 1.2) :
                                             themeManager.currentTheme.borderColor
                                border.width: parent.activeFocus ? 2 : 1
                                radius: 8

                                Behavior on border.color {
                                    ColorAnimation { duration: 200 }
                                }
                                Behavior on border.width {
                                    NumberAnimation { duration: 200 }
                                }
                            }

                            leftPadding: 16
                            rightPadding: 16
                            topPadding: 12
                            bottomPadding: 12
                        }
                    }
                }
            }

            // 弹性空间
            Item {
                Layout.fillHeight: true
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

                // 取消按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 44
                    text: "取消"

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

                    onClicked: addFriendDialog.reject()
                }

                // 添加按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 44
                    text: "添加"

                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                               parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                               themeManager.currentTheme.primaryColor
                        radius: 8

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }

                        // 添加微妙的渐变效果
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: Qt.rgba(255, 255, 255, 0.1) }
                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.1) }
                            }
                        }
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: addFriendDialog.accept()
                }
            }
        }

        onAccepted: {
            if (userIdentifierField.text.trim().length > 0) {
                messageDialog.showInfo("功能开发中", "添加好友功能正在开发中")
                userIdentifierField.clear()
                messageField.clear()
            }
        }

        onRejected: {
            userIdentifierField.clear()
            messageField.clear()
        }
    }

    // 修改备注对话框
    Dialog {
        id: editNoteDialog

        property var friendInfo: ({})

        title: qsTr("修改好友备注")
        modal: true
        anchors.centerIn: parent
        width: 300
        height: 150

        background: Rectangle {
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
        }

        TextField {
            id: noteField
            anchors.fill: parent
            anchors.margins: 20
            placeholderText: qsTr("输入备注名称")
            text: editNoteDialog.friendInfo.note || ""

            background: Rectangle {
                color: themeManager.currentTheme.inputBackgroundColor
                border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.borderColor
                border.width: 1
                radius: 6
            }
        }

        footer: DialogButtonBox {
            Button {
                text: "取消"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: "确定"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            messageDialog.showInfo("功能开发中", "修改备注功能正在开发中")
        }
    }

    // 删除好友确认对话框
    Dialog {
        id: deleteFriendDialog

        property var friendInfo: ({})

        title: qsTr("删除好友")
        modal: true
        anchors.centerIn: parent
        width: 300
        height: 150

        background: Rectangle {
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
        }

        Label {
            anchors.fill: parent
            anchors.margins: 20
            text: qsTr("确定要删除好友 \"%1\" 吗？").arg(deleteFriendDialog.friendInfo.display_name || deleteFriendDialog.friendInfo.username || "")
            color: themeManager.currentTheme.textPrimaryColor
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        footer: DialogButtonBox {
            Button {
                text: "取消"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: "删除"
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
            messageDialog.showInfo("功能开发中", "删除好友功能正在开发中")
        }
    }

    // 设置对话框
    Dialog {
        id: settingsDialog
        anchors.centerIn: parent
        modal: true
        title: "设置"
        width: 480
        height: 360

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

            // 添加内部高光效果
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                color: "transparent"
                radius: parent.radius - 1
                border.color: Qt.rgba(255, 255, 255, 0.1)
                border.width: 1
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

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 24

            // 标题区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "应用设置"
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 20
                    font.weight: Font.Bold
                }

                Text {
                    text: "管理您的应用偏好设置"
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                }
            }

            // 设置项区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 16

                // 主题设置项
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                   themeManager.currentTheme.primaryColor.g,
                                   themeManager.currentTheme.primaryColor.b, 0.05)
                    radius: 12
                    border.color: Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                          themeManager.currentTheme.primaryColor.g,
                                          themeManager.currentTheme.primaryColor.b, 0.2)
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            color: themeManager.currentTheme.primaryColor
                            radius: 8

                            Text {
                                anchors.centerIn: parent
                                text: themeManager.isDarkTheme ? "🌙" : "☀"
                                font.pixelSize: 16
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "主题模式"
                                color: themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 14
                                font.weight: Font.Medium
                            }

                            Text {
                                text: "主题切换功能已移至导航栏顶部"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }

            // 弹性空间
            Item {
                Layout.fillHeight: true
            }

            // 按钮区域
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // 弹性空间
                Item {
                    Layout.fillWidth: true
                }

                // 注销登录按钮
                Button {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 44
                    text: "注销登录"

                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(themeManager.currentTheme.errorColor, 1.2) :
                               parent.hovered ? themeManager.currentTheme.errorColor : "transparent"
                        border.color: themeManager.currentTheme.errorColor
                        border.width: 1.5
                        radius: 8

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.parent.hovered ? "white" : themeManager.currentTheme.errorColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        settingsDialog.close()
                        logoutConfirmDialog.open()
                    }
                }

                // 关闭按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 44
                    text: "关闭"

                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                               parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                               themeManager.currentTheme.primaryColor
                        radius: 8

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }

                        // 添加微妙的渐变效果
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: Qt.rgba(255, 255, 255, 0.1) }
                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.1) }
                            }
                        }
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: settingsDialog.close()
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
        width: 300
        height: 150

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
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
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

    // 用户搜索对话框
    Components.UserSearchDialog {
        id: userSearchDialog
        themeManager: mainPage.themeManager
        networkClient: ChatNetworkClient
        messageDialog: mainPage.messageDialog
        userDetailDialog: userDetailDialog
        addFriendConfirmDialog: addFriendConfirmDialog

        onUserSelected: function(userInfo) {
            // 用户选择了某个用户，可以在这里处理
            // User selected from search
        }

        onFriendRequestSent: function(userInfo) {
            // 好友请求已发送
            messageDialog.showSuccess("请求已发送", "好友请求已发送给 " + (userInfo.display_name || userInfo.username))
        }
    }

    // 用户详细信息对话框
    Components.UserDetailDialog {
        id: userDetailDialog
        themeManager: mainPage.themeManager
        messageDialog: mainPage.messageDialog

        onAddFriendClicked: {
            addFriendConfirmDialog.targetUser = userDetailDialog.userInfo
            addFriendConfirmDialog.open()
        }
    }

    // 添加好友确认对话框
    Components.AddFriendConfirmDialog {
        id: addFriendConfirmDialog
        themeManager: mainPage.themeManager
        messageDialog: mainPage.messageDialog

        onFriendRequestConfirmed: function(requestData) {
            // 发送好友请求到服务器
            if (ChatNetworkClient) {
                ChatNetworkClient.sendFriendRequest(
                    requestData.target_user_identifier,
                    requestData.message,
                    requestData.remark,
                    requestData.group
                )
            }
        }
    }

    // 分组管理对话框
    Components.GroupManagementDialog {
        id: groupManagementDialog
        themeManager: mainPage.themeManager

        onGroupCreated: function(name) {
            FriendGroupManager.createFriendGroup(name)
        }

        onGroupRenamed: function(groupId, newName) {
            FriendGroupManager.renameFriendGroup(groupId, newName)
        }

        onGroupDeleted: function(groupId) {
            FriendGroupManager.deleteFriendGroup(groupId)
        }
    }

    // 分组好友右键菜单
    Menu {
        id: groupedFriendContextMenu
        property var friendData: ({})

        MenuItem {
            text: qsTr("发送消息")
            onTriggered: {
                // Send message to grouped friend
                // TODO: 打开聊天窗口
            }
        }

        MenuItem {
            text: qsTr("查看资料")
            onTriggered: {
                // View friend profile
                // TODO: 打开用户资料窗口
            }
        }

        MenuSeparator {}

        Menu {
            title: qsTr("移动到分组")

            Repeater {
                model: FriendGroupManager.friendGroups

                MenuItem {
                    text: modelData.name
                    enabled: modelData.id !== groupedFriendContextMenu.friendData.groupId
                    onTriggered: {
                        FriendGroupManager.moveFriendToGroup(
                            groupedFriendContextMenu.friendData.id,
                            modelData.id
                        )
                    }
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("删除好友")
            onTriggered: {
                // Delete friend
                // TODO: 实现删除好友功能
            }
        }
    }

    // ChatNetworkClient信号连接
    Connections {
        target: ChatNetworkClient

        function onUsersSearchResult(users) {
            // 处理用户搜索结果
            if (userSearchDialog.visible) {
                userSearchDialog.handleSearchResults(users)
            }
        }

        function onSearchFailed(errorCode, errorMessage) {
            // 处理搜索失败
            if (userSearchDialog.visible) {
                userSearchDialog.handleSearchError(errorMessage)
            }
        }

        function onFriendRequestSent(success, message) {
            // 处理好友请求发送结果
            if (success) {
                messageDialog.showSuccess("请求已发送", "好友请求已成功发送")
                // 关闭所有相关对话框
                addFriendConfirmDialog.close()
                userDetailDialog.close()
                userSearchDialog.close()
            } else {
                messageDialog.showError("发送失败", message || "发送好友请求失败")
            }
        }

        function onFriendListReceived(friends) {
            // 处理好友列表更新
            // Friend list received
            // 这里可以更新好友列表UI
        }

        function onFriendRequestReceived(request) {
            // 处理收到的好友请求
            // Friend request received
            // 这里可以显示好友请求通知
        }

        function onFriendAdded(friendInfo) {
            // 处理好友添加成功
            // Friend added
            messageDialog.showSuccess("好友添加成功", "已成功添加好友")
        }
    }

    // 群组右键菜单
    Menu {
        id: groupContextMenu
        property var groupData: ({})

        MenuItem {
            text: qsTr("进入群聊")
            onTriggered: {
                // Enter group chat
                // TODO: 打开群聊窗口
            }
        }

        MenuItem {
            text: qsTr("群组信息")
            onTriggered: {
                // View group info
                // TODO: 打开群组信息窗口
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("退出群组")
            onTriggered: {
                // Leave group
                // TODO: 实现退出群组功能
            }
        }
    }

    // 最近联系右键菜单
    Menu {
        id: recentContextMenu
        property var contactData: ({})

        MenuItem {
            text: qsTr("发送消息")
            onTriggered: {
                // Send message to recent contact
                // TODO: 打开聊天窗口
            }
        }

        MenuItem {
            text: recentContextMenu.contactData.type === "group" ? qsTr("群组信息") : qsTr("查看资料")
            onTriggered: {
                // View recent contact info
                // TODO: 打开信息窗口
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("从最近联系中移除")
            onTriggered: {
                // Remove from recent contacts
                // TODO: 实现从最近联系中移除功能
            }
        }
    }

    // FriendGroupManager信号连接
    Connections {
        target: FriendGroupManager

        function onOperationCompleted(operation, success, message) {
            if (success) {
                messageDialog.showSuccess("操作成功", message)
            } else {
                messageDialog.showError("操作失败", message)
            }
        }

        function onDataRefreshed() {
            // Friend group data refreshed
        }
    }

    // 分组管理处理函数
    function handleGroupManagement(action, groupData) {
        switch(action) {
            case "create":
                groupManagementDialog.mode = "create"
                groupManagementDialog.groupName = ""
                groupManagementDialog.groupId = -1
                groupManagementDialog.memberCount = 0
                groupManagementDialog.open()
                break

            case "rename":
                groupManagementDialog.mode = "rename"
                groupManagementDialog.groupName = groupData.name || ""
                groupManagementDialog.groupId = groupData.id || -1
                groupManagementDialog.memberCount = groupData.members ? groupData.members.length : 0
                groupManagementDialog.open()
                break

            case "delete":
                groupManagementDialog.mode = "delete"
                groupManagementDialog.groupName = groupData.name || ""
                groupManagementDialog.groupId = groupData.id || -1
                groupManagementDialog.memberCount = groupData.members ? groupData.members.length : 0
                groupManagementDialog.open()
                break

            case "addMember":
                // 打开添加成员对话框（可以复用添加好友功能）
                userSearchDialog.show()
                break

            default:
                console.error("Unknown group management action:", action)
        }
    }

    // 群组分类管理处理函数
    function handleGroupCategoryManagement(action, groupData) {
        switch(action) {
            case "create":
                // 创建群组分类
                messageDialog.showInfo("提示", "群组分类管理功能开发中...")
                break

            case "rename":
                // 重命名群组分类
                messageDialog.showInfo("提示", "群组分类重命名功能开发中...")
                break

            case "delete":
                // 删除群组分类
                messageDialog.showInfo("提示", "群组分类删除功能开发中...")
                break

            case "addMember":
                // 添加群组到分类
                messageDialog.showInfo("提示", "添加群组到分类功能开发中...")
                break

            default:
                console.error("Unknown group category management action:", action)
        }
    }

    // 最近联系管理处理函数
    function handleRecentContactManagement(action, groupData) {
        switch(action) {
            case "create":
                // 创建最近联系分组
                messageDialog.showInfo("提示", "最近联系分组管理功能开发中...")
                break

            case "rename":
                // 重命名最近联系分组
                messageDialog.showInfo("提示", "最近联系分组重命名功能开发中...")
                break

            case "delete":
                // 删除最近联系分组
                messageDialog.showInfo("提示", "最近联系分组删除功能开发中...")
                break

            case "addMember":
                // 添加联系人到分组
                messageDialog.showInfo("提示", "添加联系人到最近联系分组功能开发中...")
                break

            default:
                console.error("Unknown recent contact management action:", action)
        }
    }

    // 组件初始化
    Component.onCompleted: {
        // MainPage loaded successfully

        // 初始化ChatNetworkClient
        if (ChatNetworkClient) {
            // ChatNetworkClient初始化在C++端处理
            // ChatNetworkClient available
        }

        // 连接认证管理器信号
        if (authManager) {
            authManager.loginSucceeded.connect(function(user) {
                // User logged in
                // 用户登录成功后，确保ChatNetworkClient已初始化
                if (ChatNetworkClient) {
                    // ChatNetworkClient ready after login
                }
            })
        }
    }
}
