import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

/**
 * @brief 加载对话框组件
 * 
 * 显示加载状态的模态对话框，包含加载动画和状态文本。
 * 用于在执行网络请求或其他耗时操作时提供用户反馈。
 */
Dialog {
    id: loadingDialog
    
    // 公共属性
    property string loadingText: "加载中..."
    property bool showCancelButton: false
    
    // 对话框属性
    modal: true
    closePolicy: showCancelButton ? Popup.CloseOnEscape : Popup.NoAutoClose
    anchors.centerIn: parent
    
    // 背景
    background: Rectangle {
        color: themeManager.currentTheme.surfaceColor
        radius: 12
        border.color: themeManager.currentTheme.borderColor
        border.width: 1
        
        // 阴影效果
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 4
            radius: 12
            samples: 25
            color: themeManager.currentTheme.shadowColor
        }
    }
    
    // 内容
    contentItem: ColumnLayout {
        spacing: 20
        
        // 加载动画
        BusyIndicator {
            id: busyIndicator
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            running: loadingDialog.visible
            
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
                        OpacityAnimator {
                            duration: 250
                        }
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
                            implicitWidth: 8
                            implicitHeight: 8
                            radius: 4
                            color: themeManager.currentTheme.primaryColor
                            transform: [
                                Translate {
                                    y: -Math.min(item.width, item.height) * 0.5 + 8
                                },
                                Rotation {
                                    angle: index / repeater.count * 360
                                    origin.x: 4
                                    origin.y: 4
                                }
                            ]
                            opacity: 1.0 - index / repeater.count
                        }
                    }
                }
            }
        }
        
        // 加载文本
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            text: loadingText
            color: themeManager.currentTheme.textPrimaryColor
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        
        // 取消按钮（可选）
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 100
            Layout.preferredHeight: 36
            visible: showCancelButton
            text: "取消"
            
            background: Rectangle {
                color: parent.pressed ? themeManager.currentTheme.borderColor : "transparent"
                border.color: themeManager.currentTheme.borderColor
                border.width: 1
                radius: 6
            }
            
            contentItem: Text {
                text: parent.text
                color: themeManager.currentTheme.textSecondaryColor
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            onClicked: {
                loadingDialog.reject()
            }
        }
    }
    
    // 公共方法
    function show(text, showCancel) {
        if (text !== undefined) {
            loadingText = text
        }
        if (showCancel !== undefined) {
            showCancelButton = showCancel
        }
        open()
    }
    
    function hide() {
        close()
    }
    
    // 信号
    signal cancelled()
    
    // 处理取消
    onRejected: {
        cancelled()
    }
}
