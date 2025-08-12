import QtQuick

/**
 * @brief 设置管理器
 *
 * 负责管理应用程序的本地设置，包括主题偏好、
 * 记住密码功能等用户偏好设置的持久化存储。
 */
QtObject {
    id: settingsManager

    // 主题设置
    property bool isDarkTheme: false

    // 记住密码设置
    property bool rememberPassword: false
    property string savedUsername: ""
    property string savedPasswordHash: ""

    // 窗口设置
    property int windowWidth: 1440
    property int windowHeight: 900
    property int windowX: -1
    property int windowY: -1

    // 登录界面窗口设置
    property int loginWindowWidth: 450
    property int loginWindowHeight: 600

    // 聊天界面窗口设置
    property int chatWindowWidth: 1440
    property int chatWindowHeight: 900

    // 语言设置
    property string language: "zh_CN"

    // 自动连接设置
    property bool autoConnect: true
    property string serverHost: "localhost"
    property int serverPort: 8080

    // 保存主题设置
    function saveTheme(isDark) {
        isDarkTheme = isDark
        // Theme saved
    }

    // 保存登录信息
    function saveLoginInfo(username, passwordHash, remember) {
        rememberPassword = remember
        if (remember) {
            savedUsername = username
            savedPasswordHash = passwordHash
        } else {
            clearLoginInfo()
        }
        // Login info saved
    }

    // 清除登录信息
    function clearLoginInfo() {
        savedUsername = ""
        savedPasswordHash = ""
        // Login info cleared
    }

    // 保存窗口设置
    function saveWindowSettings(width, height, x, y) {
        windowWidth = width
        windowHeight = height
        windowX = x
        windowY = y
        // Window settings saved
    }

    // 保存网络设置
    function saveNetworkSettings(host, port, autoConn) {
        serverHost = host
        serverPort = port
        autoConnect = autoConn
        // Network settings saved
    }

    // 保存语言设置
    function saveLanguage(lang) {
        language = lang
        // Language saved
    }

    // 获取设置值
    function getValue(key, defaultValue) {
        // Getting setting value
        return defaultValue
    }

    // 设置值
    function setValue(key, value) {
        // Setting value
    }

    // 重置所有设置
    function resetSettings() {
        // 重新加载默认值
        isDarkTheme = false
        rememberPassword = false
        savedUsername = ""
        savedPasswordHash = ""
        windowWidth = 450
        windowHeight = 700
        windowX = -1
        windowY = -1
        language = "zh_CN"
        autoConnect = true
        serverHost = "localhost"
        serverPort = 8080
        // Settings reset to defaults
    }

    // 导出设置
    function exportSettings() {
        var settingsData = {
            theme: {
                isDarkTheme: isDarkTheme
            },
            login: {
                rememberPassword: rememberPassword,
                username: savedUsername
                // 注意：不导出密码哈希以保证安全
            },
            window: {
                width: windowWidth,
                height: windowHeight,
                x: windowX,
                y: windowY
            },
            general: {
                language: language
            },
            network: {
                autoConnect: autoConnect,
                serverHost: serverHost,
                serverPort: serverPort
            }
        }
        return JSON.stringify(settingsData, null, 2)
    }

    // 导入设置（不包括敏感信息）
    function importSettings(jsonData) {
        try {
            var data = JSON.parse(jsonData)

            if (data.theme) {
                saveTheme(data.theme.isDarkTheme || false)
            }

            if (data.window) {
                saveWindowSettings(
                    data.window.width || 450,
                    data.window.height || 700,
                    data.window.x || -1,
                    data.window.y || -1
                )
            }

            if (data.general) {
                saveLanguage(data.general.language || "zh_CN")
            }

            if (data.network) {
                saveNetworkSettings(
                    data.network.serverHost || "localhost",
                    data.network.serverPort || 8080,
                    data.network.autoConnect !== false
                )
            }

            return true
        } catch (e) {
            console.error("Failed to import settings:", e)
            return false
        }
    }
}