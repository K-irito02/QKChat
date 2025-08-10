import QtQuick
import QtQuick.Controls

/**
 * @brief 自定义按钮组件
 *
 * 提供现代化的按钮设计，支持多种样式、主题切换、
 * 动画效果和加载状态。
 */
Button {
    id: customButton

    // 自定义属性
    property var themeManager
    property string buttonType: "primary" // primary, secondary, text, outline
    property bool isLoading: false
    property string iconSource: ""
    property bool iconOnRight: false
    property real buttonRadius: 12

    // 基础样式
    font.pixelSize: 16
    font.family: "Microsoft YaHei UI"
    font.weight: Font.Medium

    // 按钮背景
    background: Rectangle {
        id: backgroundRect
        radius: buttonRadius
        color: getBackgroundColor()
        border.color: getBorderColor()
        border.width: buttonType === "outline" ? 2 : 0

        // 悬停和按压效果
        opacity: {
            if (!customButton.enabled) return 0.5
            if (customButton.pressed) return 0.8
            if (customButton.hovered) return 0.9
            return 1.0
        }

        Behavior on opacity {
            OpacityAnimator { duration: 150; easing.type: Easing.OutCubic }
        }

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        // 涟漪效果
        Rectangle {
            id: ripple
            anchors.centerIn: parent
            width: 0
            height: 0
            radius: width / 2
            color: themeManager ? themeManager.currentTheme.rippleColor : "#007AFF20"
            opacity: 0

            SequentialAnimation {
                id: rippleAnimation
                PropertyAnimation {
                    target: ripple
                    properties: "width,height,opacity"
                    to: Math.max(backgroundRect.width, backgroundRect.height) * 2
                    duration: 300
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    target: ripple
                    property: "opacity"
                    to: 0
                    duration: 200
                    easing.type: Easing.OutCubic
                }
                ScriptAction {
                    script: {
                        ripple.width = 0
                        ripple.height = 0
                    }
                }
            }
        }

        // 阴影效果（仅主要按钮）
        Rectangle {
            visible: buttonType === "primary" && customButton.enabled
            anchors.fill: parent
            anchors.margins: -2
            color: "transparent"
            border.color: themeManager ? themeManager.currentTheme.shadowColor : "#00000020"
            border.width: 1
            radius: parent.radius + 2
            z: -1
            opacity: customButton.pressed ? 0.3 : 0.6

            Behavior on opacity {
                OpacityAnimator { duration: 150 }
            }
        }
    }

    // 按钮内容
    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 8
            layoutDirection: iconOnRight ? Qt.RightToLeft : Qt.LeftToRight

            // 加载指示器
            BusyIndicator {
                visible: isLoading
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                running: isLoading
            }

            // 图标
            Image {
                visible: iconSource !== "" && !isLoading
                anchors.verticalCenter: parent.verticalCenter
                source: iconSource
                width: 20
                height: 20
                fillMode: Image.PreserveAspectFit
            }

            // 文本
            Text {
                visible: customButton.text !== ""
                anchors.verticalCenter: parent.verticalCenter
                text: customButton.text
                font: customButton.font
                color: getTextColor()

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
            }
        }
    }

    // 鼠标区域（用于涟漪效果）
    MouseArea {
        anchors.fill: parent
        onPressed: function(mouse) {
            if (customButton.enabled) {
                rippleAnimation.start()
            }
            mouse.accepted = false
        }
    }

    // 获取背景颜色
    function getBackgroundColor() {
        if (!themeManager) return "#007AFF"

        switch (buttonType) {
        case "primary":
            return themeManager.currentTheme.buttonPrimaryColor
        case "secondary":
            return themeManager.currentTheme.buttonSecondaryColor
        case "text":
            return "transparent"
        case "outline":
            return "transparent"
        default:
            return themeManager.currentTheme.buttonPrimaryColor
        }
    }

    // 获取边框颜色
    function getBorderColor() {
        if (!themeManager) return "transparent"

        switch (buttonType) {
        case "outline":
            return themeManager.currentTheme.buttonPrimaryColor
        default:
            return "transparent"
        }
    }

    // 获取文本颜色
    function getTextColor() {
        if (!themeManager) return "#FFFFFF"

        switch (buttonType) {
        case "primary":
            return themeManager.currentTheme.buttonTextColor
        case "secondary":
            return themeManager.currentTheme.buttonSecondaryTextColor
        case "text":
            return themeManager.currentTheme.buttonSecondaryTextColor
        case "outline":
            return themeManager.currentTheme.buttonSecondaryTextColor
        default:
            return themeManager.currentTheme.buttonTextColor
        }
    }
}