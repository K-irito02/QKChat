#include "DatabaseManager.h"
#include "utils/Logger.h"
#include <QSqlDriver>
#include <QSqlIndex>
#include <QSqlField>
#include <QSqlResult>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QTimer>
#include <QThread>


// 静态成员初始化
DatabaseManager* DatabaseManager::m_instance = nullptr;
QMutex DatabaseManager::s_mutex;

// 数据库表名常量
const QString DatabaseManager::TABLE_USERS = "users";
const QString DatabaseManager::TABLE_CHAT_MESSAGES = "chat_messages";
const QString DatabaseManager::TABLE_FRIENDSHIPS = "friendships";
const QString DatabaseManager::TABLE_SETTINGS = "settings";

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_isConnected(false)
{
    // 设置数据库路径
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    m_databasePath = appDataPath + "/qkchat.db";
    
    // 初始化数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(m_databasePath);
}

DatabaseManager::~DatabaseManager()
{
    close();
}

DatabaseManager* DatabaseManager::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (!m_instance) {
        QMutexLocker locker(&s_mutex);
        if (!m_instance) {
            m_instance = new DatabaseManager();
        }
    }
    return m_instance;
}

bool DatabaseManager::initialize()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    // 检查是否在主线程中调用
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        LOG_WARNING("DatabaseManager::initialize() called from main thread, this may block UI");
    }
    
    if (m_database.open()) {
        m_isConnected = true;
        m_initialized = true;
        
    
        
        // 异步创建表，避免阻塞主线程
        QTimer::singleShot(100, this, [this]() {
            if (!createTables()) {
                LOG_ERROR("Failed to create database tables");
            } else {
            
            }
        });
        
        return true;
    } else {
        LOG_ERROR(QString("Failed to open database: %1").arg(m_database.lastError().text()));
        return false;
    }
}

void DatabaseManager::close()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    m_initialized = false;
}

bool DatabaseManager::createTables()
{
    // 用户表
    QString createUsersTable = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username VARCHAR(50) UNIQUE NOT NULL,"
        "email VARCHAR(100) UNIQUE NOT NULL,"
        "password_hash VARCHAR(255) NOT NULL,"
        "salt VARCHAR(100) NOT NULL,"
        "avatar_url VARCHAR(255),"
        "status VARCHAR(20) DEFAULT 'offline',"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last_login DATETIME,"
        "remember_password BOOLEAN DEFAULT 0,"
        "theme VARCHAR(20) DEFAULT 'light'"
        ")"
    ).arg(TABLE_USERS);
    
    // 聊天记录表
    QString createMessagesTable = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender_id INTEGER NOT NULL,"
        "receiver_id INTEGER NOT NULL,"
        "message_type VARCHAR(20) DEFAULT 'text',"
        "content TEXT NOT NULL,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "is_read BOOLEAN DEFAULT 0,"
        "FOREIGN KEY (sender_id) REFERENCES %2(id),"
        "FOREIGN KEY (receiver_id) REFERENCES %2(id)"
        ")"
    ).arg(TABLE_CHAT_MESSAGES, TABLE_USERS);
    
    // 好友关系表
    QString createFriendshipsTable = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL,"
        "friend_id INTEGER NOT NULL,"
        "status VARCHAR(20) DEFAULT 'pending',"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (user_id) REFERENCES %2(id),"
        "FOREIGN KEY (friend_id) REFERENCES %2(id),"
        "UNIQUE(user_id, friend_id)"
        ")"
    ).arg(TABLE_FRIENDSHIPS, TABLE_USERS);
    
    // 系统设置表
    QString createSettingsTable = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "key VARCHAR(50) UNIQUE NOT NULL,"
        "value TEXT,"
        "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")"
    ).arg(TABLE_SETTINGS);
    
    // 执行创建表语句
    QSqlQuery query(m_database);
    
    if (!query.exec(createUsersTable)) {
        logError("Create Users Table", query.lastError().text());
        return false;
    }
    
    if (!query.exec(createMessagesTable)) {
        logError("Create Messages Table", query.lastError().text());
        return false;
    }
    
    if (!query.exec(createFriendshipsTable)) {
        logError("Create Friendships Table", query.lastError().text());
        return false;
    }
    
    if (!query.exec(createSettingsTable)) {
        logError("Create Settings Table", query.lastError().text());
        return false;
    }
    
    // 插入默认设置
    QVariantMap defaultSettings;
    defaultSettings["theme"] = "light";
    defaultSettings["remember_password"] = "false";
    defaultSettings["auto_login"] = "false";
    defaultSettings["notification_sound"] = "true";
    defaultSettings["message_preview"] = "true";
    
    for (auto it = defaultSettings.begin(); it != defaultSettings.end(); ++it) {
        saveSetting(it.key(), it.value().toString());
    }
    
    return true;
}

qint64 DatabaseManager::createUser(const QString &username, const QString &email, 
                                   const QString &passwordHash, const QString &salt)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return -1;
    
    QString sql = QString(
        "INSERT INTO %1 (username, email, password_hash, salt) "
        "VALUES (:username, :email, :password_hash, :salt)"
    ).arg(TABLE_USERS);
    
    QVariantMap params;
    params[":username"] = username;
    params[":email"] = email;
    params[":password_hash"] = passwordHash;
    params[":salt"] = salt;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Create User", query.lastError().text());
        return -1;
    }
    
    return query.lastInsertId().toLongLong();
}

QJsonObject DatabaseManager::authenticateUser(const QString &username, const QString &passwordHash)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return QJsonObject();
    
    QString sql = QString(
        "SELECT id, username, email, password_hash, salt, avatar_url, status, theme "
        "FROM %1 WHERE username = :username OR email = :email"
    ).arg(TABLE_USERS);
    
    QVariantMap params;
    params[":username"] = username;
    params[":email"] = username;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Authenticate User", query.lastError().text());
        return QJsonObject();
    }
    
    if (query.next()) {
        QSqlRecord record = query.record();
        
        // 验证密码
        QString storedHash = record.value("password_hash").toString();
        if (storedHash == passwordHash) {
            QJsonObject userInfo;
            userInfo["id"] = record.value("id").toLongLong();
            userInfo["username"] = record.value("username").toString();
            userInfo["email"] = record.value("email").toString();
            userInfo["avatar_url"] = record.value("avatar_url").toString();
            userInfo["status"] = record.value("status").toString();
            userInfo["theme"] = record.value("theme").toString();
            
            // 更新最后登录时间
            updateUserStatus(userInfo["id"].toVariant().toLongLong(), "online");
            
            return userInfo;
        }
    }
    
    return QJsonObject();
}

bool DatabaseManager::updateUser(qint64 userId, const QJsonObject &userData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QStringList updateFields;
    QVariantMap params;
    
    if (userData.contains("username")) {
        updateFields << "username = :username";
        params[":username"] = userData["username"].toString();
    }
    
    if (userData.contains("email")) {
        updateFields << "email = :email";
        params[":email"] = userData["email"].toString();
    }
    
    if (userData.contains("avatar_url")) {
        updateFields << "avatar_url = :avatar_url";
        params[":avatar_url"] = userData["avatar_url"].toString();
    }
    
    if (userData.contains("theme")) {
        updateFields << "theme = :theme";
        params[":theme"] = userData["theme"].toString();
    }
    
    if (updateFields.isEmpty()) return false;
    
    updateFields << "updated_at = CURRENT_TIMESTAMP";
    params[":user_id"] = userId;
    
    QString sql = QString(
        "UPDATE %1 SET %2 WHERE id = :user_id"
    ).arg(TABLE_USERS, updateFields.join(", "));
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Update User", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

QJsonObject DatabaseManager::getUserInfo(qint64 userId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return QJsonObject();
    
    QString sql = QString(
        "SELECT id, username, email, avatar_url, status, theme, created_at, last_login "
        "FROM %1 WHERE id = :user_id"
    ).arg(TABLE_USERS);
    
    QVariantMap params;
    params[":user_id"] = userId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Get User Info", query.lastError().text());
        return QJsonObject();
    }
    
    if (query.next()) {
        QSqlRecord record = query.record();
        QJsonObject userInfo;
        userInfo["id"] = record.value("id").toLongLong();
        userInfo["username"] = record.value("username").toString();
        userInfo["email"] = record.value("email").toString();
        userInfo["avatar_url"] = record.value("avatar_url").toString();
        userInfo["status"] = record.value("status").toString();
        userInfo["theme"] = record.value("theme").toString();
        userInfo["created_at"] = record.value("created_at").toString();
        userInfo["last_login"] = record.value("last_login").toString();
        
        return userInfo;
    }
    
    return QJsonObject();
}

bool DatabaseManager::updateUserStatus(qint64 userId, const QString &status)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "UPDATE %1 SET status = :status, last_login = CURRENT_TIMESTAMP "
        "WHERE id = :user_id"
    ).arg(TABLE_USERS);
    
    QVariantMap params;
    params[":status"] = status;
    params[":user_id"] = userId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Update User Status", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

qint64 DatabaseManager::saveMessage(const QJsonObject &messageData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return -1;
    
    QString sql = QString(
        "INSERT INTO %1 (sender_id, receiver_id, message_type, content) "
        "VALUES (:sender_id, :receiver_id, :message_type, :content)"
    ).arg(TABLE_CHAT_MESSAGES);
    
    QVariantMap params;
    params[":sender_id"] = messageData["sender_id"].toVariant().toLongLong();
    params[":receiver_id"] = messageData["receiver_id"].toVariant().toLongLong();
    params[":message_type"] = messageData["message_type"].toString();
    params[":content"] = messageData["content"].toString();
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Save Message", query.lastError().text());
        return -1;
    }
    
    return query.lastInsertId().toLongLong();
}

QJsonArray DatabaseManager::getChatHistory(qint64 userId, qint64 friendId, int limit, int offset)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return QJsonArray();
    
    QString sql = QString(
        "SELECT id, sender_id, receiver_id, message_type, content, timestamp, is_read "
        "FROM %1 WHERE "
        "(sender_id = :user_id AND receiver_id = :friend_id) OR "
        "(sender_id = :friend_id AND receiver_id = :user_id) "
        "ORDER BY timestamp DESC LIMIT :limit OFFSET :offset"
    ).arg(TABLE_CHAT_MESSAGES);
    
    QVariantMap params;
    params[":user_id"] = userId;
    params[":friend_id"] = friendId;
    params[":limit"] = limit;
    params[":offset"] = offset;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Get Chat History", query.lastError().text());
        return QJsonArray();
    }
    
    QJsonArray messages;
    while (query.next()) {
        QSqlRecord record = query.record();
        QJsonObject message;
        message["id"] = record.value("id").toLongLong();
        message["sender_id"] = record.value("sender_id").toLongLong();
        message["receiver_id"] = record.value("receiver_id").toLongLong();
        message["message_type"] = record.value("message_type").toString();
        message["content"] = record.value("content").toString();
        message["timestamp"] = record.value("timestamp").toString();
        message["is_read"] = record.value("is_read").toBool();
        
        messages.append(message);
    }
    
    return messages;
}

bool DatabaseManager::markMessageAsRead(qint64 messageId, qint64 userId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "UPDATE %1 SET is_read = 1 "
        "WHERE id = :message_id AND receiver_id = :user_id"
    ).arg(TABLE_CHAT_MESSAGES);
    
    QVariantMap params;
    params[":message_id"] = messageId;
    params[":user_id"] = userId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Mark Message As Read", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteMessage(qint64 messageId, qint64 userId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "DELETE FROM %1 WHERE id = :message_id AND sender_id = :user_id"
    ).arg(TABLE_CHAT_MESSAGES);
    
    QVariantMap params;
    params[":message_id"] = messageId;
    params[":user_id"] = userId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Delete Message", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

bool DatabaseManager::addFriendship(qint64 userId, qint64 friendId, const QString &status)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "INSERT OR REPLACE INTO %1 (user_id, friend_id, status) "
        "VALUES (:user_id, :friend_id, :status)"
    ).arg(TABLE_FRIENDSHIPS);
    
    QVariantMap params;
    params[":user_id"] = userId;
    params[":friend_id"] = friendId;
    params[":status"] = status;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Add Friendship", query.lastError().text());
        return false;
    }
    
    return true;
}

bool DatabaseManager::updateFriendshipStatus(qint64 userId, qint64 friendId, const QString &status)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "UPDATE %1 SET status = :status WHERE user_id = :user_id AND friend_id = :friend_id"
    ).arg(TABLE_FRIENDSHIPS);
    
    QVariantMap params;
    params[":status"] = status;
    params[":user_id"] = userId;
    params[":friend_id"] = friendId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Update Friendship Status", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

QJsonArray DatabaseManager::getFriendsList(qint64 userId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return QJsonArray();
    
    QString sql = QString(
        "SELECT f.friend_id, u.username, u.email, u.avatar_url, u.status, f.status as friendship_status "
        "FROM %1 f "
        "JOIN %2 u ON f.friend_id = u.id "
        "WHERE f.user_id = :user_id AND f.status = 'accepted'"
    ).arg(TABLE_FRIENDSHIPS, TABLE_USERS);
    
    QVariantMap params;
    params[":user_id"] = userId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Get Friends List", query.lastError().text());
        return QJsonArray();
    }
    
    QJsonArray friends;
    while (query.next()) {
        QSqlRecord record = query.record();
        QJsonObject friendInfo;
        friendInfo["id"] = record.value("friend_id").toLongLong();
        friendInfo["username"] = record.value("username").toString();
        friendInfo["email"] = record.value("email").toString();
        friendInfo["avatar_url"] = record.value("avatar_url").toString();
        friendInfo["status"] = record.value("status").toString();
        friendInfo["friendship_status"] = record.value("friendship_status").toString();

        friends.append(friendInfo);
    }
    
    return friends;
}

bool DatabaseManager::removeFriendship(qint64 userId, qint64 friendId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "DELETE FROM %1 WHERE user_id = :user_id AND friend_id = :friend_id"
    ).arg(TABLE_FRIENDSHIPS);
    
    QVariantMap params;
    params[":user_id"] = userId;
    params[":friend_id"] = friendId;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Remove Friendship", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

bool DatabaseManager::saveSetting(const QString &key, const QString &value)
{
    // 检查当前线程是否已经持有互斥锁（避免死锁）
    bool alreadyLocked = false;
    if (m_mutex.tryLock()) {
        m_mutex.unlock();
    } else {
        alreadyLocked = true;
    }
    
    QMutexLocker locker(alreadyLocked ? nullptr : &m_mutex);
    
    if (!checkConnection()) {
        return false;
    }
    
    QString sql = QString(
        "INSERT OR REPLACE INTO %1 (key, value, updated_at) VALUES (:key, :value, CURRENT_TIMESTAMP)"
    ).arg(TABLE_SETTINGS);
    
    QVariantMap params;
    params[":key"] = key;
    params[":value"] = value;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Save Setting", query.lastError().text());
        return false;
    }
    
    return true;
}

QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return defaultValue;
    
    QString sql = QString(
        "SELECT value FROM %1 WHERE key = :key"
    ).arg(TABLE_SETTINGS);
    
    QVariantMap params;
    params[":key"] = key;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Get Setting", query.lastError().text());
        return defaultValue;
    }
    
    if (query.next()) {
        return query.value(0).toString();
    }
    
    return defaultValue;
}

bool DatabaseManager::removeSetting(const QString &key)
{
    QMutexLocker locker(&m_mutex);
    
    if (!checkConnection()) return false;
    
    QString sql = QString(
        "DELETE FROM %1 WHERE key = :key"
    ).arg(TABLE_SETTINGS);
    
    QVariantMap params;
    params[":key"] = key;
    
    QSqlQuery query = executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        logError("Remove Setting", query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

QString DatabaseManager::hashPassword(const QString &password, const QString &salt)
{
    QByteArray data = (password + salt).toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

QString DatabaseManager::generateSalt()
{
    const QString chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString salt;
    salt.reserve(32);
    
    for (int i = 0; i < 32; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        salt.append(chars.at(index));
    }
    
    return salt;
}

bool DatabaseManager::isValidEmail(const QString &email)
{
    QRegularExpression emailRegex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return emailRegex.match(email).hasMatch();
}

QSqlQuery DatabaseManager::executeQuery(const QString &sql, const QVariantMap &params)
{
    QSqlQuery query(m_database);
    query.prepare(sql);
    
    for (auto it = params.begin(); it != params.end(); ++it) {
        query.bindValue(it.key(), it.value());
    }
    
    query.exec();
    return query;
}

bool DatabaseManager::checkConnection()
{
    if (!m_initialized || !m_database.isOpen()) {
        return false;
    }
    return true;
}

void DatabaseManager::logError(const QString &operation, const QString &error)
{
    // 数据库错误日志已禁用，避免调试信息干扰
}
