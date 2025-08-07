import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * @brief 消息对话框组件
 * 
 * 显示各种类型的消息对话框，包括信息、警告、错误和确认对话框。
 * 提供统一的用户交互界面。
 */
Dialog {
    id: messageDialog
    
    // 消息类型枚举
    enum MessageType {
        Info,
        Warning,
        Error,
        Success,
        Question
    }
    
    // 公共属性
    property int messageType: MessageDialog.MessageType.Info
    property string messageTitle: ""
    property string messageText: ""
    property bool showCancelButton: false
    property string confirmButtonText: "确定"
    property string cancelButtonText: "取消"
    
    // 对话框属性
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: parent
    width: Math.min(400, parent.width - 40)
    
    // 标题
    title: messageTitle
    
    // 背景
    background: Rectangle {
        color: themeManager.currentTheme.surfaceColor
        radius: 12
        border.color: themeManager.currentTheme.borderColor
        border.width: 1
    }
    
    // 标题栏
    header: Rectangle {
        height: messageTitle ? 50 : 0
        color: "transparent"
        visible: messageTitle
        
        Text {
            anchors.centerIn: parent
            text: messageTitle
            color: themeManager.currentTheme.textPrimaryColor
            font.pixelSize: 18
            font.weight: Font.Medium
        }
        
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: themeManager.currentTheme.dividerColor
        }
    }
    
    // 内容
    contentItem: ColumnLayout {
        spacing: 20
        
        // 图标和消息
        RowLayout {
            Layout.fillWidth: true
            spacing: 15
            
            // 消息图标
            Text {
                Layout.alignment: Qt.AlignTop
                text: getMessageIcon()
                color: getMessageColor()
                font.pixelSize: 24
                font.family: "Segoe UI Symbol"
            }
            
            // 消息文本
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                text: messageText
                color: themeManager.currentTheme.textPrimaryColor
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                lineHeight: 1.4
            }
        }
    }
    
    // 底部按钮
    footer: Rectangle {
        height: 60
        color: "transparent"
        
        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: themeManager.currentTheme.dividerColor
        }
        
        RowLayout {
            anchors.centerIn: parent
            spacing: 10
            
            // 取消按钮
            Button {
                visible: showCancelButton
                Layout.preferredWidth: 80
                Layout.preferredHeight: 36
                text: cancelButtonText
                
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
                    messageDialog.reject()
                }
            }
            
            // 确定按钮
            Button {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 36
                text: confirmButtonText
                
                background: Rectangle {
                    color: parent.pressed ? 
                           Qt.darker(getMessageColor(), 1.1) : 
                           getMessageColor()
                    radius: 6
                }
                
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    messageDialog.accept()
                }
            }
        }
    }
    
    // 获取消息图标
    function getMessageIcon() {
        switch (messageType) {
            case MessageDialog.MessageType.Info:
                return "ℹ"
            case MessageDialog.MessageType.Warning:
                return "⚠"
            case MessageDialog.MessageType.Error:
                return "✖"
            case MessageDialog.MessageType.Success:
                return "✓"
            case MessageDialog.MessageType.Question:
                return "?"
            default:
                return "ℹ"
        }
    }
    
    // 获取消息颜色
    function getMessageColor() {
        switch (messageType) {
            case MessageDialog.MessageType.Info:
                return themeManager.currentTheme.primaryColor
            case MessageDialog.MessageType.Warning:
                return themeManager.currentTheme.warningColor
            case MessageDialog.MessageType.Error:
                return themeManager.currentTheme.errorColor
            case MessageDialog.MessageType.Success:
                return themeManager.currentTheme.successColor
            case MessageDialog.MessageType.Question:
                return themeManager.currentTheme.primaryColor
            default:
                return themeManager.currentTheme.primaryColor
        }
    }
    
    // 公共方法
    function showInfo(title, text) {
        messageType = MessageDialog.MessageType.Info
        messageTitle = title
        messageText = text
        showCancelButton = false
        open()
    }
    
    function showWarning(title, text) {
        messageType = MessageDialog.MessageType.Warning
        messageTitle = title
        messageText = text
        showCancelButton = false
        open()
    }
    
    function showError(title, text) {
        messageType = MessageDialog.MessageType.Error
        messageTitle = title
        messageText = text
        showCancelButton = false
        open()
    }
    
    function showSuccess(title, text) {
        messageType = MessageDialog.MessageType.Success
        messageTitle = title
        messageText = text
        showCancelButton = false
        open()
    }
    
    function showQuestion(title, text, confirmText, cancelText) {
        messageType = MessageDialog.MessageType.Question
        messageTitle = title
        messageText = text
        showCancelButton = true
        if (confirmText) confirmButtonText = confirmText
        if (cancelText) cancelButtonText = cancelText
        open()
    }
    
    // 信号
    signal confirmed()
    signal cancelled()
    
    // 处理结果
    onAccepted: {
        confirmed()
    }
    
    onRejected: {
        cancelled()
    }
}
