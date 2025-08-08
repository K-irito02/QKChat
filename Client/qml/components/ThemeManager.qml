import QtQuick
import QtQuick.Controls

/**
 * @brief 主题管理器
 *
 * 负责管理应用程序的主题系统，支持浅色和深色主题切换，
 * 提供完整的颜色方案和主题持久化功能。
 */
QtObject {
    id: themeManager

    // 主题状态
    property bool isDarkTheme: false

    // 主题持久化设置
    property var settings: null

    // 浅色主题颜色方案
    readonly property QtObject lightTheme: QtObject {
        // 背景颜色
        readonly property color backgroundColor: "#FFFFFF"
        readonly property color surfaceColor: "#F8F9FA"
        readonly property color cardColor: "#FFFFFF"
        readonly property color overlayColor: "#00000020"

        // 主要颜色
        readonly property color primaryColor: "#007AFF"
        readonly property color primaryLightColor: "#4DA6FF"
        readonly property color primaryDarkColor: "#0056CC"
        readonly property color secondaryColor: "#5856D6"
        readonly property color accentColor: "#FF3B30"

        // 状态颜色
        readonly property color successColor: "#34C759"
        readonly property color warningColor: "#FF9500"
        readonly property color errorColor: "#FF3B30"
        readonly property color infoColor: "#007AFF"

        // 文本颜色
        readonly property color textPrimaryColor: "#000000"
        readonly property color textSecondaryColor: "#666666"
        readonly property color textTertiaryColor: "#999999"
        readonly property color textDisabledColor: "#CCCCCC"
        readonly property color textOnPrimaryColor: "#FFFFFF"

        // 边框和分割线
        readonly property color borderColor: "#E0E0E0"
        readonly property color dividerColor: "#F0F0F0"
        readonly property color shadowColor: "#00000015"

        // 按钮颜色
        readonly property color buttonPrimaryColor: "#007AFF"
        readonly property color buttonSecondaryColor: "#F2F2F7"
        readonly property color buttonTextColor: "#FFFFFF"
        readonly property color buttonSecondaryTextColor: "#007AFF"
        readonly property color buttonDisabledColor: "#CCCCCC"
        readonly property color buttonHoverColor: "#0056CC"

        // 输入框颜色
        readonly property color inputBackgroundColor: "#FFFFFF"
        readonly property color inputBorderColor: "#E0E0E0"
        readonly property color inputFocusColor: "#007AFF"
        readonly property color inputErrorColor: "#FF3B30"
        readonly property color inputPlaceholderColor: "#999999"

        // 特殊效果颜色
        readonly property color rippleColor: "#007AFF20"
        readonly property color selectionColor: "#007AFF30"
    }

    // 深色主题颜色方案
    readonly property QtObject darkTheme: QtObject {
        // 背景颜色
        readonly property color backgroundColor: "#1C1C1E"
        readonly property color surfaceColor: "#2C2C2E"
        readonly property color cardColor: "#2C2C2E"
        readonly property color overlayColor: "#00000060"

        // 主要颜色
        readonly property color primaryColor: "#0A84FF"
        readonly property color primaryLightColor: "#4DA6FF"
        readonly property color primaryDarkColor: "#0056CC"
        readonly property color secondaryColor: "#5E5CE6"
        readonly property color accentColor: "#FF453A"

        // 状态颜色
        readonly property color successColor: "#30D158"
        readonly property color warningColor: "#FF9F0A"
        readonly property color errorColor: "#FF453A"
        readonly property color infoColor: "#0A84FF"

        // 文本颜色
        readonly property color textPrimaryColor: "#FFFFFF"
        readonly property color textSecondaryColor: "#8E8E93"
        readonly property color textTertiaryColor: "#636366"
        readonly property color textDisabledColor: "#48484A"
        readonly property color textOnPrimaryColor: "#FFFFFF"

        // 边框和分割线
        readonly property color borderColor: "#38383A"
        readonly property color dividerColor: "#48484A"
        readonly property color shadowColor: "#00000040"

        // 按钮颜色
        readonly property color buttonPrimaryColor: "#0A84FF"
        readonly property color buttonSecondaryColor: "#48484A"
        readonly property color buttonTextColor: "#FFFFFF"
        readonly property color buttonSecondaryTextColor: "#0A84FF"
        readonly property color buttonDisabledColor: "#48484A"
        readonly property color buttonHoverColor: "#4DA6FF"

        // 输入框颜色
        readonly property color inputBackgroundColor: "#1C1C1E"
        readonly property color inputBorderColor: "#38383A"
        readonly property color inputFocusColor: "#0A84FF"
        readonly property color inputErrorColor: "#FF453A"
        readonly property color inputPlaceholderColor: "#636366"

        // 特殊效果颜色
        readonly property color rippleColor: "#0A84FF20"
        readonly property color selectionColor: "#0A84FF30"
    }

    // 当前主题
    readonly property QtObject currentTheme: isDarkTheme ? darkTheme : lightTheme

    // 主题切换函数
    function toggleTheme() {
        isDarkTheme = !isDarkTheme
        saveThemePreference()
    }

    // 设置主题
    function setTheme(dark) {
        if (isDarkTheme !== dark) {
            isDarkTheme = dark
            saveThemePreference()
        }
    }

    // 获取主题名称
    function getThemeName() {
        return isDarkTheme ? "深色" : "浅色"
    }

    // 获取相反主题名称
    function getOppositeThemeName() {
        return isDarkTheme ? "浅色" : "深色"
    }

    // 保存主题偏好设置
    function saveThemePreference() {
        if (settings) {
            settings.setValue("theme/isDarkTheme", isDarkTheme)
        }
    }

    // 加载主题偏好设置
    function loadThemePreference() {
        if (settings) {
            isDarkTheme = settings.value("theme/isDarkTheme", false)
        }
    }

    // 初始化主题管理器
    function initialize(settingsObject) {
        settings = settingsObject
        loadThemePreference()
    }

    // 获取颜色的透明度变体
    function withOpacity(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    // 获取颜色的亮度调整变体
    function lighten(color, factor) {
        return Qt.lighter(color, 1 + factor)
    }

    function darken(color, factor) {
        return Qt.darker(color, 1 + factor)
    }
}