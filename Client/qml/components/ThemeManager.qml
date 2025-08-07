import QtQuick

/**
 * @brief 主题管理器组件
 * 
 * 提供全局主题管理功能，包括浅色和深色主题的切换。
 * 管理应用程序的颜色方案和主题状态。
 */
QtObject {
    id: themeManager
    
    // 主题状态
    property bool isDarkTheme: false
    
    // 浅色主题颜色
    readonly property QtObject lightTheme: QtObject {
        readonly property color backgroundColor: "#FFFFFF"
        readonly property color surfaceColor: "#F8F9FA"
        readonly property color primaryColor: "#007AFF"
        readonly property color secondaryColor: "#5856D6"
        readonly property color accentColor: "#FF3B30"
        readonly property color successColor: "#34C759"
        readonly property color warningColor: "#FF9500"
        readonly property color errorColor: "#FF3B30"
        
        readonly property color textPrimaryColor: "#000000"
        readonly property color textSecondaryColor: "#666666"
        readonly property color textTertiaryColor: "#999999"
        readonly property color textDisabledColor: "#CCCCCC"
        
        readonly property color borderColor: "#E0E0E0"
        readonly property color dividerColor: "#F0F0F0"
        readonly property color shadowColor: "#00000020"
        
        readonly property color buttonBackgroundColor: "#007AFF"
        readonly property color buttonTextColor: "#FFFFFF"
        readonly property color buttonDisabledColor: "#CCCCCC"
        
        readonly property color inputBackgroundColor: "#FFFFFF"
        readonly property color inputBorderColor: "#E0E0E0"
        readonly property color inputFocusColor: "#007AFF"
    }
    
    // 深色主题颜色
    readonly property QtObject darkTheme: QtObject {
        readonly property color backgroundColor: "#1C1C1E"
        readonly property color surfaceColor: "#2C2C2E"
        readonly property color primaryColor: "#0A84FF"
        readonly property color secondaryColor: "#5E5CE6"
        readonly property color accentColor: "#FF453A"
        readonly property color successColor: "#30D158"
        readonly property color warningColor: "#FF9F0A"
        readonly property color errorColor: "#FF453A"
        
        readonly property color textPrimaryColor: "#FFFFFF"
        readonly property color textSecondaryColor: "#8E8E93"
        readonly property color textTertiaryColor: "#636366"
        readonly property color textDisabledColor: "#48484A"
        
        readonly property color borderColor: "#38383A"
        readonly property color dividerColor: "#48484A"
        readonly property color shadowColor: "#00000040"
        
        readonly property color buttonBackgroundColor: "#0A84FF"
        readonly property color buttonTextColor: "#FFFFFF"
        readonly property color buttonDisabledColor: "#48484A"
        
        readonly property color inputBackgroundColor: "#1C1C1E"
        readonly property color inputBorderColor: "#38383A"
        readonly property color inputFocusColor: "#0A84FF"
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
    
    // 保存主题偏好
    function saveThemePreference() {
        if (typeof sessionManager !== 'undefined' && sessionManager.currentUser) {
            // 如果用户已登录，更新用户主题设置
            sessionManager.currentUser.theme = isDarkTheme ? "dark" : "light"
        }
        
        // 保存到本地设置
        Qt.application.organization = "QKChat"
        Qt.application.name = "Client"
        var settings = Qt.createQmlObject('import Qt.labs.settings 1.0; Settings {}', themeManager)
        settings.theme = isDarkTheme ? "dark" : "light"
    }
    
    // 加载主题偏好
    function loadThemePreference() {
        try {
            Qt.application.organization = "QKChat"
            Qt.application.name = "Client"
            var settings = Qt.createQmlObject('import Qt.labs.settings 1.0; Settings {}', themeManager)
            var savedTheme = settings.theme || "light"
            isDarkTheme = (savedTheme === "dark")
        } catch (e) {
            console.warn("Failed to load theme preference:", e)
            isDarkTheme = false
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
    
    // 组件完成时加载主题偏好
    Component.onCompleted: {
        loadThemePreference()
    }
}
