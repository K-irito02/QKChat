import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 添加好友窗口
 * 
 * 提供用户搜索和好友请求管理功能
 * 包含搜索用户页面和新朋友页面
 */
Window {
    id: root
    
    // 公共属性
    property var themeManager
    property var networkClient
    property var messageDialog
    property var userDetailDialog
    property var addFriendConfirmDialog
    
    // 搜索状态
    property bool isSearching: false
    property var searchResults: []
    property bool hasSearched: false  // 添加搜索执行标记
    
    // 好友请求状态
    property var friendRequests: []
    property bool isLoadingRequests: false
    
    // 信号
    signal userSelected(var userInfo)
    signal friendRequestSent(var userInfo)
    
    title: qsTr("添加好友")
    modality: Qt.ApplicationModal
    width: 600
    height: 700
    minimumWidth: 500
    minimumHeight: 600
    visible: false
    
    // 窗口背景
    Rectangle {
        anchors.fill: parent
        color: themeManager.currentTheme.surfaceColor
        radius: 8
        
        // 添加阴影效果
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            anchors.leftMargin: 2
            color: Qt.rgba(0, 0, 0, 0.1)
            radius: parent.radius
            z: -1
        }
    }
    
    // 主内容容器
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20
        
        // TabBar
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            
            background: Rectangle {
                color: "transparent"
            }
            
            TabButton {
                text: qsTr("搜索用户")
                width: tabBar.width / 2
                
                background: Rectangle {
                    color: parent.checked ? themeManager.currentTheme.primaryColor : "transparent"
                    radius: 8
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "white" : themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                    font.weight: parent.checked ? Font.Medium : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            
            TabButton {
                text: qsTr("新朋友")
                width: tabBar.width / 2
                
                background: Rectangle {
                    color: parent.checked ? themeManager.currentTheme.primaryColor : "transparent"
                    radius: 8
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "white" : themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                    font.weight: parent.checked ? Font.Medium : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        
        // 页面堆栈
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // 搜索用户页面
            ColumnLayout {
                spacing: 20
                
                // 标题区域
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    
                    // 标题
                    Text {
                        text: qsTr("搜索用户")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }
                    
                    // 说明文字
                    Text {
                        text: qsTr("输入用户名、邮箱或用户ID进行搜索")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                    }
                }
                
                // 搜索区域
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    
                    // 搜索输入框
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        placeholderText: qsTr("用户名/邮箱/用户ID")
                        text: ""
                        
                        background: Rectangle {
                            color: "transparent"
                            border.color: searchField.focus ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.borderColor
                            border.width: 1.5
                            radius: 8
                            
                            Behavior on border.color {
                                ColorAnimation { duration: 150 }
                            }
                        }
                        
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        selectByMouse: true
                        
                        leftPadding: 16
                        rightPadding: 16
                        topPadding: 12
                        bottomPadding: 12
                        
                        onAccepted: searchButton.clicked()
                    }
                    
                    Button {
                        id: searchButton
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 48
                        text: isSearching ? qsTr("搜索中") : qsTr("搜索")
                        enabled: !isSearching && searchField.text.trim().length > 0
                        
                        background: Rectangle {
                            color: parent.enabled ? 
                                   (parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                                    parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                                    themeManager.currentTheme.primaryColor) :
                                   Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                           themeManager.currentTheme.primaryColor.g,
                                           themeManager.currentTheme.primaryColor.b, 0.5)
                            radius: 8
                            
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: performSearch()
                    }
                }
                
                // 搜索结果区域
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    
                    // 结果标题
                    Text {
                        text: searchResults.length > 0 ? qsTr("搜索结果 (%1)").arg(searchResults.length) : 
                              isSearching ? qsTr("搜索中...") : qsTr("输入关键词开始搜索")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        visible: searchField.text.trim().length > 0 || searchResults.length > 0
                    }
                    
                    // 搜索结果列表
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        
                        ListView {
                            id: resultsListView
                            model: searchResults
                            spacing: 8
                            
                            delegate: UserSearchResultItem {
                                width: resultsListView.width
                                userInfo: modelData
                                themeManager: root.themeManager
                                
                                onViewDetailsClicked: {
                                    if (root.userDetailDialog) {
                                        root.userDetailDialog.userInfo = userInfo
                                        root.userDetailDialog.open()
                                    }
                                }

                                onAddFriendClicked: {
                                    if (root.addFriendConfirmDialog) {
                                        root.addFriendConfirmDialog.targetUser = userInfo
                                        root.addFriendConfirmDialog.open()
                                    }
                                }
                            }
                        }
                        
                        // 空状态提示
                        Text {
                            anchors.centerIn: parent
                            text: hasSearched && searchField.text.trim().length > 0 && searchResults.length === 0 && !isSearching ?
                                  qsTr("未找到匹配的用户") : ""
                            color: themeManager.currentTheme.textSecondaryColor
                            font.pixelSize: 16
                            visible: text.length > 0
                        }
                    }
                }
            }
            
            // 新朋友页面
            ColumnLayout {
                spacing: 20
                
                // 标题区域
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    
                    // 标题
                    Text {
                        text: qsTr("新朋友")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 24
                        font.weight: Font.Bold
                    }
                    
                    // 说明文字
                    Text {
                        text: qsTr("处理收到的好友请求")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                    }
                }
                
                // 刷新按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    
                    Item {
                        Layout.fillWidth: true
                    }
                    
                    Button {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 36
                        text: isLoadingRequests ? qsTr("加载中") : qsTr("刷新")
                        enabled: !isLoadingRequests
                        
                        background: Rectangle {
                            color: parent.enabled ? 
                                   (parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                                    parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                                    themeManager.currentTheme.primaryColor) :
                                   Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                           themeManager.currentTheme.primaryColor.g,
                                           themeManager.currentTheme.primaryColor.b, 0.5)
                            radius: 8
                            
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }
                        }
                        
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: loadFriendRequests()
                    }
                }
                
                // 好友请求列表
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    
                    // 列表标题
                    Text {
                        text: friendRequests.length > 0 ? qsTr("好友请求 (%1)").arg(friendRequests.length) : 
                              isLoadingRequests ? qsTr("加载中...") : qsTr("暂无好友请求")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    
                    // 好友请求列表
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        
                        ListView {
                            id: friendRequestsListView
                            model: friendRequests
                            spacing: 8
                            
                            delegate: Rectangle {
                                width: friendRequestsListView.width
                                height: 80
                                color: "transparent"
                                border.color: themeManager.currentTheme.borderColor
                                border.width: 1
                                radius: 8
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 12
                                    
                                    // 用户头像（占位符）
                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        radius: 24
                                        color: themeManager.currentTheme.primaryColor
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: (modelData.username || modelData.display_name || "U").charAt(0).toUpperCase()
                                            color: "white"
                                            font.pixelSize: 18
                                            font.weight: Font.Bold
                                        }
                                    }
                                    
                                    // 用户信息
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        
                                        Text {
                                            text: modelData.display_name || modelData.username || "未知用户"
                                            color: themeManager.currentTheme.textPrimaryColor
                                            font.pixelSize: 16
                                            font.weight: Font.Medium
                                        }
                                        
                                        Text {
                                            text: modelData.message || "请求添加您为好友"
                                            color: themeManager.currentTheme.textSecondaryColor
                                            font.pixelSize: 14
                                        }
                                    }
                                    
                                    // 操作按钮
                                    RowLayout {
                                        spacing: 8
                                        
                                        Button {
                                            Layout.preferredWidth: 60
                                            Layout.preferredHeight: 32
                                            text: qsTr("同意")
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                                                       parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                                                       themeManager.currentTheme.primaryColor
                                                radius: 6
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.text
                                                color: "white"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            
                                            onClicked: acceptFriendRequest(modelData)
                                        }
                                        
                                        Button {
                                            Layout.preferredWidth: 60
                                            Layout.preferredHeight: 32
                                            text: qsTr("拒绝")
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? Qt.darker(themeManager.currentTheme.textSecondaryColor, 1.2) :
                                                       parent.hovered ? Qt.lighter(themeManager.currentTheme.textSecondaryColor, 1.1) :
                                                       themeManager.currentTheme.textSecondaryColor
                                                radius: 6
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.text
                                                color: "white"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            
                                            onClicked: rejectFriendRequest(modelData)
                                        }
                                    }
                                }
                            }
                        }
                        
                        // 空状态提示
                        Text {
                            anchors.centerIn: parent
                            text: friendRequests.length === 0 && !isLoadingRequests ? qsTr("暂无好友请求") : ""
                            color: themeManager.currentTheme.textSecondaryColor
                            font.pixelSize: 16
                            visible: text.length > 0
                        }
                    }
                }
            }
        }
    }
    

    
    // 搜索功能
    function performSearch() {
        var keyword = searchField.text.trim()
        // Starting search
        
        if (keyword.length === 0) {
            // Empty keyword, skipping search
            return
        }
        
        isSearching = true
        hasSearched = true
        searchResults = []
        
        // 发送搜索请求到服务器
        if (networkClient) {
                    // Checking network client
            
            if (typeof networkClient.searchUsers === 'function') {
                // Calling search method
                try {
                    networkClient.searchUsers(keyword, 20)
                    // Search request sent
                } catch (error) {
                    console.error("Error calling searchUsers:", error)
                    // 如果方法调用失败，重置搜索状态
                    isSearching = false
                    if (messageDialog) {
                        messageDialog.showError("搜索失败", "无法调用搜索功能：" + (error.message || error))
                    }
                }
            } else {
                console.error("searchUsers method does not exist!")
                isSearching = false
                if (messageDialog) {
                    messageDialog.showError("搜索失败", "搜索方法不存在")
                }
            }
        } else {
            console.error("networkClient is null or undefined")
            isSearching = false
            if (messageDialog) {
                messageDialog.showError("搜索失败", "网络客户端未初始化")
            }
        }
        // Search completed
    }
    
    // 处理搜索结果
    function handleSearchResults(results) {
        // Processing search results
        
        isSearching = false
        searchResults = results || []
        
        // Search results processed
    }
    
    // 处理搜索错误
    function handleSearchError(errorMessage) {
        // Processing search error
        
        isSearching = false
        if (messageDialog) {
            messageDialog.showError(qsTr("搜索失败"), errorMessage)
        }
    }
    
    // 重置搜索状态
    function resetSearchState() {
        searchField.clear()
        searchResults = []
        isSearching = false
        hasSearched = false
        // Search state reset
    }
    
    // 加载好友请求
    function loadFriendRequests() {
        isLoadingRequests = true
        friendRequests = []
        
        // 发送获取好友请求列表的请求到服务器
        if (networkClient) {
            // TODO: 实现获取好友请求列表的API调用
            // networkClient.getFriendRequests()
            
            // 模拟加载延迟
            loadRequestsTimer.start()
        }
    }
    
    // 模拟加载定时器
    Timer {
        id: loadRequestsTimer
        interval: 1000
        repeat: false
        onTriggered: {
            isLoadingRequests = false
            // 模拟数据
            friendRequests = [
                {
                    id: 1,
                    username: "test_user1",
                    display_name: "测试用户1",
                    message: "你好，我想加你为好友"
                },
                {
                    id: 2,
                    username: "test_user2",
                    display_name: "测试用户2",
                    message: "请求添加您为好友"
                }
            ]
        }
    }
    
    // 同意好友请求
    function acceptFriendRequest(requestData) {
        if (networkClient) {
            // TODO: 实现同意好友请求的API调用
            // networkClient.acceptFriendRequest(requestData.id)
            
            // 从列表中移除该请求
            var index = friendRequests.findIndex(function(item) {
                return item.id === requestData.id
            })
            if (index !== -1) {
                friendRequests.splice(index, 1)
            }
            
            if (messageDialog) {
                messageDialog.showSuccess("好友请求已同意", "已成功添加 " + (requestData.display_name || requestData.username) + " 为好友")
            }
        }
    }
    
    // 拒绝好友请求
    function rejectFriendRequest(requestData) {
        if (networkClient) {
            // TODO: 实现拒绝好友请求的API调用
            // networkClient.rejectFriendRequest(requestData.id)
            
            // 从列表中移除该请求
            var index = friendRequests.findIndex(function(item) {
                return item.id === requestData.id
            })
            if (index !== -1) {
                friendRequests.splice(index, 1)
            }
            
            if (messageDialog) {
                messageDialog.showInfo("好友请求已拒绝", "已拒绝 " + (requestData.display_name || requestData.username) + " 的好友请求")
            }
        }
    }
    
    // 窗口关闭事件
    onClosing: {
        resetSearchState()
    }
    
    // 窗口显示事件
    onVisibleChanged: {
        // Window visibility changed
        
        if (visible) {
            // 窗口显示时自动加载好友请求
            // Loading friend requests
            loadFriendRequests()
            // 确保搜索状态正确重置
            // Resetting search state
            resetSearchState()
                          // Window initialization completed
        }
    }
}
