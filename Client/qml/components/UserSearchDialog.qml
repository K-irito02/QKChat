import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

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
    
    // 搜索状态
    property bool isSearching: false
    property var searchResults: []
    property bool hasSearched: false  // 添加搜索执行标记
    
    // 好友请求状态 - 从父组件接收
    property var friendRequests: []
    property bool isLoadingRequests: false
    
    // 分组管理
    property var friendGroups: []
    property bool isLoadingGroups: false
    
    // 信号
    signal userSelected(var userInfo)
    signal friendRequestSent(var userInfo)
    signal addFriendRequest(var userInfo)
    
    title: qsTr("添加好友")
    modality: Qt.NonModal
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
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
                                    viewUserDetail(userInfo)
                                }

                                onAddFriendClicked: {
                                    root.addFriendRequest(userInfo)
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
                                            text: modelData.requester_display_name || modelData.requester_username || "未知用户"
                                            color: themeManager.currentTheme.textPrimaryColor
                                            font.pixelSize: 16
                                            font.weight: Font.Medium
                                        }
                                        
                                        Text {
                                            text: {
                                                // 根据request_type和status来显示正确的信息
                                                if (modelData.request_type === "received") {
                                                    // 我收到的申请
                                                    if (modelData.status === "pending") {
                                                        return modelData.message || "请求添加您为好友"
                                                    } else if (modelData.status === "accepted") {
                                                        return "您已同意该好友请求"
                                                    } else if (modelData.status === "rejected") {
                                                        return "您已拒绝该好友请求"
                                                    } else if (modelData.status === "ignored") {
                                                        return "您已忽略该好友请求"
                                                    }
                                                } else if (modelData.request_type === "sent") {
                                                    // 我发送的申请
                                                    if (modelData.status === "pending") {
                                                        return "等待对方处理"
                                                    } else if (modelData.status === "cancelled") {
                                                        return "您已取消该好友请求"
                                                    }
                                                } else if (modelData.request_type === "sent_processed") {
                                                    // 我发送的已处理申请
                                                    if (modelData.status === "accepted") {
                                                        return "对方已同意您的好友请求"
                                                    } else if (modelData.status === "rejected") {
                                                        return "对方已拒绝您的好友请求"
                                                    } else if (modelData.status === "ignored") {
                                                        return "对方已忽略您的好友请求"
                                                    } else if (modelData.status === "cancelled") {
                                                        return "您已取消该好友请求"
                                                    }
                                                } else if (modelData.request_type === "received_processed") {
                                                    // 我收到的已处理申请
                                                    if (modelData.status === "accepted") {
                                                        return "您已同意该好友请求"
                                                    } else if (modelData.status === "rejected") {
                                                        return "您已拒绝该好友请求"
                                                    } else if (modelData.status === "ignored") {
                                                        return "您已忽略该好友请求"
                                                    }
                                                } else {
                                                    // 兼容旧版本
                                                    if (modelData.status === "rejected") {
                                                        return "好友请求已被拒绝"
                                                    } else if (modelData.status === "accepted") {
                                                        return "好友请求已同意"
                                                    } else if (modelData.status === "ignored") {
                                                        return "好友请求已被忽略"
                                                    } else if (modelData.status === "cancelled") {
                                                        return "好友请求已被取消"
                                                    } else {
                                                        return modelData.message || "请求添加您为好友"
                                                    }
                                                }
                                            }
                                            color: {
                                                if (modelData.status === "rejected") {
                                                    return themeManager.currentTheme.errorColor || "#e74c3c"
                                                } else if (modelData.status === "accepted") {
                                                    return themeManager.currentTheme.successColor || "#27ae60"
                                                } else if (modelData.status === "ignored") {
                                                    return themeManager.currentTheme.textSecondaryColor
                                                } else {
                                                    return themeManager.currentTheme.textSecondaryColor
                                                }
                                            }
                                            font.pixelSize: 14
                                        }
                                        
                                        // 状态图标
                                        RowLayout {
                                            spacing: 4
                                            visible: (modelData.status === "accepted" || modelData.status === "rejected" || modelData.status === "ignored") && 
                                                     (modelData.request_type === "sent_processed" || modelData.request_type === "received_processed")
                                            
                                            Rectangle {
                                                Layout.preferredWidth: 16
                                                Layout.preferredHeight: 16
                                                radius: 8
                                                color: {
                                                    if (modelData.status === "accepted") {
                                                        return themeManager.currentTheme.successColor || "#27ae60"
                                                    } else if (modelData.status === "rejected") {
                                                        return themeManager.currentTheme.errorColor || "#e74c3c"
                                                    } else {
                                                        return themeManager.currentTheme.textSecondaryColor
                                                    }
                                                }
                                                
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: {
                                                        if (modelData.status === "accepted") {
                                                            return "✓"
                                                        } else if (modelData.status === "rejected") {
                                                            return "✗"
                                                        } else {
                                                            return "○"
                                                        }
                                                    }
                                                    color: "white"
                                                    font.pixelSize: 10
                                                    font.weight: Font.Bold
                                                }
                                            }
                                            
                                            Text {
                                                text: {
                                                    if (modelData.status === "accepted") {
                                                        return qsTr("已同意")
                                                    } else if (modelData.status === "rejected") {
                                                        return qsTr("被拒绝")
                                                    } else {
                                                        return qsTr("已忽略")
                                                    }
                                                }
                                                color: {
                                                    if (modelData.status === "accepted") {
                                                        return themeManager.currentTheme.successColor || "#27ae60"
                                                    } else if (modelData.status === "rejected") {
                                                        return themeManager.currentTheme.errorColor || "#e74c3c"
                                                    } else {
                                                        return themeManager.currentTheme.textSecondaryColor
                                                    }
                                                }
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                            }
                                        }
                                    }
                                    
                                    // 操作按钮
                                    RowLayout {
                                        spacing: 6
                                        
                                        // 详情按钮 - 仅对收到的待处理申请显示
                                        Button {
                                            Layout.preferredWidth: 50
                                            Layout.preferredHeight: 28
                                            text: qsTr("详情")
                                            visible: modelData.status === "pending" && modelData.request_type === "received"
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? Qt.darker(themeManager.currentTheme.borderColor, 1.2) :
                                                       parent.hovered ? Qt.lighter(themeManager.currentTheme.borderColor, 1.1) :
                                                       themeManager.currentTheme.borderColor
                                                radius: 4
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.text
                                                color: themeManager.currentTheme.textPrimaryColor
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            
                                            onClicked: viewUserDetail(modelData)
                                        }
                                        
                                        // 忽略按钮 - 对所有类型的好友申请都可以忽略
                                        Button {
                                            Layout.preferredWidth: 50
                                            Layout.preferredHeight: 28
                                            text: {
                                                // 根据申请类型和状态显示不同的文本
                                                if (modelData.request_type === "sent" && modelData.status === "pending") {
                                                    return qsTr("取消") // 我发送的待处理申请
                                                } else if (modelData.request_type === "received" && modelData.status === "pending") {
                                                    return qsTr("忽略") // 我收到的待处理申请
                                                } else {
                                                    return qsTr("忽略") // 其他所有情况（清理通知）
                                                }
                                            }
                                            visible: {
                                                // 显示条件：
                                                // 1. 我发送的待处理申请 - 显示"取消"
                                                // 2. 我收到的待处理申请 - 显示"忽略"
                                                // 3. 已处理的申请 - 显示"忽略"（清理通知）
                                                return (modelData.request_type === "sent" && modelData.status === "pending") ||
                                                       (modelData.request_type === "received" && modelData.status === "pending") ||
                                                       (modelData.request_type === "sent_processed" || modelData.request_type === "received_processed")
                                            }
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? Qt.darker(themeManager.currentTheme.textSecondaryColor, 1.2) :
                                                       parent.hovered ? Qt.lighter(themeManager.currentTheme.textSecondaryColor, 1.1) :
                                                       themeManager.currentTheme.textSecondaryColor
                                                radius: 4
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.text
                                                color: "white"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            
                                            onClicked: ignoreFriendRequest(modelData)
                                        }
                                        
                                        // 同意按钮 - 仅对收到的待处理申请显示
                                        Button {
                                            Layout.preferredWidth: 50
                                            Layout.preferredHeight: 28
                                            text: qsTr("同意")
                                            visible: modelData.status === "pending" && modelData.request_type === "received"
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                                                       parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                                                       themeManager.currentTheme.primaryColor
                                                radius: 4
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.text
                                                color: "white"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            
                                            onClicked: showAcceptFriendDialog(modelData)
                                        }
                                        
                                        // 拒绝按钮 - 仅对收到的待处理申请显示
                                        Button {
                                            Layout.preferredWidth: 50
                                            Layout.preferredHeight: 28
                                            text: qsTr("拒绝")
                                            visible: modelData.status === "pending" && modelData.request_type === "received"
                                            
                                            background: Rectangle {
                                                color: parent.pressed ? Qt.darker(themeManager.currentTheme.textSecondaryColor, 1.2) :
                                                       parent.hovered ? Qt.lighter(themeManager.currentTheme.textSecondaryColor, 1.1) :
                                                       themeManager.currentTheme.textSecondaryColor
                                                radius: 4
                                            }
                                            
                                            contentItem: Text {
                                                text: parent.text
                                                color: "white"
                                                font.pixelSize: 11
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
                    // 如果方法调用失败，重置搜索状态
                    isSearching = false
                    if (messageDialog) {
                        messageDialog.showError("搜索失败", "无法调用搜索功能：" + (error.message || error))
                    }
                }
            } else {
                isSearching = false
                if (messageDialog) {
                    messageDialog.showError("搜索失败", "搜索方法不存在")
                }
            }
        } else {
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
        // 发送获取好友请求列表的请求到服务器
        if (networkClient) {
            console.log("正在获取好友请求列表...")
            networkClient.getFriendRequests()
            
            // 添加延迟刷新机制，确保数据同步
            var syncTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 500; repeat: false; running: true }', root, "syncTimer")
            syncTimer.triggered.connect(function() {
                console.log("执行好友请求列表同步刷新")
                networkClient.getFriendRequests()
            })
        }
    }
    
    // 同意好友请求
    function acceptFriendRequest(requestData) {
        if (networkClient) {
            console.log("同意好友请求:", JSON.stringify(requestData))
            
            // 确保request_id是数字类型
            var requestId = parseInt(requestData.request_id)
            console.log("转换后的request_id:", requestId, "类型:", typeof requestId)
            
            networkClient.respondToFriendRequest(requestId, true)
            
            // 通知父组件从列表中移除该请求
            var index = friendRequests.findIndex(function(item) {
                return item.request_id === requestData.request_id
            })
            if (index !== -1) {
                // 创建新的数组副本，移除指定项
                var newRequests = friendRequests.slice()
                newRequests.splice(index, 1)
                friendRequests = newRequests
            }
        }
    }
    
    // 拒绝好友请求
    function rejectFriendRequest(requestData) {
        if (networkClient) {
            console.log("拒绝好友请求:", JSON.stringify(requestData))
            
            // 确保request_id是数字类型
            var requestId = parseInt(requestData.request_id)
            console.log("转换后的request_id:", requestId, "类型:", typeof requestId)
            
            networkClient.respondToFriendRequest(requestId, false)
            
            // 立即从列表中移除该请求，避免重复显示
            var index = friendRequests.findIndex(function(item) {
                return item.request_id === requestData.request_id
            })
            if (index !== -1) {
                // 创建新的数组副本，移除指定项
                var newRequests = friendRequests.slice()
                newRequests.splice(index, 1)
                friendRequests = newRequests
            }
            
            // 延迟刷新好友请求列表，确保服务器状态同步
            var refreshTimer = Qt.createQmlObject('import QtQuick 2.15; Timer { interval: 1000; repeat: false; running: true }', root, "refreshTimer")
            refreshTimer.triggered.connect(function() {
                console.log("刷新好友请求列表以确保状态同步")
                loadFriendRequests()
            })
        }
    }
    
    // 忽略好友请求
    function ignoreFriendRequest(requestData) {
        if (networkClient) {
            console.log("忽略好友请求:", JSON.stringify(requestData))
            
            // 确保request_id是数字类型
            var requestId = parseInt(requestData.request_id)
            console.log("转换后的request_id:", requestId, "类型:", typeof requestId)
            console.log("申请类型:", requestData.request_type, "状态:", requestData.status)
            
            // 根据申请类型和状态决定处理方式
            if (requestData.request_type === "sent" && requestData.status === "pending") {
                // 我发送的待处理申请 - 取消申请
                console.log("取消我发送的好友申请")
                networkClient.ignoreFriendRequest(requestId)
            } else if (requestData.request_type === "received" && requestData.status === "pending") {
                // 我收到的待处理申请 - 忽略申请
                console.log("忽略我收到的好友申请")
                networkClient.ignoreFriendRequest(requestId)
            } else {
                // 已处理的申请 - 清理通知记录
                console.log("清理已处理的好友申请通知")
                networkClient.ignoreFriendRequest(requestId)
            }
            
            // 立即从列表中移除该请求
            var index = friendRequests.findIndex(function(item) {
                return item.request_id === requestData.request_id
            })
            if (index !== -1) {
                var newRequests = friendRequests.slice()
                newRequests.splice(index, 1)
                friendRequests = newRequests
            }
            
            // 显示相应的提示信息
            var actionText
            if (requestData.request_type === "sent" && requestData.status === "pending") {
                actionText = "已取消"
            } else if (requestData.request_type === "received" && requestData.status === "pending") {
                actionText = "已忽略"
            } else {
                actionText = "已清理"
            }
            
            if (messageDialog) {
                messageDialog.showInfo("操作成功", "好友申请" + actionText)
            }
        }
    }
    
    // 查看用户详情
    function viewUserDetail(userData) {
        // 判断数据类型并转换格式以适配UserDetailWindow
        var userInfo = {}
        
        // 检查是否为好友请求数据（新朋友页面）
        if (userData.requester_username || userData.requester_user_id) {
            userInfo = {
                user_id: userData.requester_user_id || userData.requester_id,
                username: userData.requester_username,
                display_name: userData.requester_display_name,
                avatar_url: userData.requester_avatar_url,
                bio: userData.requester_bio,
                created_at: userData.requested_at,
                online_status: userData.requester_online_status,
                friendship_status: "pending" // 好友请求状态
            }
        } else {
            // 搜索结果的用户数据
            userInfo = {
                user_id: userData.user_id || userData.id,
                username: userData.username,
                display_name: userData.display_name,
                avatar_url: userData.avatar_url,
                bio: userData.bio || userData.description,
                created_at: userData.created_at || userData.registration_date,
                online_status: userData.online_status,
                friendship_status: userData.friendship_status || "none"
            }
        }
        
        // 通过父组件显示用户详情窗口，这样可以统一管理窗口
        // 查找父组件中的showUserDetail函数
        var parent = root.parent
        while (parent) {
            if (typeof parent.showUserDetail === 'function') {
                parent.showUserDetail(userInfo)
                return
            }
            parent = parent.parent
        }
        
        // 如果找不到父组件的showUserDetail函数，则创建本地窗口
        var component = Qt.createComponent("qrc:/qml/windows/UserDetailWindow.qml")
        if (component.status === Component.Ready) {
            var userDetailWindow = component.createObject(null, {
                "themeManager": root.themeManager,
                "messageDialog": root.messageDialog
            })
            
            if (userDetailWindow) {
                // 设置窗口层级标志，确保在添加好友窗口之上
                userDetailWindow.flags = Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
                
                userDetailWindow.showUserDetail(userInfo)
                
                // 连接添加好友信号
                userDetailWindow.addFriendClicked.connect(function() {
                    // 处理从用户详情窗口发起的添加好友操作
                    if (userData.requester_username) {
                        // 好友请求状态，显示提示信息
                        if (messageDialog) {
                            messageDialog.showInfo("好友请求", "该用户已向您发送好友请求，请在新朋友页面处理")
                        }
                    } else {
                        // 搜索结果用户，触发添加好友流程
                        // 关闭用户详情窗口
                        userDetailWindow.close()
                        
                        // 触发添加好友流程，让父组件处理窗口管理
                        root.addFriendRequest(userData)
                    }
                })
                
                // 连接窗口关闭信号
                userDetailWindow.windowClosed.connect(function() {
                    // 窗口关闭时的清理工作
                    userDetailWindow.destroy()
                })
            }
        } else {
            console.error("Failed to create UserDetailWindow:", component.errorString())
        }
    }
    
    // 显示接受好友请求对话框
    function showAcceptFriendDialog(requestData) {
        acceptFriendDialog.requestData = requestData
        acceptFriendDialog.open()
    }
    
    // 处理接受好友请求（带备注和分组）
    function handleAcceptFriendWithSettings(requestData, note, groupName) {
        if (networkClient) {
            console.log("同意好友请求:", JSON.stringify(requestData), "备注:", note, "分组:", groupName)
            
            // 确保request_id是数字类型
            var requestId = parseInt(requestData.request_id)
            console.log("转换后的request_id:", requestId, "类型:", typeof requestId)
            
            // 发送接受请求，包含备注和分组信息
            networkClient.respondToFriendRequestWithSettings(requestId, true, note, groupName)
            
            // 从列表中移除该请求
            var index = friendRequests.findIndex(function(item) {
                return item.request_id === requestData.request_id
            })
            if (index !== -1) {
                var newRequests = friendRequests.slice()
                newRequests.splice(index, 1)
                friendRequests = newRequests
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
            // 加载好友分组
            loadFriendGroups()
            // 确保搜索状态正确重置
            // Resetting search state
            resetSearchState()
            // Window initialization completed
        }
    }
    
    // 加载好友分组
    function loadFriendGroups() {
        isLoadingGroups = true
        
        // 这里应该从服务器或本地存储加载用户的好友分组
        // 暂时保持为空等待网络客户端集成
        friendGroups = []
        
        isLoadingGroups = false
        
        // 刷新分组模型
        if (groupComboBox) {
            groupComboBox.model = groupComboBox.getGroupModel()
        }
    }
    
    // 创建新分组
    function createNewGroup(groupName) {
        if (!groupName || groupName.trim() === "") return
        
        // 检查分组是否已存在
        for (var i = 0; i < friendGroups.length; i++) {
            if (friendGroups[i].name === groupName.trim()) {
                return
            }
        }
        
        // 创建新分组
        var newGroup = {
            id: Date.now(), // 临时ID
            name: groupName.trim(),
            count: 0
        }
        
        friendGroups.push(newGroup)
        
        // 刷新分组模型
        if (groupComboBox) {
            groupComboBox.model = groupComboBox.getGroupModel()
        }
        
        // 这里应该将新分组保存到服务器
        console.log("创建新分组:", newGroup)
    }
    
    // 接受好友请求对话框
    Dialog {
        id: acceptFriendDialog
        title: qsTr("接受好友请求")
        modal: true
        anchors.centerIn: parent
        width: 400
        height: 300
        
        property var requestData: null
        
        background: Rectangle {
            color: themeManager.currentTheme.surfaceColor
            radius: 8
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
        }
        
        contentItem: ColumnLayout {
            spacing: 16
            
            // 用户信息
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                
                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    color: themeManager.currentTheme.primaryColor
                    
                    Text {
                        anchors.centerIn: parent
                        text: (acceptFriendDialog.requestData && (acceptFriendDialog.requestData.requester_username || acceptFriendDialog.requestData.requester_display_name)) ? 
                              (acceptFriendDialog.requestData.requester_username || acceptFriendDialog.requestData.requester_display_name).charAt(0).toUpperCase() : "U"
                        color: "white"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                }
                
                ColumnLayout {
                    spacing: 4
                    
                    Text {
                        text: acceptFriendDialog.requestData ? (acceptFriendDialog.requestData.requester_display_name || acceptFriendDialog.requestData.requester_username) : "未知用户"
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 16
                        font.weight: Font.Medium
                    }
                    
                    Text {
                        text: acceptFriendDialog.requestData ? (acceptFriendDialog.requestData.message || "请求添加您为好友") : ""
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                    }
                }
            }
            
            // 备注输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("备注名称（可选）")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }
                
                TextField {
                    id: noteField
                    Layout.fillWidth: true
                    placeholderText: qsTr("请输入备注名称")
                    text: acceptFriendDialog.requestData ? (acceptFriendDialog.requestData.requester_display_name || acceptFriendDialog.requestData.requester_username) : ""
                    
                    background: Rectangle {
                        color: "transparent"
                        border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.borderColor
                        border.width: 1
                        radius: 4
                    }
                }
            }
            
            // 分组选择
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("分组（可选）")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    ComboBox {
                        id: groupComboBox
                        Layout.fillWidth: true
                        model: getGroupModel()
                        editable: true
                        displayText: currentIndex === -1 ? qsTr("选择或输入分组名称") : currentText
                        
                        // 获取分组模型，包含系统默认分组和用户自定义分组
                        function getGroupModel() {
                            var groups = [qsTr("默认分组")]
                            
                            // 添加系统默认分组
                            var systemGroups = [qsTr("家人"), qsTr("朋友"), qsTr("同事"), qsTr("同学"), qsTr("重要联系人")]
                            
                            // 添加用户自定义分组
                            if (friendGroups && friendGroups.length > 0) {
                                for (var i = 0; i < friendGroups.length; i++) {
                                    var group = friendGroups[i]
                                    if (group && group.name && systemGroups.indexOf(group.name) === -1) {
                                        groups.push(group.name)
                                    }
                                }
                            }
                            
                            // 添加系统默认分组（如果用户还没有自定义这些分组）
                            for (var j = 0; j < systemGroups.length; j++) {
                                if (groups.indexOf(systemGroups[j]) === -1) {
                                    groups.push(systemGroups[j])
                                }
                            }
                            
                            return groups
                        }
                        
                        background: Rectangle {
                            color: "transparent"
                            border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.borderColor
                            border.width: 1
                            radius: 4
                        }
                    }
                }
            }
        }
        
        footer: RowLayout {
            spacing: 12
            
            Item {
                Layout.fillWidth: true
            }
            
            Button {
                text: qsTr("取消")
                
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(themeManager.currentTheme.borderColor, 1.2) :
                           parent.hovered ? Qt.lighter(themeManager.currentTheme.borderColor, 1.1) :
                           themeManager.currentTheme.borderColor
                    radius: 4
                }
                
                contentItem: Text {
                    text: parent.text
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: acceptFriendDialog.close()
            }
            
            Button {
                text: qsTr("确定")
                
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                           parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                           themeManager.currentTheme.primaryColor
                    radius: 4
                }
                
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    var note = noteField.text.trim()
                    var selectedGroup = groupComboBox.currentText.trim()
                    
                    // 如果用户输入了新分组名称，创建新分组
                    if (selectedGroup && selectedGroup !== qsTr("默认分组")) {
                        var isNewGroup = true
                        
                        // 检查是否为系统默认分组
                        var systemGroups = [qsTr("家人"), qsTr("朋友"), qsTr("同事"), qsTr("同学"), qsTr("重要联系人")]
                        if (systemGroups.indexOf(selectedGroup) !== -1) {
                            isNewGroup = false
                        }
                        
                        // 检查是否在用户自定义分组中
                        if (friendGroups && friendGroups.length > 0) {
                            for (var i = 0; i < friendGroups.length; i++) {
                                if (friendGroups[i] && friendGroups[i].name === selectedGroup) {
                                    isNewGroup = false
                                    break
                                }
                            }
                        }
                        
                        // 如果是新分组，创建它
                        if (isNewGroup) {
                            createNewGroup(selectedGroup)
                        }
                    }
                    
                    var groupName = selectedGroup || qsTr("默认分组")
                    
                    handleAcceptFriendWithSettings(acceptFriendDialog.requestData, note, groupName)
                    acceptFriendDialog.close()
                }
            }
        }
    }
}

