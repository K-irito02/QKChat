import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 自定义下拉按钮组件
 *
 * 在收缩模式下，按钮分为左右两个可点击区域：
 * - 左侧区域：显示当前选中的选项名称，点击时触发对应功能
 * - 右侧区域：显示下拉箭头图标，点击时显示菜单
 */
Rectangle {
    id: customButton

    // 公共属性
    property var themeManager
    property bool isCollapsed: false
    property string currentText: "默认选项"
    property var menuItems: []
    property string buttonType: "navigation"  // "navigation" 或 "bottom"

    // 信号
    signal itemClicked(string itemName)
    signal menuItemSelected(string itemName)

    // 样式属性
    width: isCollapsed ? 60 : 200
    height: 30
    color: mouseArea.containsMouse ? themeManager.currentTheme.borderColor : "transparent"
    radius: 6
    border.color: themeManager.currentTheme.borderColor
    border.width: isCollapsed ? 1 : 0

    Behavior on color {
        ColorAnimation { duration: 150 }
    }

    // 主要内容区域
    RowLayout {
        anchors.fill: parent
        anchors.margins: isCollapsed ? 2 : 8
        spacing: 0

        // 左侧文本区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: leftMouseArea.containsMouse ? themeManager.currentTheme.primaryColor : "transparent"
            radius: 4
            
            Behavior on color {
                ColorAnimation { duration: 150 }
            }

            Text {
                anchors.centerIn: parent
                text: isCollapsed ? currentText.charAt(0) : currentText
                color: leftMouseArea.containsMouse ? "white" : themeManager.currentTheme.textPrimaryColor
                font.pixelSize: isCollapsed ? 12 : 13
                font.weight: Font.Medium
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: leftMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    customButton.itemClicked(currentText)
                }
            }

            ToolTip {
                visible: leftMouseArea.containsMouse && isCollapsed
                text: currentText
                delay: 500
            }
        }

        // 右侧下拉箭头区域（仅在收缩模式下显示）
        Rectangle {
            Layout.preferredWidth: isCollapsed ? 20 : 0
            Layout.fillHeight: true
            visible: isCollapsed
            color: rightMouseArea.containsMouse ? themeManager.currentTheme.secondaryColor : "transparent"
            radius: 4

            Behavior on color {
                ColorAnimation { duration: 150 }
            }

            Text {
                anchors.centerIn: parent
                text: "▼"
                color: rightMouseArea.containsMouse ? "white" : themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 8
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: rightMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    // 根据按钮类型决定菜单显示位置
                    if (buttonType === "bottom") {
                        // 底部按钮显示上拉菜单
                        dropdownMenu.popup(customButton, 0, -dropdownMenu.height - 2)
                    } else {
                        // 导航按钮显示下拉菜单
                        dropdownMenu.popup(customButton, 0, customButton.height + 2)
                    }
                }
            }

            ToolTip {
                visible: rightMouseArea.containsMouse
                text: "显示菜单"
                delay: 500
            }
        }
    }

    // 非收缩模式的鼠标区域
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        visible: !isCollapsed

        onClicked: {
            customButton.itemClicked(currentText)
        }
    }

    // 下拉菜单
    Menu {
        id: dropdownMenu

        Repeater {
            model: menuItems

            MenuItem {
                text: modelData
                visible: modelData !== currentText  // 隐藏当前选中的项
                height: visible ? implicitHeight : 0

                onTriggered: {
                    customButton.menuItemSelected(modelData)
                }

                background: Rectangle {
                    color: parent.hovered ? themeManager.currentTheme.primaryColor : themeManager.currentTheme.surfaceColor
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: parent.hovered ? "white" : themeManager.currentTheme.textPrimaryColor
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 4
                    bottomPadding: 4
                }
            }
        }

        width: Math.max(120, customButton.width)

        background: Rectangle {
            color: themeManager.currentTheme.surfaceColor
            border.color: themeManager.currentTheme.borderColor
            border.width: 1
            radius: 6

            // 添加阴影效果
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 2
                anchors.leftMargin: 2
                color: themeManager.currentTheme.shadowColor
                radius: parent.radius
                z: -1
                opacity: 0.2
            }
        }
    }

    // 工具提示（非收缩模式）
    ToolTip {
        visible: mouseArea.containsMouse && !isCollapsed
        text: currentText
        delay: 500
    }
}
