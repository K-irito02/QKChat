import QtQuick 2.15
import QtQuick.Controls 2.15

/**
 * @brief 消息动画组件
 * 提供新消息的淡入动画和状态变化动画效果
 */
Item {
    id: root
    
    // 公共属性
    property bool isNewMessage: false
    property bool isStatusChanged: false
    property real animationDuration: 300
    
    // 动画状态
    property real opacity: 1.0
    property real scale: 1.0
    
    // 新消息淡入动画
    SequentialAnimation {
        id: newMessageAnimation
        running: isNewMessage
        
        // 初始状态
        ScriptAction {
            script: {
                root.opacity = 0.0
                root.scale = 0.8
            }
        }
        
        // 淡入动画
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                to: 1.0
                duration: animationDuration
                easing.type: Easing.OutCubic
            }
            
            NumberAnimation {
                target: root
                property: "scale"
                to: 1.0
                duration: animationDuration
                easing.type: Easing.OutBack
            }
        }
        
        // 动画完成
        ScriptAction {
            script: {
                isNewMessage = false
            }
        }
    }
    
    // 状态变化动画
    SequentialAnimation {
        id: statusChangeAnimation
        running: isStatusChanged
        
        // 轻微缩放效果
        NumberAnimation {
            target: root
            property: "scale"
            to: 1.1
            duration: 150
            easing.type: Easing.OutQuad
        }
        
        NumberAnimation {
            target: root
            property: "scale"
            to: 1.0
            duration: 150
            easing.type: Easing.InQuad
        }
        
        // 动画完成
        ScriptAction {
            script: {
                isStatusChanged = false
            }
        }
    }
    
    // 触发新消息动画
    function triggerNewMessageAnimation() {
        isNewMessage = true
    }
    
    // 触发状态变化动画
    function triggerStatusChangeAnimation() {
        isStatusChanged = true
    }
}
