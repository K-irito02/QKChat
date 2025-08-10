#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMutex>
#include <QVariant>

/**
 * @brief 配置管理器类
 * 
 * 负责管理服务器的所有配置信息，支持JSON和INI格式的配置文件。
 * 提供配置文件热重载、环境变量覆盖、配置验证等功能。
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 配置文件格式枚举
     */
    enum ConfigFormat {
        JSON,
        INI
    };
    Q_ENUM(ConfigFormat)

    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();
    
    /**
     * @brief 获取单例实例
     * @return 配置管理器实例
     */
    static ConfigManager* instance();
    
    /**
     * @brief 加载配置文件
     * @param filePath 配置文件路径
     * @param format 配置文件格式
     * @return 加载是否成功
     */
    bool loadConfig(const QString &filePath, ConfigFormat format = JSON);
    
    /**
     * @brief 保存配置到文件
     * @param filePath 配置文件路径
     * @return 保存是否成功
     */
    bool saveConfig(const QString &filePath = "");
    
    /**
     * @brief 重新加载配置文件
     * @return 重载是否成功
     */
    bool reloadConfig();
    
    /**
     * @brief 启用配置文件热重载
     * @param enabled 是否启用
     */
    void setHotReloadEnabled(bool enabled);
    
    /**
     * @brief 获取配置值
     * @param key 配置键（支持点分隔的嵌套键）
     * @param defaultValue 默认值
     * @return 配置值
     */
    QVariant getValue(const QString &key, const QVariant &defaultValue = QVariant()) const;
    
    /**
     * @brief 设置配置值
     * @param key 配置键
     * @param value 配置值
     */
    void setValue(const QString &key, const QVariant &value);
    
    /**
     * @brief 检查配置键是否存在
     * @param key 配置键
     * @return 是否存在
     */
    bool contains(const QString &key) const;
    
    /**
     * @brief 获取所有配置
     * @return 配置JSON对象
     */
    QJsonObject getAllConfig() const;
    
    /**
     * @brief 获取配置节
     * @param section 节名称
     * @return 配置节JSON对象
     */
    QJsonObject getSection(const QString &section) const;
    
    /**
     * @brief 验证配置
     * @return 验证结果和错误信息
     */
    QPair<bool, QString> validateConfig() const;
    
    /**
     * @brief 应用环境变量覆盖
     */
    void applyEnvironmentOverrides();
    
    /**
     * @brief 获取数据库配置
     * @return 数据库配置对象
     */
    QJsonObject getDatabaseConfig() const;
    
    /**
     * @brief 获取Redis配置
     * @return Redis配置对象
     */
    QJsonObject getRedisConfig() const;
    
    /**
     * @brief 获取SMTP配置
     * @return SMTP配置对象
     */
    QJsonObject getSmtpConfig() const;
    
    /**
     * @brief 获取服务器配置
     * @return 服务器配置对象
     */
    QJsonObject getServerConfig() const;
    
    /**
     * @brief 获取日志配置
     * @return 日志配置对象
     */
    QJsonObject getLogConfig() const;
    
    /**
     * @brief 获取安全配置
     * @return 安全配置对象
     */
    QJsonObject getSecurityConfig() const;

signals:
    /**
     * @brief 配置已重载信号
     */
    void configReloaded();
    
    /**
     * @brief 配置值改变信号
     * @param key 配置键
     * @param newValue 新值
     * @param oldValue 旧值
     */
    void configChanged(const QString &key, const QVariant &newValue, const QVariant &oldValue);
    
    /**
     * @brief 配置错误信号
     * @param error 错误信息
     */
    void configError(const QString &error);

private slots:
    void onConfigFileChanged(const QString &path);
    void onReloadTimer();

private:
    /**
     * @brief 加载JSON配置文件
     * @param filePath 文件路径
     * @return 加载是否成功
     */
    bool loadJsonConfig(const QString &filePath);
    
    /**
     * @brief 加载INI配置文件
     * @param filePath 文件路径
     * @return 加载是否成功
     */
    bool loadIniConfig(const QString &filePath);
    
    /**
     * @brief 保存JSON配置文件
     * @param filePath 文件路径
     * @return 保存是否成功
     */
    bool saveJsonConfig(const QString &filePath);
    
    /**
     * @brief 保存INI配置文件
     * @param filePath 文件路径
     * @return 保存是否成功
     */
    bool saveIniConfig(const QString &filePath);
    
    /**
     * @brief 设置默认配置
     */
    void setDefaultConfig();
    
    /**
     * @brief 解析嵌套键
     * @param key 嵌套键（如"database.host"）
     * @param obj 输出对象引用
     * @param finalKey 输出最终键名
     * @return 解析是否成功
     */
    bool parseNestedKey(const QString &key, QJsonObject &obj, QString &finalKey) const;
    
    /**
     * @brief 从环境变量获取值
     * @param envKey 环境变量键
     * @param configKey 配置键
     * @return 环境变量值
     */
    QVariant getEnvironmentValue(const QString &envKey, const QString &configKey) const;

    /**
     * @brief 无锁版本的环境变量覆盖
     * @param config 要修改的配置对象
     */
    void applyEnvironmentOverridesUnlocked(QJsonObject &config);

    /**
     * @brief 无锁版本的配置验证
     * @param config 要验证的配置对象
     * @return 验证结果
     */
    QPair<bool, QString> validateConfigUnlocked(const QJsonObject &config) const;

    /**
     * @brief 无锁版本的setValue
     * @param config 要修改的配置对象
     * @param key 配置键
     * @param value 配置值
     */
    void setValueUnlocked(QJsonObject &config, const QString &key, const QVariant &value);

    /**
     * @brief 无锁版本的getValue
     * @param config 配置对象
     * @param key 配置键
     * @param defaultValue 默认值
     * @return 配置值
     */
    QVariant getValueUnlocked(const QJsonObject &config, const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * @brief 无锁版本的contains
     * @param config 配置对象
     * @param key 配置键
     * @return 是否包含该键
     */
    bool containsUnlocked(const QJsonObject &config, const QString &key) const;

    /**
     * @brief 无锁版本的parseNestedKey
     * @param config 配置对象
     * @param key 配置键
     * @param obj 输出的JSON对象
     * @param finalKey 输出的最终键
     * @return 解析是否成功
     */
    bool parseNestedKeyUnlocked(const QJsonObject &config, const QString &key, QJsonObject &obj, QString &finalKey) const;

    /**
     * @brief 更新嵌套对象
     * @param config 要修改的配置对象
     * @param key 配置键
     * @param value 配置值
     */
    void updateNestedObject(QJsonObject &config, const QString &key, const QJsonValue &value);

    /**
     * @brief 重建嵌套路径
     * @param config 要修改的配置对象
     * @param key 配置键
     * @param value 配置值
     */
    void rebuildNestedPath(QJsonObject &config, const QString &key, const QJsonValue &value);

private:
    static ConfigManager* s_instance;
    static QMutex s_instanceMutex;  // 保护单例创建的互斥锁

    QJsonObject _config;
    QString _configFilePath;
    ConfigFormat _configFormat;

    QFileSystemWatcher* _fileWatcher;
    QTimer* _reloadTimer;
    bool _hotReloadEnabled;

    mutable QMutex _configMutex;
};

#endif // CONFIGMANAGER_H
