import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 好友列表视图组件
 * 显示用户的好友列表，包括在线状态、头像、昵称等信息
 */
Rectangle {
    id: root
    
    // 公共属性
    property alias model: friendListView.model
    property bool showOnlineOnly: false
    property string searchText: ""
    
    // 信号
    signal friendClicked(var friendInfo)
    signal friendDoubleClicked(var friendInfo)
    signal friendContextMenu(var friendInfo, point position)
    signal addFriendClicked()
    
    color: ThemeManager.backgroundColor
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        
        // 顶部工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            
            // 搜索框
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("搜索好友...")
                text: root.searchText
                
                onTextChanged: {
                    root.searchText = text
                    friendListModel.filterText = text
                }
                
                background: Rectangle {
                    color: ThemeManager.inputBackgroundColor
                    border.color: searchField.activeFocus ? ThemeManager.accentColor : ThemeManager.borderColor
                    border.width: 1
                    radius: 6
                }
            }
            
            // 添加好友按钮
            Button {
                id: addFriendBtn
                text: qsTr("添加好友")
                
                onClicked: root.addFriendClicked()
                
                background: Rectangle {
                    color: addFriendBtn.pressed ? Qt.darker(ThemeManager.accentColor, 1.2) : 
                           addFriendBtn.hovered ? Qt.lighter(ThemeManager.accentColor, 1.1) : ThemeManager.accentColor
                    radius: 6
                }
                
                contentItem: Text {
                    text: addFriendBtn.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 14
                }
            }
            
            // 在线状态过滤按钮
            Button {
                id: onlineFilterBtn
                text: root.showOnlineOnly ? qsTr("显示全部") : qsTr("仅在线")
                checkable: true
                checked: root.showOnlineOnly
                
                onClicked: {
                    root.showOnlineOnly = !root.showOnlineOnly
                    friendListModel.showOnlineOnly = root.showOnlineOnly
                }
                
                background: Rectangle {
                    color: onlineFilterBtn.checked ? ThemeManager.accentColor : 
                           onlineFilterBtn.pressed ? Qt.darker(ThemeManager.buttonColor, 1.2) : 
                           onlineFilterBtn.hovered ? Qt.lighter(ThemeManager.buttonColor, 1.1) : ThemeManager.buttonColor
                    border.color: ThemeManager.borderColor
                    border.width: 1
                    radius: 6
                }
                
                contentItem: Text {
                    text: onlineFilterBtn.text
                    color: onlineFilterBtn.checked ? "white" : ThemeManager.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 14
                }
            }
        }
        
        // 好友列表
        ListView {
            id: friendListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            clip: true
            spacing: 2
            
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
            
            delegate: FriendListItem {
                width: friendListView.width
                friendInfo: model
                
                onClicked: root.friendClicked(friendInfo)
                onDoubleClicked: root.friendDoubleClicked(friendInfo)
                onRightClicked: root.friendContextMenu(friendInfo, position)
            }
            
            // 空状态提示
            Label {
                anchors.centerIn: parent
                visible: friendListView.count === 0
                text: root.searchText !== "" ? qsTr("未找到匹配的好友") : 
                      root.showOnlineOnly ? qsTr("暂无在线好友") : qsTr("暂无好友")
                color: ThemeManager.secondaryTextColor
                font.pixelSize: 16
            }
        }
    }
    
    // 好友列表数据模型
    ListModel {
        id: friendListModel
        
        property string filterText: ""
        property bool showOnlineOnly: false
        
        // 过滤函数
        function applyFilter() {
            // 这里应该连接到C++的好友数据模型
            // 实际实现中会通过NetworkClient获取好友列表数据
        }
        
        // 添加好友到列表
        function addFriend(friendData) {
            append(friendData)
        }
        
        // 更新好友信息
        function updateFriend(friendId, friendData) {
            for (var i = 0; i < count; i++) {
                if (get(i).friend_id === friendId) {
                    set(i, friendData)
                    break
                }
            }
        }
        
        // 移除好友
        function removeFriend(friendId) {
            for (var i = 0; i < count; i++) {
                if (get(i).friend_id === friendId) {
                    remove(i)
                    break
                }
            }
        }
        
        // 更新好友在线状态
        function updateFriendStatus(friendId, status, lastSeen) {
            for (var i = 0; i < count; i++) {
                if (get(i).friend_id === friendId) {
                    setProperty(i, "online_status", status)
                    setProperty(i, "last_seen", lastSeen)
                    break
                }
            }
        }
    }
    
    // 连接到NetworkClient的信号
    Connections {
        target: NetworkClient
        
        function onFriendListReceived(friends) {
            friendListModel.clear()
            for (var i = 0; i < friends.length; i++) {
                friendListModel.addFriend(friends[i])
            }
        }
        
        function onFriendStatusChanged(friendId, status, lastSeen) {
            friendListModel.updateFriendStatus(friendId, status, lastSeen)
        }
        
        function onFriendAdded(friendData) {
            friendListModel.addFriend(friendData)
        }
        
        function onFriendRemoved(friendId) {
            friendListModel.removeFriend(friendId)
        }
    }
    
    // 组件初始化时加载好友列表
    Component.onCompleted: {
        if (NetworkClient.isConnected) {
            NetworkClient.getFriendList()
        }
    }
}
