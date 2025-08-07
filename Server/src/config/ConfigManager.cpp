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
    if (!s_instance) {
        s_instance = new ConfigManager();
    }
    return s_instance;
}

bool ConfigManager::loadConfig(const QString &filePath, ConfigFormat format)
{
    QMutexLocker locker(&_configMutex);
    
    _configFilePath = filePath;
    _configFormat = format;
    
    bool success = false;
    
    if (format == JSON) {
        success = loadJsonConfig(filePath);
    } else {
        success = loadIniConfig(filePath);
    }
    
    if (success) {
        // 应用环境变量覆盖
        applyEnvironmentOverrides();
        
        // 验证配置
        auto validation = validateConfig();
        if (!validation.first) {
            LOG_WARNING(QString("Configuration validation failed: %1").arg(validation.second));
        }
        
        // 启用文件监视
        if (_hotReloadEnabled) {
            _fileWatcher->removePaths(_fileWatcher->files());
            _fileWatcher->addPath(filePath);
        }
        
        LOG_INFO(QString("Configuration loaded from: %1").arg(filePath));
        emit configReloaded();
    } else {
        LOG_ERROR(QString("Failed to load configuration from: %1").arg(filePath));
    }
    
    return success;
}

bool ConfigManager::saveConfig(const QString &filePath)
{
    QMutexLocker locker(&_configMutex);
    
    QString targetPath = filePath.isEmpty() ? _configFilePath : filePath;
    
    bool success = false;
    
    if (_configFormat == JSON) {
        success = saveJsonConfig(targetPath);
    } else {
        success = saveIniConfig(targetPath);
    }
    
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
    QMutexLocker locker(&_configMutex);
    
    _hotReloadEnabled = enabled;
    
    if (enabled && !_configFilePath.isEmpty()) {
        _fileWatcher->addPath(_configFilePath);
    } else {
        _fileWatcher->removePaths(_fileWatcher->files());
    }
    
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
    QMutexLocker locker(&_configMutex);
    
    QVariant oldValue = getValue(key);
    
    QJsonObject obj;
    QString finalKey;
    
    if (!parseNestedKey(key, obj, finalKey)) {
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
    
    obj[finalKey] = jsonValue;
    
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
    // 验证必需的配置项
    QStringList requiredKeys = {
        "server.port",
        "database.host",
        "database.name"
    };
    
    for (const QString &key : requiredKeys) {
        if (!contains(key)) {
            return qMakePair(false, QString("Missing required configuration: %1").arg(key));
        }
    }
    
    // 验证端口范围
    int serverPort = getValue("server.port", 0).toInt();
    if (serverPort <= 0 || serverPort > 65535) {
        return qMakePair(false, "Invalid server port");
    }
    
    int dbPort = getValue("database.port", 0).toInt();
    if (dbPort <= 0 || dbPort > 65535) {
        return qMakePair(false, "Invalid database port");
    }
    
    return qMakePair(true, "");
}

void ConfigManager::applyEnvironmentOverrides()
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
            setValue(it.value(), envValue);
            LOG_DEBUG(QString("Applied environment override: %1 = %2").arg(it.value()).arg(envValue.toString()));
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
    LOG_INFO("Configuration file changed, reloading...");

    if (reloadConfig()) {
        LOG_INFO("Configuration reloaded successfully");
    } else {
        LOG_ERROR("Failed to reload configuration");
        emit configError("Failed to reload configuration file");
    }
}

bool ConfigManager::loadJsonConfig(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open config file: %1").arg(filePath));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("JSON parse error in config file: %1").arg(parseError.errorString()));
        return false;
    }

    if (!doc.isObject()) {
        LOG_ERROR("Config file must contain a JSON object");
        return false;
    }

    _config = doc.object();
    return true;
}

bool ConfigManager::loadIniConfig(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);

    if (settings.status() != QSettings::NoError) {
        LOG_ERROR(QString("Cannot load INI config file: %1").arg(filePath));
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
    QStringList parts = key.split('.');
    if (parts.isEmpty()) {
        return false;
    }

    obj = _config;

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
