import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

/**
 * @brief 添加好友窗口
 * 
 * 用户点击"加为好友"后弹出的确认窗口
 * 包含备注名称、好友分组、验证信息等字段
 */
Window {
    id: root
    
    // 公共属性
    property var targetUser: ({})
    property var themeManager
    property var messageDialog
    
    // 分组管理
    property var friendGroups: []
    property bool isLoadingGroups: false
    
    // 信号
    signal friendRequestConfirmed(var requestData)
    signal windowClosed()
    
    title: qsTr("发送好友请求")
    width: 500
    height: 600
    minimumWidth: 450
    minimumHeight: 550
    visible: false
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    modality: Qt.NonModal
    
    Component.onCompleted: {
        // AddFriendWindow component completed
    }
    
    // 窗口关闭事件
    onClosing: {
        windowClosed()
    }
    
    // 窗口可见性变化事件 - 子组件方式，Qt自动处理层级
    onVisibleChanged: {
        // 子组件方式，Qt自动处理层级，无需手动管理
    }
    
    // 窗口激活状态变化事件 - 子组件方式，Qt自动处理层级
    onActiveChanged: {
        // 子组件方式，Qt自动处理层级，无需手动管理
    }
    
    // 设置窗口背景色
    color: themeManager.currentTheme.backgroundColor
    
    // 主内容容器
    Rectangle {
        anchors.fill: parent
        color: themeManager.currentTheme.backgroundColor
        
        ScrollView {
            anchors.fill: parent
            anchors.margins: 24
            clip: true
        
        ColumnLayout {
            width: parent.width
            spacing: 24
            
            // 标题区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("发送好友请求")
                    color: themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 20
                    font.weight: Font.Bold
                }
                
                Text {
                    text: qsTr("向 %1 发送好友请求").arg(targetUser && (targetUser.display_name || targetUser.username) ? (targetUser.display_name || targetUser.username) : qsTr("该用户"))
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 14
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
            
            // 目标用户信息预览
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: Qt.rgba(themeManager.currentTheme.primaryColor.r,
                               themeManager.currentTheme.primaryColor.g,
                               themeManager.currentTheme.primaryColor.b, 0.1)
                radius: 12
                border.color: Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                     themeManager.currentTheme.primaryColor.g,
                                     themeManager.currentTheme.primaryColor.b, 0.3)
                border.width: 1
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16
                    
                    // 用户头像
                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        radius: 24
                        color: themeManager.currentTheme.primaryColor
                        border.color: "#4CAF50"
                        border.width: 2
                        
                        // 头像图片（如果有的话）
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: parent.radius - 2
                            visible: targetUser && targetUser.avatar_url && typeof targetUser.avatar_url === "string" && targetUser.avatar_url.length > 0
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: (targetUser && targetUser.avatar_url && typeof targetUser.avatar_url === "string") ? targetUser.avatar_url : ""
                                fillMode: Image.PreserveAspectCrop

                                onStatusChanged: {
                                    if (status === Image.Error) {
                                        parent.visible = false
                                    }
                                }
                            }
                        }
                        
                        // 默认头像文字
                        Text {
                            anchors.centerIn: parent
                            text: (targetUser && (targetUser.display_name || targetUser.username) ? (targetUser.display_name || targetUser.username) : "U").charAt(0).toUpperCase()
                            color: "white"
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            visible: !(targetUser && targetUser.avatar_url && typeof targetUser.avatar_url === "string" && targetUser.avatar_url.length > 0)
                        }
                    }
                    
                    // 用户信息
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4
                        
                        Text {
                            text: targetUser && (targetUser.display_name || targetUser.username) ? (targetUser.display_name || targetUser.username) : qsTr("未知用户")
                            color: themeManager.currentTheme.textPrimaryColor
                            font.pixelSize: 16
                            font.weight: Font.Medium
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        
                        Text {
                            text: qsTr("ID: %1").arg(targetUser && targetUser.user_id ? targetUser.user_id : "")
                            color: themeManager.currentTheme.textSecondaryColor
                            font.pixelSize: 12
                        }
                    }
                }
            }
            
            // 输入字段区域
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 20
                
                // 备注名称
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Text {
                        text: qsTr("备注名称") + " " + qsTr("(可选)")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    
                    TextField {
                        id: remarkField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        placeholderText: qsTr("为好友设置备注名称")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        
                        background: Rectangle {
                            color: "transparent"
                            border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.borderColor
                            border.width: parent.activeFocus ? 2 : 1
                            radius: 8
                            
                            Behavior on border.color {
                                ColorAnimation { duration: 150 }
                            }
                        }
                    }
                }
                
                // 好友分组
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Text {
                        text: qsTr("好友分组") + " " + qsTr("(可选)")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    
                    ComboBox {
                        id: groupComboBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        model: getGroupModel()
                        editable: true
                        currentIndex: 0
                        displayText: currentText || qsTr("选择或输入分组名称")
                        
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
                            border.width: parent.activeFocus ? 2 : 1
                            radius: 8
                            
                            Behavior on border.color {
                                ColorAnimation { duration: 150 }
                            }
                        }
                        
                        contentItem: Text {
                            text: parent.displayText
                            color: themeManager.currentTheme.textPrimaryColor
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                            rightPadding: parent.indicator.width + parent.spacing
                        }
                        
                        indicator: Rectangle {
                            x: parent.width - width - parent.rightPadding
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            width: 12
                            height: 8
                            color: "transparent"
                            
                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.strokeStyle = themeManager.currentTheme.textSecondaryColor
                                    ctx.lineWidth = 2
                                    ctx.beginPath()
                                    ctx.moveTo(0, 0)
                                    ctx.lineTo(width / 2, height)
                                    ctx.lineTo(width, 0)
                                    ctx.stroke()
                                }
                            }
                        }
                        
                        popup: Popup {
                            y: parent.height
                            width: parent.width
                            implicitHeight: contentItem.implicitHeight
                            padding: 1
                            
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: parent.model
                                currentIndex: parent.highlightedIndex
                                
                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: 36
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 8
                                        
                                        Text {
                                            text: modelData
                                            color: themeManager.currentTheme.textPrimaryColor
                                            font.pixelSize: 14
                                            verticalAlignment: Text.AlignVCenter
                                            Layout.fillWidth: true
                                        }
                                        
                                        // 显示新分组标识
                                        Text {
                                            text: qsTr("新建")
                                            color: themeManager.currentTheme.primaryColor
                                            font.pixelSize: 10
                                            font.weight: Font.Medium
                                            visible: isNewGroup(modelData)
                                        }
                                    }
                                    
                                    background: Rectangle {
                                        color: parent.highlighted ? Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                                                          themeManager.currentTheme.primaryColor.g,
                                                                          themeManager.currentTheme.primaryColor.b, 0.1) : "transparent"
                                    }
                                    
                                    // 检查是否为新分组
                                    function isNewGroup(groupName) {
                                        if (!groupName || groupName === qsTr("默认分组")) return false
                                        
                                        var systemGroups = [qsTr("家人"), qsTr("朋友"), qsTr("同事"), qsTr("同学"), qsTr("重要联系人")]
                                        if (systemGroups.indexOf(groupName) !== -1) return false
                                        
                                        // 检查是否在用户自定义分组中
                                        if (friendGroups && friendGroups.length > 0) {
                                            for (var i = 0; i < friendGroups.length; i++) {
                                                if (friendGroups[i] && friendGroups[i].name === groupName) {
                                                    return false
                                                }
                                            }
                                        }
                                        
                                        return true
                                    }
                                }
                                
                                highlight: Rectangle {
                                    color: Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                                  themeManager.currentTheme.primaryColor.g,
                                                  themeManager.currentTheme.primaryColor.b, 0.1)
                                }
                                
                                ScrollIndicator.vertical: ScrollIndicator { }
                            }
                            
                            background: Rectangle {
                                color: themeManager.currentTheme.surfaceColor
                                border.color: themeManager.currentTheme.borderColor
                                border.width: 1
                                radius: 8
                            }
                        }
                    }
                }
                
                // 验证信息
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Text {
                        text: qsTr("验证信息") + " " + qsTr("(可选)")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }
                    
                    TextArea {
                        id: messageField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        placeholderText: qsTr("我是...")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        wrapMode: TextArea.Wrap
                        
                        background: Rectangle {
                            color: "transparent"
                            border.color: parent.activeFocus ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.borderColor
                            border.width: parent.activeFocus ? 2 : 1
                            radius: 8
                            
                            Behavior on border.color {
                                ColorAnimation { duration: 150 }
                            }
                        }
                    }
                }
            }
            
            // 按钮区域
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                spacing: 16
                
                // 取消按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 40
                    text: qsTr("取消")
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(themeManager.currentTheme.surfaceColor, 1.1) :
                               parent.hovered ? Qt.lighter(themeManager.currentTheme.surfaceColor, 1.05) :
                               themeManager.currentTheme.surfaceColor
                        radius: 8
                        border.color: themeManager.currentTheme.borderColor
                        border.width: 1
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        clearFields()
                        root.close()
                    }
                }
                
                // 发送请求按钮
                Button {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 40
                    text: qsTr("发送请求")
                    
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                               parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                               themeManager.currentTheme.primaryColor
                        radius: 8
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
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
                    
                    onClicked: {
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
                        
                        var requestData = {
                            target_user_id: targetUser && targetUser.id ? targetUser.id : null,
                            target_user_identifier: targetUser && (targetUser.user_id || targetUser.username) ? (targetUser.user_id || targetUser.username) : "",
                            remark: remarkField.text.trim(),
                            group: selectedGroup || qsTr("默认分组"),
                            message: messageField.text.trim()
                        }
                        
                        friendRequestConfirmed(requestData)
                        clearFields()
                        root.close()
                    }
                }
            }
        }
        }
    }
    
    // 清除字段函数
    function clearFields() {
        remarkField.clear()
        if (groupComboBox && typeof groupComboBox.currentIndex !== "undefined") {
            groupComboBox.currentIndex = 0
        }
        messageField.clear()
    }
    
    // 窗口显示函数
    function showAddFriend(user) {
        targetUser = user || {}
        clearFields()
        loadFriendGroups()
        show()
        // 子组件方式，Qt自动处理层级，无需手动管理
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
}
