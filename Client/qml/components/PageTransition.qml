import QtQuick

/**
 * @brief 页面转换动画组件
 *
 * 提供多种页面切换动画效果，包括滑动、淡入淡出、
 * 缩放等现代化的转换动画。
 */
QtObject {
    id: pageTransition

    // 动画类型枚举
    enum TransitionType {
        SlideLeft,
        SlideRight,
        SlideUp,
        SlideDown,
        Fade,
        Scale,
        Flip,
        Push,
        Pop
    }

    // 创建滑动转换动画
    function createSlideTransition(type, duration) {
        duration = duration || 300

        var component = Qt.createComponent("qrc:/qt/qml/Client/components/SlideTransition.qml")
        if (component.status === Component.Ready) {
            return component.createObject(null, {
                "transitionType": type,
                "duration": duration
            })
        }

        // 回退到内联动画
        return createInlineSlideTransition(type, duration)
    }

    // 创建内联滑动动画
    function createInlineSlideTransition(type, duration) {
        var pushTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                PropertyAnimation {
                    property: "x"
                    duration: ${duration}
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    duration: ${duration}
                    easing.type: Easing.OutCubic
                }
            }
        `, pageTransition)

        var popTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                PropertyAnimation {
                    property: "x"
                    duration: ${duration}
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    duration: ${duration}
                    easing.type: Easing.OutCubic
                }
            }
        `, pageTransition)

        return {
            pushTransition: pushTransition,
            popTransition: popTransition
        }
    }

    // 创建淡入淡出动画
    function createFadeTransition(duration) {
        duration = duration || 250

        var pushTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: ${duration}
                    easing.type: Easing.OutCubic
                }
            }
        `, pageTransition)

        var popTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                PropertyAnimation {
                    property: "opacity"
                    from: 1.0
                    to: 0.0
                    duration: ${duration}
                    easing.type: Easing.OutCubic
                }
            }
        `, pageTransition)

        return {
            pushTransition: pushTransition,
            popTransition: popTransition
        }
    }

    // 创建缩放动画
    function createScaleTransition(duration) {
        duration = duration || 300

        var pushTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                ParallelAnimation {
                    PropertyAnimation {
                        property: "scale"
                        from: 0.8
                        to: 1.0
                        duration: ${duration}
                        easing.type: Easing.OutBack
                    }
                    PropertyAnimation {
                        property: "opacity"
                        from: 0.0
                        to: 1.0
                        duration: ${duration}
                        easing.type: Easing.OutCubic
                    }
                }
            }
        `, pageTransition)

        var popTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                ParallelAnimation {
                    PropertyAnimation {
                        property: "scale"
                        from: 1.0
                        to: 0.9
                        duration: ${duration}
                        easing.type: Easing.OutCubic
                    }
                    PropertyAnimation {
                        property: "opacity"
                        from: 1.0
                        to: 0.0
                        duration: ${duration}
                        easing.type: Easing.OutCubic
                    }
                }
            }
        `, pageTransition)

        return {
            pushTransition: pushTransition,
            popTransition: popTransition
        }
    }

    // 创建翻转动画
    function createFlipTransition(duration) {
        duration = duration || 400

        var pushTransition = Qt.createQmlObject(`
            import QtQuick
            Transition {
                SequentialAnimation {
                    PropertyAnimation {
                        property: "rotation"
                        from: 0
                        to: 90
                        duration: ${duration / 2}
                        easing.type: Easing.OutCubic
                    }
                    PropertyAction {
                        property: "visible"
                        value: true
                    }
                    PropertyAnimation {
                        property: "rotation"
                        from: -90
                        to: 0
                        duration: ${duration / 2}
                        easing.type: Easing.OutCubic
                    }
                }
            }
        `, pageTransition)

        return {
            pushTransition: pushTransition,
            popTransition: pushTransition
        }
    }
}