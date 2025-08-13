#include "ConfigManager.h"
#include "../utils/Logger.h"
#include <QFile>
#include <QDir>
#include <QJsonParseError>
#include <QJsonArray>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QStandardPaths>

// 静态成员初始化
ConfigManager* ConfigManager::s_instance = nullptr;
QMutex ConfigManager::s_instanceMutex;

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , _configFormat(JSON)
    , _hotReloadEnabled(true)
{
    // 创建文件监视器
    _fileWatcher = new QFileSystemWatcher(this);
    connect(_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &ConfigManager::onConfigFileChanged);
    
    // 创建重载定时器（防抖）
    _reloadTimer = new QTimer(this);
    _reloadTimer->setSingleShot(true);
    _reloadTimer->setInterval(1000); // 1秒延迟
    connect(_reloadTimer, &QTimer::timeout, this, &ConfigManager::onReloadTimer);
    
    // 设置默认配置
    setDefaultConfig();
}

ConfigManager::~ConfigManager()
{
}

ConfigManager* ConfigManager::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new ConfigManager();
        }
    }
    return s_instance;
}

bool ConfigManager::loadConfig(const QString &filePath, ConfigFormat format)
{
    // 准备日志信息和文件监视信息，避免在锁内调用危险操作
    QString logMessage;
    QString validationError;
    bool success = false;
    bool needUpdateFileWatcher = false;
    QString watchPath;
    QJsonObject tempConfig;

    // 第一阶段：在锁内加载配置文件
    {
        QMutexLocker locker(&_configMutex);

        _configFilePath = filePath;
        _configFormat = format;

        if (format == JSON) {
            success = loadJsonConfig(filePath);
        } else {
            success = loadIniConfig(filePath);
        }

        if (success) {
            // 保存配置的副本，用于锁外处理
            tempConfig = _config;

            // 准备文件监视信息，但不在锁内操作QFileSystemWatcher
            if (_hotReloadEnabled) {
                needUpdateFileWatcher = true;
                watchPath = filePath;
            }

            logMessage = QString("Configuration loaded from: %1").arg(filePath);
        } else {
            logMessage = QString("Failed to load configuration from: %1").arg(filePath);
        }
    } // 锁在这里释放

    // 第二阶段：在锁外应用环境变量覆盖和验证
    if (success) {
        // 应用环境变量覆盖（在锁外）
        applyEnvironmentOverridesUnlocked(tempConfig);

        // 验证配置（在锁外）
        auto validation = validateConfigUnlocked(tempConfig);
        if (!validation.first) {
            validationError = validation.second;
        }

        // 第三阶段：将处理后的配置写回
        {
            QMutexLocker locker(&_configMutex);
            _config = tempConfig;
        }
    }

    // 在锁外操作QFileSystemWatcher，避免信号死锁
    if (needUpdateFileWatcher) {
        _fileWatcher->removePaths(_fileWatcher->files());
        _fileWatcher->addPath(watchPath);
    }

    // 在锁外记录日志和发射信号
    if (success) {
        // 配置加载成功
        if (!validationError.isEmpty()) {
            LOG_WARNING(QString("Configuration validation failed: %1").arg(validationError));
        }
        // 使用QTimer延迟发射信号，确保配置完全稳定
        QTimer::singleShot(0, this, &ConfigManager::configReloaded);
    } else {
        LOG_ERROR(logMessage);
    }

    return success;
}

bool ConfigManager::saveConfig(const QString &filePath)
{
    QString targetPath;
    bool success = false;

    {
        QMutexLocker locker(&_configMutex);

        targetPath = filePath.isEmpty() ? _configFilePath : filePath;

        if (_configFormat == JSON) {
            success = saveJsonConfig(targetPath);
        } else {
            success = saveIniConfig(targetPath);
        }
    } // 锁在这里释放

    // 在锁外记录日志
    if (success) {
        LOG_INFO(QString("Configuration saved to: %1").arg(targetPath));
    } else {
        LOG_ERROR(QString("Failed to save configuration to: %1").arg(targetPath));
    }

    return success;
}

bool ConfigManager::reloadConfig()
{
    if (_configFilePath.isEmpty()) {
        return false;
    }
    
    return loadConfig(_configFilePath, _configFormat);
}

void ConfigManager::setHotReloadEnabled(bool enabled)
{
    bool needUpdateWatcher = false;
    QString configPath;

    {
        QMutexLocker locker(&_configMutex);
        _hotReloadEnabled = enabled;
        needUpdateWatcher = true;
        configPath = _configFilePath;
    } // 锁在这里释放

    // 在锁外操作QFileSystemWatcher
    if (needUpdateWatcher) {
        if (enabled && !configPath.isEmpty()) {
            _fileWatcher->addPath(configPath);
        } else {
            _fileWatcher->removePaths(_fileWatcher->files());
        }
    }

    // 在锁外记录日志
    LOG_INFO(QString("Configuration hot reload %1").arg(enabled ? "enabled" : "disabled"));
}

QVariant ConfigManager::getValue(const QString &key, const QVariant &defaultValue) const
{
    QMutexLocker locker(&_configMutex);
    
    QJsonObject obj;
    QString finalKey;
    
    if (!parseNestedKey(key, obj, finalKey)) {
        return defaultValue;
    }
    
    if (!obj.contains(finalKey)) {
        return defaultValue;
    }
    
    QJsonValue value = obj[finalKey];
    
    // 转换JSON值到QVariant
    if (value.isBool()) {
        return value.toBool();
    } else if (value.isDouble()) {
        return value.toDouble();
    } else if (value.isString()) {
        return value.toString();
    } else if (value.isArray()) {
        QVariantList list;
        for (const QJsonValue &item : value.toArray()) {
            if (item.isString()) {
                list.append(item.toString());
            } else if (item.isDouble()) {
                list.append(item.toDouble());
            } else if (item.isBool()) {
                list.append(item.toBool());
            }
        }
        return list;
    } else if (value.isObject()) {
        return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    }
    
    return defaultValue;
}

void ConfigManager::setValue(const QString &key, const QVariant &value)
{
    QVariant oldValue;

    {
        QMutexLocker locker(&_configMutex);

        // 获取旧值，使用无锁版本避免递归锁定
        oldValue = getValueUnlocked(_config, key);

        // 设置新值，使用无锁版本避免递归锁定
        setValueUnlocked(_config, key, value);
    } // 锁在这里释放

    // 在锁外发射信号
    emit configChanged(key, value, oldValue);
}

bool ConfigManager::contains(const QString &key) const
{
    QMutexLocker locker(&_configMutex);
    
    QJsonObject obj;
    QString finalKey;
    
    if (!parseNestedKey(key, obj, finalKey)) {
        return false;
    }
    
    return obj.contains(finalKey);
}

QJsonObject ConfigManager::getAllConfig() const
{
    QMutexLocker locker(&_configMutex);
    return _config;
}

QJsonObject ConfigManager::getSection(const QString &section) const
{
    QMutexLocker locker(&_configMutex);
    
    if (_config.contains(section) && _config[section].isObject()) {
        return _config[section].toObject();
    }
    
    return QJsonObject();
}

QPair<bool, QString> ConfigManager::validateConfig() const
{
    QMutexLocker locker(&_configMutex);
    return validateConfigUnlocked(_config);
}

QPair<bool, QString> ConfigManager::validateConfigUnlocked(const QJsonObject &config) const
{
    // 验证必需的配置项
    QStringList requiredKeys = {
        "server.port",
        "database.host",
        "database.name"
    };

    for (const QString &key : requiredKeys) {
        if (!containsUnlocked(config, key)) {
            return qMakePair(false, QString("Missing required configuration: %1").arg(key));
        }
    }

    // 验证端口范围
    int serverPort = getValueUnlocked(config, "server.port", 0).toInt();
    if (serverPort <= 0 || serverPort > 65535) {
        return qMakePair(false, "Invalid server port");
    }

    int dbPort = getValueUnlocked(config, "database.port", 0).toInt();
    if (dbPort <= 0 || dbPort > 65535) {
        return qMakePair(false, "Invalid database port");
    }

    return qMakePair(true, "");
}

void ConfigManager::applyEnvironmentOverrides()
{
    QMutexLocker locker(&_configMutex);
    applyEnvironmentOverridesUnlocked(_config);
}

void ConfigManager::applyEnvironmentOverridesUnlocked(QJsonObject &config)
{
    // 定义环境变量映射
    QMap<QString, QString> envMappings = {
        {"QKCHAT_DB_HOST", "database.host"},
        {"QKCHAT_DB_PORT", "database.port"},
        {"QKCHAT_DB_NAME", "database.name"},
        {"QKCHAT_DB_USER", "database.username"},
        {"QKCHAT_DB_PASS", "database.password"},
        {"QKCHAT_REDIS_HOST", "redis.host"},
        {"QKCHAT_REDIS_PORT", "redis.port"},
        {"QKCHAT_REDIS_PASS", "redis.password"},
        {"QKCHAT_SERVER_PORT", "server.port"},
        {"QKCHAT_LOG_LEVEL", "logging.level"},
        {"QKCHAT_SMTP_HOST", "smtp.host"},
        {"QKCHAT_SMTP_PORT", "smtp.port"},
        {"QKCHAT_SMTP_USER", "smtp.username"},
        {"QKCHAT_SMTP_PASS", "smtp.password"}
    };

    for (auto it = envMappings.begin(); it != envMappings.end(); ++it) {
        QVariant envValue = getEnvironmentValue(it.key(), it.value());
        if (envValue.isValid()) {
            setValueUnlocked(config, it.value(), envValue);
        }
    }
}

QJsonObject ConfigManager::getDatabaseConfig() const
{
    return getSection("database");
}

QJsonObject ConfigManager::getRedisConfig() const
{
    return getSection("redis");
}

QJsonObject ConfigManager::getSmtpConfig() const
{
    return getSection("smtp");
}

QJsonObject ConfigManager::getServerConfig() const
{
    return getSection("server");
}

QJsonObject ConfigManager::getLogConfig() const
{
    return getSection("logging");
}

QJsonObject ConfigManager::getSecurityConfig() const
{
    return getSection("security");
}

void ConfigManager::onConfigFileChanged(const QString &path)
{
    Q_UNUSED(path)

    if (_hotReloadEnabled && !_reloadTimer->isActive()) {
        _reloadTimer->start();
    }
}

void ConfigManager::onReloadTimer()
{
    // 使用QTimer延迟记录日志，避免在信号处理中直接调用LOG宏
    QTimer::singleShot(0, [this]() {
        // 配置文件已更改，正在重新加载

        if (reloadConfig()) {
        
        } else {
            LOG_ERROR("Failed to reload configuration");
            emit configError("Failed to reload configuration file");
        }
    });
}

bool ConfigManager::loadJsonConfig(const QString &filePath)
{
    // 注意：此方法在持有锁的情况下调用，不能直接使用LOG_*宏
    // 错误信息将由调用者处理
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    if (!doc.isObject()) {
        return false;
    }

    _config = doc.object();
    return true;
}

bool ConfigManager::loadIniConfig(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);

    if (settings.status() != QSettings::NoError) {
        // 注意：此方法在持有锁的情况下调用，错误信息将由调用者处理
        return false;
    }

    _config = QJsonObject();

    // 转换INI设置到JSON对象
    QStringList groups = settings.childGroups();
    for (const QString &group : groups) {
        settings.beginGroup(group);

        QJsonObject groupObj;
        QStringList keys = settings.childKeys();
        for (const QString &key : keys) {
            QVariant value = settings.value(key);

            if (value.typeId() == QMetaType::Bool) {
                groupObj[key] = value.toBool();
            } else if (value.typeId() == QMetaType::Int) {
                groupObj[key] = value.toInt();
            } else if (value.typeId() == QMetaType::Double) {
                groupObj[key] = value.toDouble();
            } else {
                groupObj[key] = value.toString();
            }
        }

        _config[group] = groupObj;
        settings.endGroup();
    }

    return true;
}

bool ConfigManager::saveJsonConfig(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(_config);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

bool ConfigManager::saveIniConfig(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);
    settings.clear();

    for (auto it = _config.begin(); it != _config.end(); ++it) {
        QString group = it.key();
        QJsonValue value = it.value();

        if (value.isObject()) {
            settings.beginGroup(group);
            QJsonObject obj = value.toObject();

            for (auto objIt = obj.begin(); objIt != obj.end(); ++objIt) {
                QString key = objIt.key();
                QJsonValue val = objIt.value();

                if (val.isBool()) {
                    settings.setValue(key, val.toBool());
                } else if (val.isDouble()) {
                    settings.setValue(key, val.toDouble());
                } else {
                    settings.setValue(key, val.toString());
                }
            }

            settings.endGroup();
        }
    }

    settings.sync();
    return settings.status() == QSettings::NoError;
}

void ConfigManager::setDefaultConfig()
{
    _config = QJsonObject();

    // 服务器配置
    QJsonObject serverConfig;
    serverConfig["port"] = 8080;
    serverConfig["max_clients"] = 1000;
    serverConfig["heartbeat_interval"] = 30000;
    serverConfig["use_tls"] = true;
    _config["server"] = serverConfig;

    // 数据库配置
    QJsonObject dbConfig;
    dbConfig["host"] = "localhost";
    dbConfig["port"] = 3306;
    dbConfig["name"] = "qkchat";
    dbConfig["username"] = "root";
    dbConfig["password"] = "3143285505";
    dbConfig["charset"] = "utf8mb4";
    dbConfig["pool_size"] = 10;
    _config["database"] = dbConfig;

    // Redis配置
    QJsonObject redisConfig;
    redisConfig["host"] = "localhost";
    redisConfig["port"] = 6379;
    redisConfig["password"] = "";
    redisConfig["database"] = 0;
    _config["redis"] = redisConfig;

    // SMTP配置
    QJsonObject smtpConfig;
    smtpConfig["host"] = "smtp.qq.com";
    smtpConfig["port"] = 587;
    smtpConfig["username"] = "saokiritoasuna00@qq.com";
    smtpConfig["password"] = "ssvbzaqvotjcchjh";
    smtpConfig["use_tls"] = true;
    _config["smtp"] = smtpConfig;

    // 日志配置
    QJsonObject logConfig;
    logConfig["level"] = "INFO";
    logConfig["console_output"] = true;
    logConfig["json_format"] = false;
    logConfig["max_file_size"] = 104857600; // 100MB
    logConfig["retention_days"] = 30;
    logConfig["directory"] = ""; // 将在运行时设置
    _config["logging"] = logConfig;

    // 安全配置
    QJsonObject securityConfig;
    securityConfig["rate_limit_enabled"] = true;
    securityConfig["max_requests_per_minute"] = 60;
    securityConfig["session_timeout"] = 86400; // 24小时
    securityConfig["password_min_length"] = 6;
    _config["security"] = securityConfig;
}

bool ConfigManager::parseNestedKey(const QString &key, QJsonObject &obj, QString &finalKey) const
{
    return parseNestedKeyUnlocked(_config, key, obj, finalKey);
}

bool ConfigManager::parseNestedKeyUnlocked(const QJsonObject &config, const QString &key, QJsonObject &obj, QString &finalKey) const
{
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return false;
    }

    obj = config;

    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString &part = parts[i];

        if (!obj.contains(part)) {
            return false;
        }

        if (!obj[part].isObject()) {
            return false;
        }

        obj = obj[part].toObject();
    }

    finalKey = parts.last();
    return true;
}

void ConfigManager::setValueUnlocked(QJsonObject &config, const QString &key, const QVariant &value)
{
    QJsonObject obj;
    QString finalKey;

    if (!parseNestedKeyUnlocked(config, key, obj, finalKey)) {
        return;
    }

    // 转换QVariant到JSON值
    QJsonValue jsonValue;

    switch (value.typeId()) {
        case QMetaType::Bool:
            jsonValue = value.toBool();
            break;
        case QMetaType::Int:
        case QMetaType::LongLong:
            jsonValue = value.toLongLong();
            break;
        case QMetaType::Double:
            jsonValue = value.toDouble();
            break;
        case QMetaType::QString:
            jsonValue = value.toString();
            break;
        case QMetaType::QStringList: {
            QJsonArray array;
            for (const QString &str : value.toStringList()) {
                array.append(str);
            }
            jsonValue = array;
            break;
        }
        default:
            jsonValue = value.toString();
            break;
    }

    // 需要更新嵌套对象
    updateNestedObject(config, key, jsonValue);
}

QVariant ConfigManager::getValueUnlocked(const QJsonObject &config, const QString &key, const QVariant &defaultValue) const
{
    QJsonObject obj;
    QString finalKey;

    if (!parseNestedKeyUnlocked(config, key, obj, finalKey)) {
        return defaultValue;
    }

    if (!obj.contains(finalKey)) {
        return defaultValue;
    }

    QJsonValue value = obj[finalKey];

    // 转换JSON值到QVariant
    if (value.isBool()) {
        return value.toBool();
    } else if (value.isDouble()) {
        return value.toDouble();
    } else if (value.isString()) {
        return value.toString();
    } else if (value.isArray()) {
        QStringList list;
        for (const QJsonValue &val : value.toArray()) {
            list.append(val.toString());
        }
        return list;
    }

    return defaultValue;
}

bool ConfigManager::containsUnlocked(const QJsonObject &config, const QString &key) const
{
    QJsonObject obj;
    QString finalKey;

    if (!parseNestedKeyUnlocked(config, key, obj, finalKey)) {
        return false;
    }

    return obj.contains(finalKey);
}

void ConfigManager::updateNestedObject(QJsonObject &config, const QString &key, const QJsonValue &value)
{
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return;
    }

    QJsonObject *currentObj = &config;

    // 遍历到倒数第二层
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString &part = parts[i];

        if (!currentObj->contains(part)) {
            (*currentObj)[part] = QJsonObject();
        }

        if (!(*currentObj)[part].isObject()) {
            (*currentObj)[part] = QJsonObject();
        }

        QJsonObject nestedObj = (*currentObj)[part].toObject();
        (*currentObj)[part] = nestedObj;
        currentObj = &nestedObj;
    }

    // 设置最终值
    (*currentObj)[parts.last()] = value;

    // 需要重新构建整个路径，因为QJsonObject是值类型
    rebuildNestedPath(config, key, value);
}

void ConfigManager::rebuildNestedPath(QJsonObject &config, const QString &key, const QJsonValue &value)
{
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return;
    }

    if (parts.size() == 1) {
        config[parts[0]] = value;
        return;
    }

    // 递归重建路径
    QString firstPart = parts[0];
    QString remainingPath = parts.mid(1).join('.');

    QJsonObject nestedObj;
    if (config.contains(firstPart) && config[firstPart].isObject()) {
        nestedObj = config[firstPart].toObject();
    }

    rebuildNestedPath(nestedObj, remainingPath, value);
    config[firstPart] = nestedObj;
}

QVariant ConfigManager::getEnvironmentValue(const QString &envKey, const QString &configKey) const
{
    QByteArray envValue = qgetenv(envKey.toUtf8());
    if (envValue.isEmpty()) {
        return QVariant();
    }

    QString strValue = QString::fromUtf8(envValue);

    // 尝试转换为适当的类型
    if (configKey.contains("port") || configKey.contains("size") || configKey.contains("timeout")) {
        bool ok;
        int intValue = strValue.toInt(&ok);
        if (ok) {
            return intValue;
        }
    }

    if (strValue.toLower() == "true" || strValue.toLower() == "false") {
        return strValue.toLower() == "true";
    }

    return strValue;
}
