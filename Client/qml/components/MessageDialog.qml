import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 自定义消息对话框组件
 *
 * 提供现代化的消息对话框设计，支持不同类型的消息显示、
 * 主题切换和动画效果。
 */
Dialog {
    id: messageDialog

    // 自定义属性
    property var themeManager
    property string messageType: "info" // success, error, warning, info
    property string messageTitle: ""
    property string messageText: ""
    property bool showIcon: true
    property bool autoClose: false
    property int autoCloseDelay: 3000

    // 对话框属性
    modal: true
    anchors.centerIn: parent
    width: Math.min(400, parent ? parent.width - 40 : 400)

    // 背景
    background: Rectangle {
        color: themeManager ? themeManager.currentTheme.cardColor : "#FFFFFF"
        radius: 16
        border.color: themeManager ? themeManager.currentTheme.borderColor : "#E0E0E0"
        border.width: 1

        // 阴影效果
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            color: "transparent"
            border.color: themeManager ? themeManager.currentTheme.shadowColor : "#00000020"
            border.width: 1
            radius: parent.radius + 4
            z: -1
        }
    }

    // 内容区域
    contentItem: ColumnLayout {
        spacing: 16

        // 标题和图标区域
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // 图标
            Rectangle {
                visible: showIcon
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: 20
                color: getIconBackgroundColor()

                Text {
                    anchors.centerIn: parent
                    text: getIconText()
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: "white"
                }
            }

            // 标题
            Text {
                visible: messageTitle !== ""
                Layout.fillWidth: true
                text: messageTitle
                font.pixelSize: 18
                font.weight: Font.Bold
                color: themeManager ? themeManager.currentTheme.textPrimaryColor : "#000000"
                wrapMode: Text.WordWrap
            }
        }

        // 消息内容
        Text {
            Layout.fillWidth: true
            text: messageText
            font.pixelSize: 14
            color: themeManager ? themeManager.currentTheme.textSecondaryColor : "#666666"
            wrapMode: Text.WordWrap
            lineHeight: 1.4
        }

        // 按钮区域
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8

            Item { Layout.fillWidth: true }

            Button {
                text: "确定"
                onClicked: messageDialog.close()

                background: Rectangle {
                    color: themeManager ? themeManager.currentTheme.buttonPrimaryColor : "#007AFF"
                    radius: 8
                    opacity: parent.pressed ? 0.8 : 1.0

                    Behavior on opacity {
                        OpacityAnimator { duration: 150 }
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
            }
        }
    }

    // 自动关闭定时器
    Timer {
        id: autoCloseTimer
        interval: autoCloseDelay
        onTriggered: messageDialog.close()
    }

    // 打开动画
    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 200
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.8
                to: 1.0
                duration: 200
                easing.type: Easing.OutBack
            }
        }
    }

    // 关闭动画
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 150
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.9
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
    }

    // 公共方法
    function showSuccess(title, message) {
        messageType = "success"
        messageTitle = title
        messageText = message
        open()
        if (autoClose) {
            autoCloseTimer.start()
        }
    }

    function showError(title, message) {
        messageType = "error"
        messageTitle = title
        messageText = message
        open()
    }

    function showWarning(title, message) {
        messageType = "warning"
        messageTitle = title
        messageText = message
        open()
    }

    function showInfo(title, message) {
        messageType = "info"
        messageTitle = title
        messageText = message
        open()
        if (autoClose) {
            autoCloseTimer.start()
        }
    }

    // 获取图标背景颜色
    function getIconBackgroundColor() {
        if (!themeManager) return "#007AFF"

        switch (messageType) {
        case "success":
            return themeManager.currentTheme.successColor
        case "error":
            return themeManager.currentTheme.errorColor
        case "warning":
            return themeManager.currentTheme.warningColor
        case "info":
        default:
            return themeManager.currentTheme.infoColor
        }
    }

    // 获取图标文本
    function getIconText() {
        switch (messageType) {
        case "success":
            return "✓"
        case "error":
            return "✕"
        case "warning":
            return "!"
        case "info":
        default:
            return "i"
        }
    }

    // 关闭时停止定时器
    onClosed: {
        autoCloseTimer.stop()
    }
}