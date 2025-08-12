import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 添加好友确认对话框
 * 
 * 用户点击"加为好友"后弹出的确认对话框
 * 包含备注名称、好友分组、验证信息等字段
 */
Dialog {
    id: root
    
    // 公共属性
    property var targetUser: ({})
    property var themeManager
    property var messageDialog
    
    // 信号
    signal friendRequestConfirmed(var requestData)
    
    title: qsTr("添加好友")
    modal: true
    anchors.centerIn: parent
    width: 450
    height: 500
    
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
                text: qsTr("发送好友请求")
                color: themeManager.currentTheme.textPrimaryColor
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            
            Text {
                text: qsTr("向 %1 发送好友请求").arg(targetUser.display_name || targetUser.username || qsTr("该用户"))
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
                           themeManager.currentTheme.primaryColor.b, 0.05)
            radius: 12
            border.color: Qt.rgba(themeManager.currentTheme.primaryColor.r,
                                  themeManager.currentTheme.primaryColor.g,
                                  themeManager.currentTheme.primaryColor.b, 0.2)
            border.width: 1
            
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                
                // 用户头像
                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    Layout.alignment: Qt.AlignVCenter
                    
                    color: themeManager.currentTheme.primaryColor
                    radius: 28
                    border.color: themeManager.currentTheme.successColor
                    border.width: 2
                    
                    Text {
                        anchors.centerIn: parent
                        text: targetUser.username ? targetUser.username.charAt(0).toUpperCase() : "U"
                        color: "white"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                    }
                    
                    // 头像图片（如果有的话）
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: parent.radius - 2
                        visible: targetUser.avatar_url && typeof targetUser.avatar_url === "string" && targetUser.avatar_url.length > 0
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: (targetUser.avatar_url && typeof targetUser.avatar_url === "string") ? targetUser.avatar_url : ""
                            fillMode: Image.PreserveAspectCrop

                            onStatusChanged: {
                                if (status === Image.Error) {
                                    parent.visible = false
                                }
                            }
                        }
                    }
                }
                
                // 用户信息
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 4
                    
                    Text {
                        text: targetUser.display_name || targetUser.username || qsTr("未知用户")
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    
                    Text {
                        text: qsTr("ID: %1").arg(targetUser.user_id || "")
                        color: themeManager.currentTheme.textSecondaryColor
                        font.pixelSize: 12
                    }
                }
            }
        }
        
        // 输入字段区域
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            
            // 备注名称
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("备注名称（可选）")
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }
                
                TextField {
                    id: remarkField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: qsTr("为好友设置备注名称")
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
                    
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                }
            }
            
            // 好友分组
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("好友分组（可选）")
                    color: themeManager.currentTheme.textSecondaryColor
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }
                
                ComboBox {
                    id: groupComboBox
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    
                    model: [qsTr("默认分组"), qsTr("同事"), qsTr("朋友"), qsTr("家人"), qsTr("同学")]
                    
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
                    
                    contentItem: Text {
                        text: parent.displayText
                        color: themeManager.currentTheme.textPrimaryColor
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                        rightPadding: 12
                    }
                }
            }
            
            // 验证信息
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                
                Text {
                    text: qsTr("验证信息（可选）")
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
                        
                        leftPadding: 12
                        rightPadding: 12
                        topPadding: 8
                        bottomPadding: 8
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
                    var requestData = {
                        target_user_id: targetUser.id,
                        target_user_identifier: targetUser.user_id || targetUser.username,
                        remark: remarkField.text.trim(),
                        group: groupComboBox.currentText,
                        message: messageField.text.trim()
                    }
                    
                    friendRequestConfirmed(requestData)
                    root.accept()
                }
            }
        }
    }
    
    onRejected: {
        remarkField.clear()
        groupComboBox.currentIndex = 0
        messageField.clear()
    }
    
    onAccepted: {
        remarkField.clear()
        groupComboBox.currentIndex = 0
        messageField.clear()
    }
}
