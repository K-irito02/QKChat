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
    property int defaultExpandedWidth: 300
    property int navWidth: isNavCollapsed ? defaultCollapsedWidth : defaultExpandedWidth
    property int minNavWidth: 80
    property int maxNavWidth: 500

    // 分割线位置
    property real verticalSplitPosition: 0.3  // 左侧占比
    property real horizontalSplitPosition: 0.7  // 上方占比

    // 当前导航分类
    property string currentNavCategory: "recent"
    property string currentBottomCategory: "addFriend"

    // 信号
    signal logout()

    // 监听navWidth变化，确保状态同步
    onNavWidthChanged: {
        // 如果宽度接近收缩状态但isNavCollapsed为false，则更新状态
        if (navWidth <= (defaultCollapsedWidth + 20) && !isNavCollapsed) {
            isNavCollapsed = true
        }
        // 如果宽度接近展开状态但isNavCollapsed为true，则更新状态
        else if (navWidth >= (defaultExpandedWidth - 20) && isNavCollapsed) {
            isNavCollapsed = false
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

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: isNavCollapsed ? 4 : 15
                        spacing: isNavCollapsed ? 0 : 12

                        // 用户头像
                        Rectangle {
                            Layout.preferredWidth: isNavCollapsed ? 36 : 40
                            Layout.preferredHeight: isNavCollapsed ? 36 : 40
                            Layout.alignment: isNavCollapsed ? Qt.AlignHCenter : Qt.AlignLeft
                            color: themeManager.currentTheme.primaryColor
                            radius: isNavCollapsed ? 18 : 20
                            border.color: themeManager.currentTheme.successColor
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: sessionManager && sessionManager.currentUser ?
                                      sessionManager.currentUser.username.charAt(0).toUpperCase() : "U"
                                color: "white"
                                font.pixelSize: isNavCollapsed ? 14 : 16
                                font.weight: Font.Bold
                            }
                        }

                        // 用户信息 (收缩时隐藏)
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            visible: !isNavCollapsed

                            Text {
                                text: sessionManager.currentUser ?
                                      sessionManager.currentUser.username : "未知用户"
                                color: themeManager.currentTheme.textPrimaryColor
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 6
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: themeManager.currentTheme.successColor
                                }

                                Text {
                                    text: "在线"
                                    color: themeManager.currentTheme.successColor
                                    font.pixelSize: 12
                                }
                            }
                        }

                        // 功能按钮区域
                        RowLayout {
                            spacing: 6
                            visible: !isNavCollapsed

                            // 主题切换按钮
                            Button {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28

                                background: Rectangle {
                                    color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                    radius: 14

                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }
                                }

                                contentItem: Text {
                                    text: themeManager.isDarkTheme ? "🌙" : "☀"
                                    color: themeManager.currentTheme.textSecondaryColor
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: themeManager.toggleTheme()

                                ToolTip.visible: hovered
                                ToolTip.text: themeManager.isDarkTheme ? "切换到浅色模式" : "切换到深色模式"
                                ToolTip.delay: 500
                            }

                            // 设置按钮
                            Button {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28

                                background: Rectangle {
                                    color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                    radius: 14

                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }
                                }

                                contentItem: Text {
                                    text: "⚙"
                                    color: themeManager.currentTheme.textSecondaryColor
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: settingsDialog.open()

                                ToolTip.visible: hovered
                                ToolTip.text: "设置"
                                ToolTip.delay: 500
                            }

                            // 收缩按钮
                            Button {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28

                                background: Rectangle {
                                    color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                    radius: 14

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
                                ToolTip.text: "收缩导航栏"
                                ToolTip.delay: 500
                            }
                        }

                        // 收缩模式下的恢复按钮
                        Button {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            visible: isNavCollapsed

                            background: Rectangle {
                                color: parent.hovered ? themeManager.currentTheme.borderColor : "transparent"
                                radius: 12

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            contentItem: Text {
                                text: "▶"
                                color: themeManager.currentTheme.textSecondaryColor
                                font.pixelSize: 10
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

                // 联系人列表
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: isNavCollapsed ? 2 : 5

                        ListView {
                            id: contactsList
                            model: contactsModel
                            delegate: isNavCollapsed ? collapsedContactDelegate : contactDelegate
                            spacing: isNavCollapsed ? 4 : 2

                            // 模拟联系人数据
                            Component.onCompleted: {
                                // 检查是否已经有数据，避免重复添加
                                if (contactsModel.count === 0) {
                                    contactsModel.append({
                                        "name": "产品设计小组",
                                        "lastMessage": "线上产品会议",
                                        "time": "10:45",
                                        "unreadCount": 0,
                                        "isGroup": true,
                                        "isOnline": true
                                    })
                                    contactsModel.append({
                                        "name": "王芳",
                                        "lastMessage": "设计方案的修改已经发送给了，大家看看有没有问题",
                                        "time": "10:15",
                                        "unreadCount": 0,
                                        "isGroup": false,
                                        "isOnline": true
                                    })
                                    contactsModel.append({
                                        "name": "李华",
                                        "lastMessage": "新自然语文已经",
                                        "time": "09:30",
                                        "unreadCount": 0,
                                        "isGroup": false,
                                        "isOnline": false
                                    })
                                    contactsModel.append({
                                        "name": "赵雷",
                                        "lastMessage": "前端开发完",
                                        "time": "昨天",
                                        "unreadCount": 0,
                                        "isGroup": false,
                                        "isOnline": false
                                    })
                                    contactsModel.append({
                                        "name": "好友列表",
                                        "lastMessage": "",
                                        "time": "",
                                        "unreadCount": 0,
                                        "isGroup": false,
                                        "isOnline": false
                                    })
                                }
                                contactsModel.append({
                                    "name": "王芳",
                                    "lastMessage": "UI设计师",
                                    "time": "",
                                    "unreadCount": 0,
                                    "isGroup": false,
                                    "isOnline": true
                                })
                                contactsModel.append({
                                    "name": "赵雷",
                                    "lastMessage": "产品经理",
                                    "time": "",
                                    "unreadCount": 0,
                                    "isGroup": false,
                                    "isOnline": false
                                })
                                contactsModel.append({
                                    "name": "群组列表",
                                    "lastMessage": "",
                                    "time": "",
                                    "unreadCount": 0,
                                    "isGroup": false,
                                    "isOnline": false
                                })
                                contactsModel.append({
                                    "name": "前端开发组",
                                    "lastMessage": "8名成员",
                                    "time": "",
                                    "unreadCount": 0,
                                    "isGroup": true,
                                    "isOnline": true
                                })
                                contactsModel.append({
                                    "name": "公司团建群",
                                    "lastMessage": "",
                                    "time": "",
                                    "unreadCount": 0,
                                    "isGroup": true,
                                    "isOnline": false
                                })
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
                                messageDialog.showInfo("功能开发中", "添加好友功能正在开发中")
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
                                    messageDialog.showInfo("功能开发中", "添加好友功能正在开发中")
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
                                    messageDialog.showInfo("功能开发中", "添加好友功能正在开发中")
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
                        var newWidth = startWidth + deltaX

                        // 限制最小和最大宽度
                        if (newWidth >= minNavWidth && newWidth <= maxNavWidth) {
                            leftPanel.Layout.preferredWidth = newWidth
                            navWidth = newWidth
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

                                // 更新分割位置
                                horizontalSplitPosition = newSplitPosition
                                messagesArea.Layout.preferredHeight = availableHeight * newSplitPosition
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
                    Layout.preferredHeight: Math.max(120, messagesArea.parent.height * (1 - horizontalSplitPosition) - 6)
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
        id: contactsModel
    }

    ListModel {
        id: messagesModel
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
                anchors.margins: 10
                spacing: 12

                // 头像
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

                // 信息
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: model.name
                            color: themeManager.currentTheme.textPrimaryColor
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.time
                            color: themeManager.currentTheme.textTertiaryColor
                            font.pixelSize: 11
                            visible: model.time !== ""
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: model.lastMessage
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        visible: model.lastMessage !== ""
                    }
                }

                // 未读消息数
                Rectangle {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    color: themeManager.currentTheme.errorColor
                    radius: 10
                    visible: model.unreadCount > 0

                    Text {
                        anchors.centerIn: parent
                        text: model.unreadCount
                        color: "white"
                        font.pixelSize: 10
                        font.weight: Font.Bold
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
        console.log("Sending message:", messageText)
    }

    // 收缩模式的联系人委托
    Component {
        id: collapsedContactDelegate

        Rectangle {
            width: contactsList.width
            height: 60
            color: "transparent"

            Rectangle {
                anchors.centerIn: parent
                width: 40
                height: 40
                color: model.isGroup ? themeManager.currentTheme.secondaryColor : themeManager.currentTheme.primaryColor
                radius: 20
                border.color: model.isOnline ? themeManager.currentTheme.successColor : themeManager.currentTheme.borderColor
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: model.name.charAt(0)
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.Bold
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
                    radius: 8
                    opacity: 0.3

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
            }
        }
    }



    // 设置对话框
    Dialog {
        id: settingsDialog
        anchors.centerIn: parent
        modal: true
        title: "设置"
        width: 400
        height: 300

        background: Rectangle {
            color: themeManager.currentTheme.surfaceColor
            radius: 12
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 20

            Text {
                text: "应用设置"
                color: themeManager.currentTheme.textPrimaryColor
                font.pixelSize: 16
                font.weight: Font.Medium
            }

            Text {
                text: "主题切换功能已移至导航栏顶部"
                color: themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            Item {
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "注销登录"

                    background: Rectangle {
                        color: parent.hovered ? themeManager.currentTheme.errorColor : "transparent"
                        border.color: themeManager.currentTheme.errorColor
                        border.width: 1
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.parent.hovered ? "white" : themeManager.currentTheme.errorColor
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        settingsDialog.close()
                        logoutConfirmDialog.open()
                    }
                }

                Button {
                    text: "关闭"

                    background: Rectangle {
                        color: parent.hovered ? themeManager.currentTheme.primaryColor : "transparent"
                        border.color: themeManager.currentTheme.primaryColor
                        border.width: 1
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.parent.hovered ? "white" : themeManager.currentTheme.primaryColor
                        font.pixelSize: 12
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
}
