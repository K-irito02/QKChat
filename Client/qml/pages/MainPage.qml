import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components" as Components
import "../windows" as Windows

/**
 * @brief 聊天主界面
 *
 * 用户登录成功后的聊天界面，包含左侧导航栏和右侧聊天区域。
 * 实现了联系人管理、消息展示和发送等核心聊天功能。
 * 支持可拖拽分割线、导航栏收缩等高级功能。
 */
Rectangle {
    id: mainPage
    anchors.fill: parent

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

    // 监听currentChatUser变化
    onCurrentChatUserChanged: {
        console.log("=== currentChatUser变化 ===")
        console.log("新的currentChatUser:", JSON.stringify(currentChatUser))
        if (currentChatUser) {
            console.log("用户ID字段检查:")
            console.log("  user_id:", currentChatUser.user_id)
            console.log("  id:", currentChatUser.id)
            console.log("  friend_id:", currentChatUser.friend_id)
        }
        console.log("=== currentChatUser变化结束 ===")
    }
    
    // 好友请求相关属性
    property var friendRequests: []
    property bool isLoadingRequests: false
    
    // 好友请求刷新定时器
    Timer {
        id: friendRequestTimer
        interval: 30000 // 30秒刷新一次
        repeat: true
        running: false
        onTriggered: {
            refreshFriendRequests()
        }
    }

    // 信号
    signal logout()

    // 安全的主题访问函数
    function getThemeColor(colorProperty, defaultValue) {
        if (themeManager && themeManager.currentTheme && themeManager.currentTheme[colorProperty]) {
            return themeManager.currentTheme[colorProperty]
        }
        return defaultValue || "#f5f5f5"
    }
    
    // 检查主题管理器是否可用
    function isThemeManagerAvailable() {
        return themeManager && themeManager.currentTheme
    }

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
    
    // 刷新好友请求列表
    function refreshFriendRequests() {
        console.log("刷新好友请求列表")
        if (isLoadingRequests) {
            console.log("好友请求正在加载中，跳过重复刷新")
            return
        }
        isLoadingRequests = true
        if (ChatNetworkClient) {
            ChatNetworkClient.getFriendRequests()
        } else {
            console.error("ChatNetworkClient not available")
            isLoadingRequests = false
        }
    }
    
    // 刷新好友列表和分组（防重复调用）
    property bool isRefreshingFriends: false
    function refreshFriendData() {
        console.log("=== 开始刷新好友数据 ===")
        console.log("当前时间:", new Date().toISOString())
        console.log("刷新状态:", isRefreshingFriends ? "正在刷新中" : "可以刷新")
        
        if (isRefreshingFriends) {
            console.log("好友数据正在刷新中，跳过重复刷新")
            return
        }
        
        console.log("开始刷新好友数据")
        isRefreshingFriends = true
        
        if (ChatNetworkClient) {
            console.log("ChatNetworkClient可用，发送好友列表请求...")
            ChatNetworkClient.getFriendList()
            console.log("好友列表请求已发送")
            
            console.log("发送好友分组请求...")
            ChatNetworkClient.getFriendGroups()
            console.log("好友分组请求已发送")
        } else {
            console.error("ChatNetworkClient不可用，无法刷新好友数据")
        }
        
        // 重置刷新状态
        var resetTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 2000; repeat: false; running: true }', mainPage, "resetRefreshTimer")
        resetTimer.triggered.connect(function() {
            console.log("重置好友数据刷新状态")
            isRefreshingFriends = false
        })
        
        console.log("=== 好友数据刷新请求完成 ===")
    }
    
    // 初始化时获取好友请求和好友列表（已合并到文件末尾的Component.onCompleted中）

    color: themeManager && themeManager.currentTheme ? themeManager.currentTheme.backgroundColor : "#f5f5f5"

    // 主布局：左侧导航栏 + 垂直分割线 + 右侧聊天区域
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧导航栏
        Rectangle {
            id: leftPanel
            Layout.preferredWidth: navWidth
            Layout.fillHeight: true
            color: getThemeColor("surfaceColor", "#ffffff")
            border.color: getThemeColor("borderColor", "#e0e0e0")
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
                    color: themeManager && themeManager.currentTheme ? themeManager.currentTheme.backgroundColor : "#f5f5f5"
                    border.color: themeManager && themeManager.currentTheme ? themeManager.currentTheme.borderColor : "#e0e0e0"
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
                            color: getThemeColor("primaryColor", "#2196F3")
                            radius: 18
                            border.color: getThemeColor("successColor", "#4CAF50")
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: (sessionManager && sessionManager.currentUser && sessionManager.currentUser.username) ?
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
                                color: parent.hovered ? getThemeColor("borderColor", "#e0e0e0") : "transparent"
                                radius: 10

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "▶"
                                color: getThemeColor("textSecondaryColor", "#666666")
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
                            color: getThemeColor("primaryColor", "#2196F3")
                            radius: 18
                            border.color: getThemeColor("successColor", "#4CAF50")
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: (sessionManager && sessionManager.currentUser && sessionManager.currentUser.username) ?
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
                                text: (sessionManager && sessionManager.currentUser && sessionManager.currentUser.username) ?
                                      sessionManager.currentUser.username : "用户"
                                color: getThemeColor("textPrimaryColor", "#333333")
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

                                // 最近联系人列表
                                ListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    model: RecentContactsManager.recentContacts
                                    delegate: recentContactDelegate
                                    spacing: 2
                                    
                                    // 添加调试信息
                                    onCountChanged: {
                                        console.log("最近联系人列表数量变化:", count)
                                        console.log("最近联系人列表内容长度:", RecentContactsManager.recentContacts.length)
                                        if (RecentContactsManager.recentContacts.length > 0) {
                                            console.log("第一个联系人数据:")
                                            var firstContact = RecentContactsManager.recentContacts[0]
                                            console.log("  user_id:", firstContact.user_id)
                                            console.log("  username:", firstContact.username)
                                            console.log("  display_name:", firstContact.display_name)
                                        }
                                    }
                                    
                                    ScrollBar.vertical: ScrollBar {
                                        active: true
                                        policy: ScrollBar.AsNeeded
                                    }
                                }

                                // 空状态提示
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    visible: RecentContactsManager.recentContacts.length === 0
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
                                        // Friend clicked - 设置当前聊天用户
                                        console.log("=== 好友点击事件开始 ===")
                                        console.log("点击的好友数据字段检查:")
                                        console.log("  user_id:", itemData.user_id)
                                        console.log("  id:", itemData.id)
                                        console.log("  username:", itemData.username)
                                        console.log("  display_name:", itemData.display_name)
                                        
                                        currentChatUser = itemData
                                        console.log("currentChatUser已设置，字段检查:")
                                        console.log("  user_id:", currentChatUser.user_id)
                                        console.log("  username:", currentChatUser.username)
                                        console.log("  display_name:", currentChatUser.display_name)
                                        
                                        ChatMessageManager.setCurrentChatUser(itemData)
                                        console.log("ChatMessageManager.setCurrentChatUser已调用")
                                        
                                        // 切换到最近联系分类，显示聊天记录
                                        recentButton.isActive = true
                                        friendsButton.isActive = false
                                        groupsButton.isActive = false
                                        currentNavCategory = "recent"
                                        console.log("已切换到最近联系分类，currentNavCategory:", currentNavCategory)
                                        
                                        // 添加到最近联系人
                                        console.log("准备添加到最近联系人...")
                                        addToRecentContacts(itemData)
                                        console.log("=== 好友点击事件结束 ===")
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
            color: getThemeColor("backgroundColor", "#f5f5f5")
            border.color: getThemeColor("borderColor", "#e0e0e0")
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 聊天头部
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    color: getThemeColor("surfaceColor", "#ffffff")
                    border.color: getThemeColor("borderColor", "#e0e0e0")
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
                            color: getThemeColor("secondaryColor", "#FF9800")
                            radius: 20
                            visible: currentChatUser && (currentChatUser.user_id || currentChatUser.id)

                            Text {
                                anchors.centerIn: parent
                                text: currentChatUser ? 
                                      (currentChatUser.display_name ? currentChatUser.display_name.charAt(0).toUpperCase() :
                                       currentChatUser.username ? currentChatUser.username.charAt(0).toUpperCase() : "?") : "?"
                                color: "white"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                            }
                        }

                        // 联系人信息 - 水平居中
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2
                            visible: currentChatUser && (currentChatUser.user_id || currentChatUser.id)

                            Text {
                                text: currentChatUser ? (currentChatUser.display_name || currentChatUser.username || "未知用户") : ""
                                color: getThemeColor("textPrimaryColor", "#333333")
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 4

                                Rectangle {
                                    Layout.preferredWidth: 6
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: currentChatUser && currentChatUser.is_online ? 
                                           getThemeColor("successColor", "#4CAF50") : 
                                           getThemeColor("textTertiaryColor", "#999999")
                                }

                                Text {
                                    text: currentChatUser && currentChatUser.is_online ? "在线" : "离线"
                                    color: getThemeColor("textSecondaryColor", "#666666")
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }

                        // 默认提示信息
                        Text {
                            anchors.centerIn: parent
                            text: "请选择一个好友开始聊天"
                            color: getThemeColor("textSecondaryColor", "#666666")
                            font.pixelSize: 14
                            visible: !currentChatUser || (!currentChatUser.user_id && !currentChatUser.id)
                        }
                    }
                }

                // 消息展示区域
                Rectangle {
                    id: messagesArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: parent.height * horizontalSplitPosition
                    color: getThemeColor("backgroundColor", "#f5f5f5")

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.bottomMargin: 15  // 增加底部间距

                        ListView {
                            id: messagesList
                            model: ChatMessageManager.messages
                            delegate: messageDelegate1
                            spacing: 8
                            verticalLayoutDirection: ListView.BottomToTop
                            
                            // 自动滚动到底部（新消息）
                            onCountChanged: {
                                Qt.callLater(function() {
                                    if (count > 0) {
                                        positionViewAtEnd()
                                    }
                                })
                            }
                            
                            // 空状态提示
                            Label {
                                anchors.centerIn: parent
                                visible: parent.count === 0 && currentChatUser && (currentChatUser.user_id || currentChatUser.id)
                                text: "暂无聊天记录"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 14
                            }
                        }
                    }
                }

                // 水平分割线
                Rectangle {
                    id: horizontalSplitter
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    color: getThemeColor("borderColor", "#e0e0e0")

                    // 添加视觉指示器
                    Rectangle {
                        anchors.centerIn: parent
                        width: 40
                        height: 2
                        radius: 1
                        color: getThemeColor("textTertiaryColor", "#999999")
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
                    color: getThemeColor("surfaceColor", "#ffffff")
                    border.color: getThemeColor("borderColor", "#e0e0e0")
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
                                        color: parent.hovered ? getThemeColor("borderColor", "#e0e0e0") : "transparent"
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

    // 最近联系人委托组件
    Component {
        id: recentContactDelegate

        Rectangle {
            width: parent ? parent.width : 200
            height: 60
            color: recentMouseArea.containsMouse ? themeManager.currentTheme.borderColor : "transparent"
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
                    border.color: modelData.is_online ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: {
                            var displayName = modelData.display_name || modelData.username || modelData.name || modelData.displayName || "";
                            console.log("最近联系人头像显示 - displayName:", displayName);
                            console.log("最近联系人头像显示 - 字段检查:");
                            console.log("  display_name:", modelData.display_name);
                            console.log("  username:", modelData.username);
                            console.log("  name:", modelData.name);
                            console.log("  displayName:", modelData.displayName);
                            if (displayName && displayName.length > 0) {
                                var firstChar = displayName.charAt(0).toUpperCase();
                                console.log("最近联系人头像显示 - firstChar:", firstChar);
                                return firstChar;
                            } else {
                                console.log("最近联系人头像显示 - 使用默认字符 ?");
                                return "?";
                            }
                        }
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
                        color: modelData.is_online ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                        border.color: themeManager.currentTheme.backgroundColor
                        border.width: 2
                    }
                }

                // 联系人信息
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: {
                            var displayText = modelData.display_name || modelData.username || modelData.name || modelData.displayName || "";
                            console.log("最近联系人显示名称:", displayText);
                            console.log("最近联系人数据字段检查:");
                            console.log("  display_name:", modelData.display_name);
                            console.log("  username:", modelData.username);
                            console.log("  user_id:", modelData.user_id);
                            console.log("  name:", modelData.name);
                            console.log("  displayName:", modelData.displayName);
                            return displayText || "未知用户";
                        }
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    Text {
                        text: modelData.last_message || "暂无消息"
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                // 时间和未读消息
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 4

                    Text {
                        text: modelData.last_message_time || ""
                        color: themeManager.currentTheme.textTertiaryColor
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight
                    }

                    Rectangle {
                        Layout.preferredWidth: Math.max(18, unreadText1.implicitWidth + 6)
                        Layout.preferredHeight: 18
                        radius: 9
                        color: themeManager.currentTheme.primaryColor
                        visible: modelData.unread_count > 0

                        Text {
                            id: unreadText1
                            anchors.centerIn: parent
                            text: modelData.unread_count > 99 ? "99+" : modelData.unread_count.toString()
                            color: "white"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            MouseArea {
                id: recentMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton) {
                        console.log("=== 最近联系人点击事件开始 ===")
                        console.log("点击的最近联系人数据字段检查:")
                        console.log("  user_id:", modelData.user_id)
                        console.log("  id:", modelData.id)
                        console.log("  username:", modelData.username)
                        console.log("  display_name:", modelData.display_name)
                        
                        currentChatUser = modelData
                        console.log("currentChatUser已设置，字段检查:")
                        console.log("  user_id:", currentChatUser.user_id)
                        console.log("  username:", currentChatUser.username)
                        console.log("  display_name:", currentChatUser.display_name)
                        
                        ChatMessageManager.setCurrentChatUser(modelData)
                        console.log("ChatMessageManager.setCurrentChatUser已调用")
                        
                        console.log("=== 最近联系人点击事件结束 ===")
                    } else if (mouse.button === Qt.RightButton) {
                        // 显示右键菜单
                        recentContextMenu.contactData = modelData
                        recentContextMenu.popup()
                    }
                }

                onDoubleClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton) {
                        console.log("=== 最近联系人双击事件开始 ===")
                        console.log("双击的最近联系人数据字段检查:")
                        console.log("  user_id:", modelData.user_id)
                        console.log("  username:", modelData.username)
                        console.log("  display_name:", modelData.display_name)
                        
                        currentChatUser = modelData
                        console.log("currentChatUser已设置，字段检查:")
                        console.log("  user_id:", currentChatUser.user_id)
                        console.log("  username:", currentChatUser.username)
                        console.log("  display_name:", currentChatUser.display_name)
                        
                        ChatMessageManager.setCurrentChatUser(modelData)
                        console.log("ChatMessageManager.setCurrentChatUser已调用")
                        
                        console.log("=== 最近联系人双击事件结束 ===")
                    }
                }
            }
        }
    }

    // 消息委托组件
    Component {
        id: messageDelegate1

        Rectangle {
            width: messagesList.width
            height: messageContent1.height + 20
            color: "transparent"

            // 分界线 - 中间分隔线
            Rectangle {
                id: centerDivider
                width: 2
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter
                color: themeManager.currentTheme.borderColor
                opacity: 0.2
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10
                layoutDirection: modelData.is_own ? Qt.RightToLeft : Qt.LeftToRight

                // 头像
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    Layout.alignment: Qt.AlignTop
                    color: modelData.is_own ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.secondaryColor
                    radius: 18

                    Text {
                        anchors.centerIn: parent
                        text: modelData.sender_avatar || "?"
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }

                // 消息内容区域
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: messagesList.width * 0.45
                    spacing: 4

                    // 发送者和时间
                    RowLayout {
                        Layout.fillWidth: true
                        layoutDirection: modelData.is_own ? Qt.RightToLeft : Qt.LeftToRight

                        Text {
                            text: modelData.sender_name || ""
                            color: themeManager.currentTheme.textSecondaryColor
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        Text {
                            text: modelData.time || ""
                            color: themeManager.currentTheme.textTertiaryColor
                            font.pixelSize: 11
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // 消息气泡 - 改进的布局
                    Rectangle {
                        id: messageContent1
                        Layout.preferredWidth: Math.min(messageText1.implicitWidth + 20, messagesList.width * 0.4)
                        Layout.maximumWidth: messagesList.width * 0.4
                        implicitHeight: messageText1.implicitHeight + 16
                        color: modelData.is_own ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.surfaceColor
                        radius: 12
                        border.color: modelData.is_own ? "transparent" : themeManager.currentTheme.borderColor
                        border.width: modelData.is_own ? 0 : 1

                        // 根据发送者调整对齐方式
                        Layout.alignment: modelData.is_own ? Qt.AlignRight : Qt.AlignLeft

                        Text {
                            id: messageText1
                            anchors.fill: parent
                            anchors.margins: 8
                            text: modelData.content || ""
                            color: modelData.is_own ? "white" : themeManager.currentTheme.textPrimaryColor
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: modelData.is_own ? Text.AlignRight : Text.AlignLeft
                        }
                    }

                    // 消息状态
                    Text {
                        Layout.alignment: modelData.is_own ? Qt.AlignRight : Qt.AlignLeft
                        visible: modelData.is_own
                        text: {
                            switch(modelData.delivery_status) {
                                case "sent": return "已发送"
                                case "delivered": return "已送达"
                                case "read": return "已读"
                                case "failed": return "发送失败"
                                default: return ""
                            }
                        }
                        color: themeManager.currentTheme.textTertiaryColor
                        font.pixelSize: 10
                    }
                }

                // 占位符，用于对齐
                Item {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    visible: !modelData.is_own
                }
            }
        }
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
                    border.color: model.is_online ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
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
                        color: model.is_online ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
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
                        text: model.is_online ? qsTr("在线") : qsTr("离线")
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
                    visible: model.unread_count > 0

                    Text {
                        id: unreadText2
                        anchors.centerIn: parent
                        text: model.unread_count > 99 ? "99+" : model.unread_count.toString()
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
                        console.log("Friend selected:", JSON.stringify(model))
                        mainPage.currentChatUser = model
                        ChatMessageManager.setCurrentChatUser(model)
                        
                        // 添加到最近联系人
                        addToRecentContacts(model)
                    } else if (mouse.button === Qt.RightButton) {
                        friendContextMenu.friendInfo = model
                        friendContextMenu.popup()
                    }
                }

                onDoubleClicked: function(mouse) {
                    if (mouse.button === Qt.LeftButton) {
                        console.log("Friend double clicked:", JSON.stringify(model))
                        mainPage.currentChatUser = model
                        ChatMessageManager.setCurrentChatUser(model)
                        
                        // 添加到最近联系人
                        addToRecentContacts(model)
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

        if (!currentChatUser || (!currentChatUser.user_id && !currentChatUser.id)) {
            messageDialog.showError("错误", "请先选择聊天对象")
            return
        }

        // 通过ChatMessageManager发送消息
        ChatMessageManager.sendMessage(messageText, "text")
        
        // 清空输入框
        messageInput.text = ""
    }
    
    // 添加到最近联系人函数
    function addToRecentContacts(friendData) {
        console.log("=== addToRecentContacts函数开始 ===")
        console.log("传入的friendData字段检查:")
        console.log("  user_id:", friendData.user_id)
        console.log("  id:", friendData.id)
        console.log("  username:", friendData.username)
        console.log("  display_name:", friendData.display_name)
        console.log("RecentContactsManager是否存在:", !!RecentContactsManager)
        
        if (RecentContactsManager) {
            console.log("调用RecentContactsManager.addRecentContact...")
            RecentContactsManager.addRecentContact(friendData)
            console.log("RecentContactsManager.addRecentContact调用完成")
            
            // 检查最近联系人列表状态
            console.log("最近联系人列表长度:", RecentContactsManager.recentContacts.length)
            if (RecentContactsManager.recentContacts.length > 0) {
                console.log("第一个联系人数据:")
                var firstContact = RecentContactsManager.recentContacts[0]
                console.log("  user_id:", firstContact.user_id)
                console.log("  username:", firstContact.username)
                console.log("  display_name:", firstContact.display_name)
            }
        } else {
            console.error("RecentContactsManager不可用!")
        }
        console.log("=== addToRecentContacts函数结束 ===")
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
                    text: (model.name || model.username || "U").charAt(0).toUpperCase()
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
                showUserDetail(friendContextMenu.friendInfo)
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
            // 调用删除好友功能
            var friendId = deleteFriendDialog.friendInfo.user_id || deleteFriendDialog.friendInfo.id || deleteFriendDialog.friendInfo.friend_id
            if (friendId) {
                console.log("删除好友:", friendId, deleteFriendDialog.friendInfo.display_name || deleteFriendDialog.friendInfo.username)
                ChatNetworkClient.removeFriend(friendId)
            } else {
                messageDialog.showError("删除失败", "无法获取好友ID")
            }
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
        friendRequests: mainPage.friendRequests
        isLoadingRequests: mainPage.isLoadingRequests

        onUserSelected: function(userInfo) {
            // 用户选择了某个用户，可以在这里处理
            // User selected from search
        }

        onFriendRequestSent: function(userInfo) {
            // 好友请求已发送
            messageDialog.showSuccess("请求已发送", "好友请求已发送给 " + (userInfo.display_name || userInfo.username))
        }
        
        onAddFriendRequest: function(userInfo) {
            showAddFriend(userInfo)
        }
        
        // 监听UserSearchDialog的可见性变化
        onVisibleChanged: {
            userSearchDialogVisible = userSearchDialog.visible
        }
    }
    
    // 窗口管理 - 动态创建多个窗口实例

    // 窗口管理 - 支持多个窗口同时存在
    
    // 窗口可见性状态管理
    property bool userSearchDialogVisible: false
    property bool userDetailWindowVisible: false
    property bool addFriendWindowVisible: false
    
    // 多窗口管理 - 存储所有活跃的窗口
    property var activeUserDetailWindows: []
    property var activeAddFriendWindows: []
    
    // 窗口层级管理 - 动态窗口层级管理
    
    // 监听主窗口焦点变化，确保弹出窗口层级
    onActiveFocusChanged: {
        if (activeFocus) {
            // 主窗口获得焦点时，确保所有弹出窗口都在主窗口之上
            Qt.callLater(ensureAllWindowsAboveMain)
        }
    }
    
    // 确保所有弹出窗口都在聊天主界面之上
    function ensureAllWindowsAboveMain() {
        // 确保所有用户详情窗口都在聊天主界面之上
        for (var i = 0; i < activeUserDetailWindows.length; i++) {
            var window = activeUserDetailWindows[i]
            if (window && window.visible) {
                window.raise()
                window.requestActivate()
            }
        }
        
        // 确保所有添加好友窗口都在聊天主界面之上
        for (var i = 0; i < activeAddFriendWindows.length; i++) {
            var window = activeAddFriendWindows[i]
            if (window && window.visible) {
                window.raise()
                window.requestActivate()
            }
        }
        
        // 确保UserSearchDialog在聊天主界面之上
        if (userSearchDialog && userSearchDialog.visible) {
            userSearchDialog.raise()
            userSearchDialog.requestActivate()
        }
    }
    

    
    // 显示用户详情窗口
    function showUserDetail(user) {
        // 检查是否已有相同用户的用户详情窗口
        for (var i = 0; i < activeUserDetailWindows.length; i++) {
            var window = activeUserDetailWindows[i]
            if (window && window.userInfo && window.userInfo.user_id === user.user_id) {
                window.raise()
                return
            }
        }
        
        // 创建新的用户详情窗口
        var component = Qt.createComponent("qrc:/qml/windows/UserDetailWindow.qml")
        if (component.status === Component.Ready) {
            var newWindow = component.createObject(null, {
                "themeManager": mainPage.themeManager,
                "messageDialog": mainPage.messageDialog
            })
            
            if (newWindow) {
                // 设置窗口层级标志，确保在聊天主界面之上
                newWindow.flags = Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
                
                newWindow.showUserDetail(user)
                activeUserDetailWindows.push(newWindow)
                userDetailWindowVisible = true
                
                // 确保窗口在聊天主界面之上
                Qt.callLater(function() {
                    if (newWindow && newWindow.visible) {
                        newWindow.raise()
                        newWindow.requestActivate()
                    }
                })
                
                // 连接信号
                newWindow.addFriendClicked.connect(function() {
                    onUserDetailAddFriendClicked(newWindow)
                })
                
                newWindow.windowClosed.connect(function() {
                    onUserDetailWindowClosed(newWindow)
                })
            } else {
                // Failed to create userDetailWindow
            }
        } else {
            // Failed to load UserDetailWindow component
        }
    }
    
    // 显示添加好友窗口
    function showAddFriend(user) {
        // 检查是否已有相同用户的添加好友窗口
        for (var i = 0; i < activeAddFriendWindows.length; i++) {
            var window = activeAddFriendWindows[i]
            if (window && window.targetUser && window.targetUser.user_id === user.user_id) {
                window.raise()
                return
            }
        }
        
        // 创建新的添加好友窗口
        var component = Qt.createComponent("qrc:/qml/windows/AddFriendWindow.qml")
        if (component.status === Component.Ready) {
            var newWindow = component.createObject(null, {
                "themeManager": mainPage.themeManager,
                "messageDialog": mainPage.messageDialog
            })
            
            if (newWindow) {
                // 设置窗口层级标志，确保在聊天主界面之上
                newWindow.flags = Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
                
                newWindow.showAddFriend(user)
                activeAddFriendWindows.push(newWindow)
                addFriendWindowVisible = true
                
                // 确保窗口在聊天主界面之上
                Qt.callLater(function() {
                    if (newWindow && newWindow.visible) {
                        newWindow.raise()
                        newWindow.requestActivate()
                    }
                })
                
                // 连接信号
                newWindow.friendRequestConfirmed.connect(function(requestData) {
                    onFriendRequestConfirmed(requestData)
                })
                
                newWindow.windowClosed.connect(function() {
                    onAddFriendWindowClosed(newWindow)
                })
            } else {
                // Failed to create addFriendWindow
            }
        } else {
                // Failed to load AddFriendWindow component
        }
    }
    
    // 用户详情窗口添加好友事件 - 窗口转换模式
    function onUserDetailAddFriendClicked(sourceWindow) {
        if (sourceWindow && sourceWindow.userInfo) {
            // 删除其他相同用户的发送好友请求窗口
            for (var i = activeAddFriendWindows.length - 1; i >= 0; i--) {
                var existingWindow = activeAddFriendWindows[i]
                if (existingWindow && existingWindow.targetUser && 
                    existingWindow.targetUser.user_id === sourceWindow.userInfo.user_id) {
                    existingWindow.close()
                    activeAddFriendWindows.splice(i, 1)
                }
            }
            
            // 从用户详情窗口列表中移除当前窗口
            var index = activeUserDetailWindows.indexOf(sourceWindow)
            if (index !== -1) {
                activeUserDetailWindows.splice(index, 1)
            }
            
            // 关闭当前用户详情窗口
            sourceWindow.close()
            
            // 创建新的发送好友请求窗口（替换原窗口位置）
            showAddFriend(sourceWindow.userInfo)
            
            // 确保新窗口在正确位置显示
            Qt.callLater(function() {
                ensureAllWindowsAboveMain()
            })
        } else {
            // sourceWindow or userInfo is null
        }
    }
    
    // 用户详情窗口关闭事件
    function onUserDetailWindowClosed(closedWindow) {
        // 从活跃窗口列表中移除
        var index = activeUserDetailWindows.indexOf(closedWindow)
        if (index !== -1) {
            activeUserDetailWindows.splice(index, 1)
        }
        
        // 更新可见性状态
        userDetailWindowVisible = activeUserDetailWindows.length > 0
    }
    
    // 添加好友窗口关闭事件
    function onAddFriendWindowClosed(closedWindow) {
        // 从活跃窗口列表中移除
        var index = activeAddFriendWindows.indexOf(closedWindow)
        if (index !== -1) {
            activeAddFriendWindows.splice(index, 1)
        }
        
        // 更新可见性状态
        addFriendWindowVisible = activeAddFriendWindows.length > 0
    }
    
    // 窗口层级管理 - 子组件方式，Qt自动处理
    

    
    // 好友请求确认事件
    function onFriendRequestConfirmed(requestData) {
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
                showUserDetail(groupedFriendContextMenu.friendData)
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
                deleteFriendDialog.friendInfo = groupedFriendContextMenu.friendData
                deleteFriendDialog.open()
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
                // 关闭所有发送好友请求窗口，但保留其他窗口
                for (var i = activeAddFriendWindows.length - 1; i >= 0; i--) {
                    var window = activeAddFriendWindows[i]
                    if (window) {
                        window.close()
                    }
                }
                activeAddFriendWindows = []
                addFriendWindowVisible = false
                // 不自动关闭用户详情窗口和搜索窗口，让用户自己决定
            } else {
                messageDialog.showError("发送失败", message || "发送好友请求失败")
            }
        }

        function onFriendListReceived(friends) {
            // 处理好友列表更新
            console.log("=== 收到好友列表更新 ===")
            console.log("好友列表:", JSON.stringify(friends))
            console.log("好友列表长度:", friends ? friends.length : 0)
            
            if (!friends || friends.length === 0) {
                console.warn("好友列表为空，可能的原因:")
                console.warn("1. 服务器端friendships表中没有好友关系记录")
                console.warn("2. 好友关系状态不是'accepted'")
                console.warn("3. AcceptFriendRequest存储过程没有正确创建好友关系")
            }
            
            // 更新FriendGroupManager中的好友数据
            if (FriendGroupManager && typeof FriendGroupManager.handleFriendListReceived === 'function') {
                console.log("调用FriendGroupManager.handleFriendListReceived")
                FriendGroupManager.handleFriendListReceived(friends)
            } else {
                console.error("FriendGroupManager不可用或handleFriendListReceived方法不存在")
            }
            
            // 更新本地好友列表模型（用于其他显示）
            friendsModel.clear()
            for (var i = 0; i < friends.length; i++) {
                var friend = friends[i]
                console.log("处理好友:", friend.username, "ID:", friend.friend_id)
                friendsModel.append({
                    "id": friend.friend_id || friend.id,
                    "username": friend.username,
                    "display_name": friend.display_name,
                    "avatar_url": friend.avatar_url,
                    "online_status": friend.online_status,
                    "note": friend.note,
                    "accepted_at": friend.accepted_at,
                    "isOnline": friend.online_status === "online"
                })
            }
            
            // 强制刷新UI，确保数据更新
            var uiRefreshTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 100; repeat: false; running: true }', mainPage, "uiRefreshTimer")
            uiRefreshTimer.triggered.connect(function() {
                console.log("强制刷新UI - 好友列表更新")
                // 强制触发属性更新
                if (FriendGroupManager) {
                    FriendGroupManager.refreshData()
                }
            })
        }
        
        function onFriendGroupsReceived(groups) {
            // 处理好友分组更新
            console.log("收到好友分组:", JSON.stringify(groups))
            
            // 更新FriendGroupManager中的分组数据
            if (FriendGroupManager && typeof FriendGroupManager.handleFriendGroupsReceived === 'function') {
                FriendGroupManager.handleFriendGroupsReceived(groups)
            }
            
            // 强制刷新UI，确保数据更新
            var uiRefreshTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 100; repeat: false; running: true }', mainPage, "uiRefreshTimer2")
            uiRefreshTimer.triggered.connect(function() {
                console.log("强制刷新UI - 好友分组更新")
                // 强制触发属性更新
                if (FriendGroupManager) {
                    FriendGroupManager.refreshData()
                }
            })
        }
        
        function onFriendRequestReceived(request) {
            // 处理收到的好友请求
            console.log("收到好友请求通知:", JSON.stringify(request))
            
            // 显示好友请求通知
            if (request && request.notification_type === "friend_request") {
                var requesterName = request.from_display_name || request.from_username || "未知用户"
                var message = request.message || "请求添加您为好友"
                
                messageDialog.showInfo("新的好友请求", 
                    requesterName + " 想添加您为好友\n\n消息: " + message)
                
                // 自动刷新好友请求列表
                refreshFriendRequests()
            }
        }

        function onFriendAdded(friendInfo) {
            // 处理好友添加成功
            // Friend added
            messageDialog.showSuccess("好友添加成功", "已成功添加好友")
        }
        
        function onFriendRequestResponded(success, message) {
            console.log("=== 收到好友请求响应 ===")
            console.log("当前时间:", new Date().toISOString())
            console.log("成功状态:", success)
            console.log("响应消息:", message)
            
            if (success) {
                console.log("好友请求处理成功，立即刷新好友数据")
                // 立即刷新好友数据，不使用延迟
                refreshFriendData()
                refreshFriendRequests()
            } else {
                console.log("好友请求处理失败:", message)
            }
            
            console.log("=== 好友请求响应处理完成 ===")
        }
        
        function onFriendListUpdated() {
            console.log("=== 收到好友列表更新信号，自动刷新好友数据 ===")
            console.log("当前时间:", new Date().toISOString())
            refreshFriendData()
        }
        
        function onFriendRemoved(friendId) {
            console.log("=== 好友被删除 ===")
            console.log("被删除的好友ID:", friendId)
            
            // 从最近联系人中移除该好友
            if (RecentContactsManager && typeof RecentContactsManager.removeRecentContact === 'function') {
                RecentContactsManager.removeRecentContact(friendId)
                console.log("已从最近联系人中移除好友:", friendId)
            }
            
            // 清理该好友的聊天数据
            if (ChatMessageManager && typeof ChatMessageManager.clearMessagesForUser === 'function') {
                ChatMessageManager.clearMessagesForUser(friendId)
                console.log("已清理好友的聊天数据:", friendId)
            }
            
            // 如果当前聊天用户是被删除的好友，清空聊天区域
            if (currentChatUser && (currentChatUser.user_id === friendId || currentChatUser.id === friendId || currentChatUser.friend_id === friendId)) {
                console.log("当前聊天用户被删除，清空聊天区域")
                currentChatUser = {}
                ChatMessageManager.setCurrentChatUser({})
            }
            
            // 刷新好友列表
            refreshFriendData()
            
            messageDialog.showInfo("删除成功", "好友已删除，相关聊天记录也已清理")
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
                var contactId = recentContextMenu.contactData.user_id || recentContextMenu.contactData.id || recentContextMenu.contactData.friend_id
                if (contactId) {
                    RecentContactsManager.removeRecentContact(contactId)
                    messageDialog.showInfo("移除成功", "已从最近联系中移除")
                }
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
                // Unknown group management action
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
                // Unknown group category management action
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
                // Unknown recent contact management action
        }
    }

    // 好友请求响应处理函数
    function onFriendRequestAccepted(requestId, acceptedByUserId, acceptedByUsername, acceptedByDisplayName, note, groupName, timestamp) {
        // 处理好友请求被接受的通知
        console.log("=== 收到好友请求被接受通知 ===")
        console.log("当前时间:", new Date().toISOString())
        console.log("请求ID:", requestId)
        console.log("接受者用户ID:", acceptedByUserId)
        console.log("接受者用户名:", acceptedByUsername)
        console.log("接受者显示名:", acceptedByDisplayName)
        console.log("备注:", note)
        console.log("分组名称:", groupName)
        console.log("时间戳:", timestamp)
        
        // 显示通知
        var acceptedByName = acceptedByDisplayName || acceptedByUsername || "未知用户"
        var message = acceptedByName + " 已接受您的好友请求"
        if (note && note.trim() !== "") {
            message += "\n备注: " + note
        }
        if (groupName && groupName.trim() !== "") {
            message += "\n分组: " + groupName
        }
        
        console.log("显示通知消息:", message)
        messageDialog.showSuccess("好友请求已接受", message)
        
        // 刷新好友列表和好友请求列表
        console.log("开始刷新好友数据...")
        refreshFriendData()
        console.log("开始刷新好友请求列表...")
        refreshFriendRequests()
        console.log("=== 好友请求接受处理完成 ===")
    }
    
    function onFriendRequestRejected(requestId, rejectedByUserId, rejectedByUsername, rejectedByDisplayName, timestamp) {
        // 处理好友请求被拒绝的通知
        console.log("收到好友请求被拒绝通知:", requestId, rejectedByUsername)
        
        // 显示通知
        var rejectedByName = rejectedByDisplayName || rejectedByUsername || "未知用户"
        var message = rejectedByName + " 已拒绝您的好友请求"
        
        messageDialog.showWarning("好友请求被拒绝", message)
        
        // 刷新好友请求列表
        refreshFriendRequests()
    }
    
    function onFriendRequestIgnored(requestId, ignoredByUserId, ignoredByUsername, ignoredByDisplayName, timestamp) {
        // 处理好友请求被忽略的通知
        console.log("收到好友请求被忽略通知:", requestId, ignoredByUsername)
        
        // 显示通知
        var ignoredByName = ignoredByDisplayName || ignoredByUsername || "未知用户"
        var message = ignoredByName + " 已忽略您的好友请求"
        
        messageDialog.showInfo("好友请求被忽略", message)
        
        // 刷新好友请求列表
        refreshFriendRequests()
    }
    
    function onFriendRequestNotification(requestId, fromUserId, fromUsername, fromDisplayName, notificationType, message, timestamp, isOfflineMessage) {
        // 处理好友请求通知（包括离线消息）
        console.log("收到好友请求通知:", requestId, fromUsername, notificationType, isOfflineMessage ? "离线消息" : "实时消息")
        
        // 显示通知
        var fromName = fromDisplayName || fromUsername || "未知用户"
        var notificationTitle = isOfflineMessage ? "离线好友请求通知" : "好友请求通知"
        
        messageDialog.showInfo(notificationTitle, fromName + ": " + message)
        
        // 刷新好友请求列表
        refreshFriendRequests()
        
        // 如果是接受通知，也刷新好友列表
        if (notificationType === "accepted") {
            refreshFriendData()
        }
    }
    
    function onFriendRequestsReceived(requests) {
        // 处理好友请求列表更新
        friendRequests = requests || []
        isLoadingRequests = false
    }

    // 好友请求响应处理函数
    function onFriendRequestResponded(success, message) {
        if (success) {
            // 立即刷新好友数据，不使用延迟
            refreshFriendData()
            refreshFriendRequests()
        }
    }

    // 好友列表更新处理函数
    function onFriendListUpdated() {
        refreshFriendData()
    }
    
    // 消息处理函数
            function onMessageReceived(message) {
            console.log("Message received:", JSON.stringify(message))
            
            // 检查是否是好友删除通知
            if (message && message.action === "friend_removed") {
                console.log("收到好友删除通知:", message.remover_id)
                // 刷新好友列表，因为可能被其他用户删除了好友关系
                refreshFriendData()
                messageDialog.showInfo("好友关系变更", "您与某位用户的好友关系已被对方删除")
                return
            }
            
            if (ChatMessageManager) {
                ChatMessageManager.handleMessageReceived(message)
            }
        }
    
    function onMessageSent(messageId, success) {
        console.log("Message sent:", messageId, success)
        if (ChatMessageManager) {
            ChatMessageManager.handleMessageSent(messageId, success)
        }
    }
    
    function onChatHistoryReceived(userId, messages) {
        console.log("Chat history received for user:", userId, "messages count:", messages.length)
        if (ChatMessageManager) {
            ChatMessageManager.handleChatHistoryReceived(userId, messages)
        }
    }
    
    function onMessageStatusUpdated(messageId, status) {
        console.log("Message status updated:", messageId, status)
        if (ChatMessageManager) {
            ChatMessageManager.handleMessageStatusUpdated(messageId, status)
        }
    }
    
    function onMessageSendResult(success, message) {
        if (success) {
            console.log("Message send success:", message)
        } else {
            console.error("Message send failed:", message)
            if (message && message.includes("未加对方为好友")) {
                messageDialog.showError("发送失败", "未加对方为好友，无法发送消息")
            } else {
                messageDialog.showError("发送失败", message)
            }
        }
    }
    
    function onNewMessageReceived(message) {
        console.log("New message received:", JSON.stringify(message))
        // 可以在这里添加通知或其他UI更新
    }

    // 组件初始化
    Component.onCompleted: {
        console.log("MainPage Component.onCompleted")
        
        // 连接ChatNetworkClient信号
        if (ChatNetworkClient) {
            ChatNetworkClient.friendRequestResponded.connect(onFriendRequestResponded)
            ChatNetworkClient.friendListUpdated.connect(onFriendListUpdated)
            ChatNetworkClient.friendRequestsReceived.connect(onFriendRequestsReceived)
            ChatNetworkClient.friendRemoved.connect(onFriendRemoved)
            
            // 连接消息相关信号
            ChatNetworkClient.messageReceived.connect(onMessageReceived)
            ChatNetworkClient.messageSent.connect(onMessageSent)
            ChatNetworkClient.chatHistoryReceived.connect(onChatHistoryReceived)
            ChatNetworkClient.messageStatusUpdated.connect(onMessageStatusUpdated)
        }
        
        // 连接ChatMessageManager信号
        if (ChatMessageManager) {
            ChatMessageManager.messageSendResult.connect(onMessageSendResult)
            ChatMessageManager.newMessageReceived.connect(onNewMessageReceived)
        }
        
        // 检查是否已经有好友数据，如果没有则发送请求
        if (FriendGroupManager && FriendGroupManager.friendGroups && FriendGroupManager.friendGroups.length <= 1) { // 只有默认分组
            refreshFriendData()
        }
    }
    
    // 组件销毁时清理窗口
    Component.onDestruction: {
        // 清理所有动态创建的用户详情窗口
        for (var i = activeUserDetailWindows.length - 1; i >= 0; i--) {
            var window = activeUserDetailWindows[i]
            if (window) {
                window.close()
                window.destroy()
            }
        }
        activeUserDetailWindows = []
        userDetailWindowVisible = false
        
        // 清理所有动态创建的添加好友窗口
        for (var j = activeAddFriendWindows.length - 1; j >= 0; j--) {
            var friendWindow = activeAddFriendWindows[j]
            if (friendWindow) {
                friendWindow.close()
                friendWindow.destroy()
            }
        }
        activeAddFriendWindows = []
        addFriendWindowVisible = false
    }
}