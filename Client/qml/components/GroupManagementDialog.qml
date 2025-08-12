import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 分组管理对话框
 * 
 * 提供创建、重命名、删除分组的功能
 */
Dialog {
    id: root
    
    // 公共属性
    property var themeManager
    property string mode: "create"  // create, rename, delete
    property string groupName: ""
    property int groupId: -1
    property int memberCount: 0
    
    // 信号
    signal groupCreated(string name)
    signal groupRenamed(int groupId, string newName)
    signal groupDeleted(int groupId)
    
    title: {
        switch(mode) {
            case "create": return qsTr("创建分组")
            case "rename": return qsTr("重命名分组")
            case "delete": return qsTr("删除分组")
            default: return qsTr("分组管理")
        }
    }
    
    modal: true
    anchors.centerIn: parent
    width: 400
    height: mode === "delete" ? 200 : 250
    
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
                text: root.title
                color: themeManager.currentTheme.textPrimaryColor
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            
            Text {
                text: {
                    switch(mode) {
                        case "create": return qsTr("请输入新分组的名称")
                        case "rename": return qsTr("请输入新的分组名称")
                        case "delete": return qsTr("确定要删除此分组吗？")
                        default: return ""
                    }
                }
                color: themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 14
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
        
        // 删除确认信息
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: deleteInfo.contentHeight + 16
            color: Qt.rgba(themeManager.currentTheme.errorColor.r,
                           themeManager.currentTheme.errorColor.g,
                           themeManager.currentTheme.errorColor.b, 0.1)
            radius: 8
            border.color: Qt.rgba(themeManager.currentTheme.errorColor.r,
                                  themeManager.currentTheme.errorColor.g,
                                  themeManager.currentTheme.errorColor.b, 0.3)
            border.width: 1
            visible: mode === "delete"
            
            Text {
                id: deleteInfo
                anchors.fill: parent
                anchors.margins: 8
                text: memberCount > 0 ? 
                      qsTr("分组 \"%1\" 中有 %2 个成员，删除后这些成员将移动到默认分组。").arg(groupName).arg(memberCount) :
                      qsTr("分组 \"%1\" 为空分组，可以安全删除。").arg(groupName)
                color: themeManager.currentTheme.textPrimaryColor
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }
        }
        
        // 输入字段（创建和重命名模式）
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: mode !== "delete"
            
            Text {
                text: qsTr("分组名称")
                color: themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 13
                font.weight: Font.Medium
            }
            
            TextField {
                id: nameField
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: mode === "rename" ? groupName : ""
                placeholderText: qsTr("请输入分组名称")
                font.pixelSize: 14
                selectByMouse: true
                
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
                
                leftPadding: 12
                rightPadding: 12
                topPadding: 8
                bottomPadding: 8
                
                onAccepted: confirmButton.clicked()
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
                Layout.preferredWidth: 80
                Layout.preferredHeight: 40
                text: qsTr("取消")
                
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
                
                onClicked: root.reject()
            }
            
            // 确认按钮
            Button {
                id: confirmButton
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                text: {
                    switch(mode) {
                        case "create": return qsTr("创建")
                        case "rename": return qsTr("重命名")
                        case "delete": return qsTr("删除")
                        default: return qsTr("确定")
                    }
                }
                enabled: mode === "delete" || nameField.text.trim().length > 0
                
                background: Rectangle {
                    color: {
                        if (!parent.enabled) {
                            return Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                          themeManager.currentTheme.primaryColor.g,
                                          themeManager.currentTheme.primaryColor.b, 0.3)
                        }
                        if (mode === "delete") {
                            return parent.pressed ? Qt.darker(themeManager.currentTheme.errorColor, 1.2) :
                                   parent.hovered ? Qt.lighter(themeManager.currentTheme.errorColor, 1.1) :
                                   themeManager.currentTheme.errorColor
                        } else {
                            return parent.pressed ? Qt.darker(themeManager.currentTheme.primaryColor, 1.2) :
                                   parent.hovered ? Qt.lighter(themeManager.currentTheme.primaryColor, 1.1) :
                                   themeManager.currentTheme.primaryColor
                        }
                    }
                    radius: 8
                    
                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }
                
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "white" : Qt.rgba(255, 255, 255, 0.6)
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    switch(mode) {
                        case "create":
                            groupCreated(nameField.text.trim())
                            break
                        case "rename":
                            groupRenamed(groupId, nameField.text.trim())
                            break
                        case "delete":
                            groupDeleted(groupId)
                            break
                    }
                    root.accept()
                }
            }
        }
    }
    
    onOpened: {
        if (mode !== "delete") {
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
    }
    
    onRejected: {
        nameField.clear()
    }
    
    onAccepted: {
        nameField.clear()
    }
}
