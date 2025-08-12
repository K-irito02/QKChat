import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief 分组头部组件
 * 
 * 显示分组名称、成员数量，支持展开/收缩、右键菜单等操作
 */
Rectangle {
    id: root
    
    // 公共属性
    property string groupName: ""
    property int memberCount: 0
    property bool isExpanded: true
    property bool isDefault: false  // 是否为默认分组（不可删除）
    property var themeManager
    
    // 信号
    signal expandToggled()
    signal renameRequested()
    signal deleteRequested()
    signal addMemberRequested()
    
    height: 36
    color: mouseArea.containsMouse ? 
           Qt.rgba(themeManager.currentTheme.primaryColor.r,
                   themeManager.currentTheme.primaryColor.g,
                   themeManager.currentTheme.primaryColor.b, 0.1) : "transparent"
    radius: 6
    
    Behavior on color {
        ColorAnimation { duration: 150 }
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8
        
        // 展开/收缩图标
        Rectangle {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.alignment: Qt.AlignVCenter
            color: "transparent"
            
            Text {
                anchors.centerIn: parent
                text: isExpanded ? "▼" : "▶"
                color: themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 10
                
                Behavior on rotation {
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }
            }
        }
        
        // 分组名称
        Text {
            Layout.fillWidth: true
            text: groupName + (memberCount > 0 ? ` (${memberCount})` : "")
            color: themeManager.currentTheme.textPrimaryColor
            font.pixelSize: 13
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
        
        // 操作按钮（悬停时显示）
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: 4
            visible: mouseArea.containsMouse && !isDefault
            
            // 添加成员按钮
            Rectangle {
                width: 20
                height: 20
                radius: 10
                color: addMouseArea.containsMouse ? 
                       themeManager.currentTheme.primaryColor : "transparent"
                border.color: themeManager.currentTheme.primaryColor
                border.width: 1
                
                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: addMouseArea.containsMouse ? 
                           "white" : themeManager.currentTheme.primaryColor
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }
                
                MouseArea {
                    id: addMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: addMemberRequested()
                    
                    ToolTip.visible: containsMouse
                    ToolTip.text: qsTr("添加成员到此分组")
                    ToolTip.delay: 1000
                }
            }
        }
    }
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        onClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                expandToggled()
            } else if (mouse.button === Qt.RightButton) {
                contextMenu.popup()
            }
        }
    }
    
    // 右键菜单
    Menu {
        id: contextMenu
        
        MenuItem {
            text: qsTr("重命名分组")
            enabled: !isDefault
            onTriggered: renameRequested()
        }
        
        MenuItem {
            text: qsTr("添加成员")
            onTriggered: addMemberRequested()
        }
        
        MenuSeparator {
            visible: !isDefault
        }
        
        MenuItem {
            text: qsTr("删除分组")
            enabled: !isDefault
            onTriggered: deleteRequested()
        }
    }
}