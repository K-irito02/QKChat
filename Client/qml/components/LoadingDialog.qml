import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 自定义加载对话框组件
 *
 * 提供现代化的加载指示器设计，支持自定义文本、
 * 主题切换和动画效果。
 */
Dialog {
    id: loadingDialog

    // 自定义属性
    property var themeManager
    property string loadingText: "加载中..."
    property bool showProgress: false
    property real progress: 0.0

    // 对话框属性
    modal: true
    anchors.centerIn: parent
    width: 200
    height: 150
    closePolicy: Popup.NoAutoClose

    // 背景
    background: Rectangle {
        color: themeManager ? themeManager.currentTheme.cardColor : "#FFFFFF"
        radius: 16
        border.color: themeManager ? themeManager.currentTheme.borderColor : "#E0E0E0"
        border.width: 1
        opacity: 0.95

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
        anchors.centerIn: parent

        // 加载指示器
        BusyIndicator {
            id: busyIndicator
            Layout.alignment: Qt.AlignHCenter
            running: loadingDialog.visible
            width: 48
            height: 48

            // 自定义加载指示器样式
            contentItem: Item {
                implicitWidth: 48
                implicitHeight: 48

                Item {
                    id: item
                    x: parent.width / 2 - 24
                    y: parent.height / 2 - 24
                    width: 48
                    height: 48
                    opacity: busyIndicator.running ? 1 : 0

                    Behavior on opacity {
                        OpacityAnimator { duration: 250 }
                    }

                    RotationAnimator {
                        target: item
                        running: busyIndicator.visible && busyIndicator.running
                        from: 0
                        to: 360
                        loops: Animation.Infinite
                        duration: 1250
                    }

                    Repeater {
                        id: repeater
                        model: 6

                        Rectangle {
                            x: item.width / 2 - width / 2
                            y: item.height / 2 - height / 2
                            implicitWidth: 6
                            implicitHeight: 6
                            radius: 3
                            color: themeManager ? themeManager.currentTheme.primaryColor : "#007AFF"
                            transform: [
                                Translate {
                                    y: -Math.min(item.width, item.height) * 0.5 + 6
                                },
                                Rotation {
                                    angle: index / repeater.count * 360
                                    origin.x: 3
                                    origin.y: 3
                                }
                            ]
                            opacity: 1.0 - index / repeater.count
                        }
                    }
                }
            }
        }

        // 进度条（可选）
        ProgressBar {
            visible: showProgress
            Layout.fillWidth: true
            Layout.preferredHeight: 4
            value: progress

            background: Rectangle {
                implicitWidth: 200
                implicitHeight: 4
                color: themeManager ? themeManager.currentTheme.dividerColor : "#F0F0F0"
                radius: 2
            }

            contentItem: Item {
                implicitWidth: 200
                implicitHeight: 4

                Rectangle {
                    width: parent.width * progress
                    height: parent.height
                    radius: 2
                    color: themeManager ? themeManager.currentTheme.primaryColor : "#007AFF"

                    Behavior on width {
                        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                    }
                }
            }
        }

        // 加载文本
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: loadingText
            font.pixelSize: 14
            font.family: "Microsoft YaHei UI"
            color: themeManager ? themeManager.currentTheme.textPrimaryColor : "#000000"
            horizontalAlignment: Text.AlignHCenter
        }
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
    function show(text) {
        if (text !== undefined) {
            loadingText = text
        }
        open()
    }

    function hide() {
        close()
    }

    function setProgress(value) {
        showProgress = true
        progress = Math.max(0, Math.min(1, value))
    }

    function hideProgress() {
        showProgress = false
        progress = 0
    }
}