import QtQuick
import QtQuick.Controls

/**
 * @brief 自定义输入框组件
 *
 * 提供现代化的输入框设计，支持主题切换、验证状态、
 * 动画效果和各种输入类型。
 */
TextField {
    id: customTextField

    // 自定义属性
    property var themeManager
    property bool hasError: false
    property string errorMessage: ""
    property bool showClearButton: false
    property string iconSource: ""
    property bool isPassword: false

    // 基础样式
    font.pixelSize: 16
    font.family: "Microsoft YaHei UI"
    color: themeManager ? themeManager.currentTheme.textPrimaryColor : "#000000"
    placeholderTextColor: themeManager ? themeManager.currentTheme.inputPlaceholderColor : "#999999"
    selectByMouse: true
    selectionColor: themeManager ? themeManager.currentTheme.selectionColor : "#007AFF30"
    verticalAlignment: TextInput.AlignVCenter

    // 密码模式
    echoMode: isPassword ? TextInput.Password : TextInput.Normal

    // 输入框背景
    background: Rectangle {
        id: backgroundRect
        color: themeManager ? themeManager.currentTheme.inputBackgroundColor : "#FFFFFF"
        border.color: {
            if (hasError) {
                return themeManager ? themeManager.currentTheme.inputErrorColor : "#FF3B30"
            } else if (customTextField.focus) {
                return themeManager ? themeManager.currentTheme.inputFocusColor : "#007AFF"
            } else {
                return themeManager ? themeManager.currentTheme.inputBorderColor : "#E0E0E0"
            }
        }
        border.width: customTextField.focus || hasError ? 2 : 1
        radius: 12

        // 聚焦动画
        Behavior on border.color {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        Behavior on border.width {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        // 阴影效果
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            color: "transparent"
            border.color: themeManager ? themeManager.currentTheme.shadowColor : "#00000015"
            border.width: customTextField.focus ? 1 : 0
            radius: parent.radius + 2
            z: -1

            Behavior on border.width {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }
        }
    }

    // 左侧图标
    leftPadding: iconSource !== "" ? 48 : 16

    Rectangle {
        id: iconContainer
        visible: iconSource !== ""
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        color: "transparent"

        Image {
            anchors.centerIn: parent
            source: iconSource
            width: 20
            height: 20
            fillMode: Image.PreserveAspectFit
            opacity: customTextField.enabled ? 0.7 : 0.4

            Behavior on opacity {
                OpacityAnimator { duration: 150 }
            }
        }
    }

    // 右侧清除按钮
    rightPadding: showClearButton && text.length > 0 ? 48 : 16

    Rectangle {
        id: clearButton
        visible: showClearButton && customTextField.text.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 12
        color: clearButtonArea.pressed ?
               (themeManager ? themeManager.currentTheme.borderColor : "#E0E0E0") :
               "transparent"

        Text {
            anchors.centerIn: parent
            text: "×"
            font.pixelSize: 18
            font.weight: Font.Bold
            color: themeManager ? themeManager.currentTheme.textSecondaryColor : "#666666"
        }

        MouseArea {
            id: clearButtonArea
            anchors.fill: parent
            onClicked: customTextField.clear()
            cursorShape: Qt.PointingHandCursor
        }

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    // 错误提示
    Text {
        id: errorText
        visible: hasError && errorMessage !== ""
        anchors.top: parent.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.leftMargin: 16
        text: errorMessage
        font.pixelSize: 12
        color: themeManager ? themeManager.currentTheme.errorColor : "#FF3B30"

        // 显示动画
        opacity: visible ? 1.0 : 0.0
        Behavior on opacity {
            OpacityAnimator { duration: 200 }
        }
    }

    // 焦点获取时的动画效果
    onFocusChanged: {
        if (focus) {
            focusAnimation.start()
        }
    }

    SequentialAnimation {
        id: focusAnimation
        NumberAnimation {
            target: backgroundRect
            property: "scale"
            from: 1.0
            to: 1.02
            duration: 100
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: backgroundRect
            property: "scale"
            from: 1.02
            to: 1.0
            duration: 100
            easing.type: Easing.OutCubic
        }
    }

    // 公共方法
    function setError(message) {
        hasError = true
        errorMessage = message
    }

    function clearError() {
        hasError = false
        errorMessage = ""
    }

    function validate() {
        // 子类可以重写此方法进行验证
        return true
    }
}