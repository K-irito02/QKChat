#include "RecentContactsManager.h"
#include "../utils/Logger.h"
#include "../auth/NetworkClient.h"
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMutexLocker>
#include <QDateTime>
#include <QSet>

// 静态成员初始化
RecentContactsManager* RecentContactsManager::s_instance = nullptr;
QMutex RecentContactsManager::s_instanceMutex;

RecentContactsManager::RecentContactsManager(QObject *parent)
    : QObject(parent)
    , _isLoading(false)
{
    // 初始化自动保存定时器
    _autoSaveTimer = new QTimer(this);
    _autoSaveTimer->setInterval(30000); // 30秒自动保存一次
    _autoSaveTimer->setSingleShot(false);
    connect(_autoSaveTimer, &QTimer::timeout, this, &RecentContactsManager::onAutoSaveTimer);
    _autoSaveTimer->start();
    
    // 加载本地数据
    loadFromLocal();
    

}

RecentContactsManager* RecentContactsManager::instance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new RecentContactsManager();
    }
    return s_instance;
}

void RecentContactsManager::addRecentContact(const QVariantMap& contact)
{
    QMutexLocker locker(&_mutex);
    
    // 支持多种用户ID字段名
    qint64 userId = contact.value("user_id").toLongLong();
    if (userId <= 0) {
        userId = contact.value("id").toLongLong();
    }
    if (userId <= 0) {
        userId = contact.value("friend_id").toLongLong();
    }
    
    if (userId <= 0) {
        LOG_WARNING("Invalid user ID for recent contact");
        LOG_WARNING(QString("Contact data: %1").arg(QJsonDocument(QJsonObject::fromVariantMap(contact)).toJson().constData()));
        return;
    }
    
    // 检查是否已存在
    int existingIndex = findContactIndex(userId);
    if (existingIndex >= 0) {
        // 移动到顶部
        moveToTop(existingIndex);
    
        return;
    }
    
    // 创建联系人数据
    QVariantMap contactData = createContactData(contact);
    
    // 添加到列表顶部
    _recentContacts.prepend(contactData);
    
    // 限制最大数量（保留最近50个联系人）
    while (_recentContacts.size() > 50) {
        _recentContacts.removeLast();
    }
    

    
    emit recentContactsChanged();
    emit contactAdded(contactData);
    
    // 立即保存到本地文件
    QTimer::singleShot(0, this, [this]() {
        saveToLocal();
    });
}

void RecentContactsManager::removeRecentContact(qint64 userId)
{
    QMutexLocker locker(&_mutex);
    
    int index = findContactIndex(userId);
    if (index >= 0) {
        _recentContacts.removeAt(index);
        
    
        
        emit recentContactsChanged();
        emit contactRemoved(userId);
        
        // 立即保存到本地文件
        QTimer::singleShot(0, this, [this]() {
            saveToLocal();
        });
    }
}

void RecentContactsManager::updateLastMessage(qint64 userId, const QString& message, const QString& time)
{
    QMutexLocker locker(&_mutex);
    
    int index = findContactIndex(userId);
    if (index >= 0) {
        QVariantMap contact = _recentContacts[index].toMap();
        contact["last_message"] = message;
        contact["last_message_time"] = time;
        contact["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        _recentContacts[index] = contact;
        
        // 移动到顶部
        moveToTop(index);
        
    
        
        emit recentContactsChanged();
        emit contactUpdated(userId);
        
        // 立即保存到本地文件
        QTimer::singleShot(0, this, [this]() {
            saveToLocal();
        });
    }
}

void RecentContactsManager::updateUnreadCount(qint64 userId, int count)
{
    QMutexLocker locker(&_mutex);
    
    int index = findContactIndex(userId);
    if (index >= 0) {
        QVariantMap contact = _recentContacts[index].toMap();
        contact["unread_count"] = count;
        contact["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        _recentContacts[index] = contact;
        
    
        
        emit recentContactsChanged();
        emit contactUpdated(userId);
        
        // 立即保存到本地文件
        QTimer::singleShot(0, this, [this]() {
            saveToLocal();
        });
    }
}

void RecentContactsManager::clearRecentContacts()
{
    QMutexLocker locker(&_mutex);
    
    _recentContacts.clear();
    

    
    emit recentContactsChanged();
    
    // 立即保存到本地文件
    QTimer::singleShot(0, this, [this]() {
        saveToLocal();
    });
}

void RecentContactsManager::refreshRecentContacts()
{
    // 重新加载本地数据
    loadFromLocal();
}

void RecentContactsManager::filterByFriendList(const QVariantList& friendList)
{
    QMutexLocker locker(&_mutex);
    
    // 提取好友ID列表
    QSet<qint64> validFriendIds;
    for (const auto& friendVar : friendList) {
        QVariantMap friendData = friendVar.toMap();
        qint64 friendId = friendData.value("user_id").toLongLong();
        if (friendId <= 0) {
            friendId = friendData.value("id").toLongLong();
        }
        if (friendId <= 0) {
            friendId = friendData.value("friend_id").toLongLong();
        }
        if (friendId > 0) {
            validFriendIds.insert(friendId);
        }
    }
    

    
    // 过滤最近联系人，只保留有效的好友
    bool hasChanges = false;
    for (int i = _recentContacts.size() - 1; i >= 0; --i) {
        QVariantMap contact = _recentContacts[i].toMap();
        qint64 contactId = contact.value("user_id").toLongLong();
        
        if (!validFriendIds.contains(contactId)) {

            _recentContacts.removeAt(i);
            hasChanges = true;
        }
    }
    
    if (hasChanges) {
    
        emit recentContactsChanged();
        
        // 延迟保存，避免在信号处理中调用
        QTimer::singleShot(0, this, [this]() {
            saveToLocal();
        });
    }
}

void RecentContactsManager::clearInvalidContacts()
{
    QMutexLocker locker(&_mutex);
    

    _recentContacts.clear();
    emit recentContactsChanged();
    
    // 直接保存，避免死锁
    try {
        QString filePath = getRecentContactsFilePath();
        
        QJsonArray jsonArray; // 空数组
        QJsonDocument doc(jsonArray);
        
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        
        } else {
            LOG_ERROR("Failed to save cleared recent contacts to local file");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception while saving cleared recent contacts: %1").arg(e.what()));
    }
}

void RecentContactsManager::onAutoSaveTimer()
{
    saveToLocal();
}

void RecentContactsManager::setIsLoading(bool loading)
{
    if (_isLoading != loading) {
        _isLoading = loading;
        emit isLoadingChanged();
    }
}

int RecentContactsManager::findContactIndex(qint64 userId)
{
    for (int i = 0; i < _recentContacts.size(); ++i) {
        QVariantMap contact = _recentContacts[i].toMap();
        qint64 contactUserId = contact.value("user_id").toLongLong();
        if (contactUserId == userId) {
            return i;
        }
    }
    return -1;
}

void RecentContactsManager::moveToTop(int index)
{
    if (index > 0 && index < _recentContacts.size()) {
        QVariant contact = _recentContacts.takeAt(index);
        _recentContacts.prepend(contact);
    }
}

void RecentContactsManager::saveToLocal()
{
    // 创建数据副本以避免死锁
    QVariantList contactsCopy;
    {
        QMutexLocker locker(&_mutex);
        contactsCopy = _recentContacts;
    }
    
    try {
        QString filePath = getRecentContactsFilePath();
        
        QJsonArray jsonArray;
        for (const auto& contactVar : contactsCopy) {
            QVariantMap contact = contactVar.toMap();
            QJsonObject jsonObj = QJsonObject::fromVariantMap(contact);
            jsonArray.append(jsonObj);
        }
        
        QJsonDocument doc(jsonArray);
        
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        
        } else {
            LOG_ERROR("Failed to save recent contacts to local file");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception while saving recent contacts: %1").arg(e.what()));
    }
}

void RecentContactsManager::loadFromLocal()
{
    QMutexLocker locker(&_mutex);
    
    try {
        QString filePath = getRecentContactsFilePath();
        
        QFile file(filePath);
        if (!file.exists()) {
        
            return;
        }
        
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray()) {
                QJsonArray jsonArray = doc.array();
                
                _recentContacts.clear();
                for (const auto& value : jsonArray) {
                    if (value.isObject()) {
                        QJsonObject jsonObj = value.toObject();
                        QVariantMap contact = jsonObj.toVariantMap();
                        _recentContacts.append(contact);
                    }
                }
                
            
                emit recentContactsChanged();
            }
        } else {
            LOG_ERROR("Failed to open recent contacts file");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(QString("Exception while loading recent contacts: %1").arg(e.what()));
    }
}

QVariantMap RecentContactsManager::createContactData(const QVariantMap& contact)
{
    QVariantMap data;
    
    // 支持多种用户ID字段名
    qint64 userId = contact.value("user_id").toLongLong();
    if (userId <= 0) {
        userId = contact.value("id").toLongLong();
    }
    if (userId <= 0) {
        userId = contact.value("friend_id").toLongLong();
    }
    
    data["user_id"] = userId;
    data["username"] = contact.value("username");
    
    // 修复显示名称映射 - 优先使用name字段，然后是display_name，最后是username
    QString displayName = contact.value("name").toString();
    if (displayName.isEmpty()) {
        displayName = contact.value("display_name").toString();
    }
    if (displayName.isEmpty()) {
        displayName = contact.value("displayName").toString();
    }
    if (displayName.isEmpty()) {
        displayName = contact.value("username").toString();
    }
    data["display_name"] = displayName;
    
    // 修复头像字段映射 - 优先使用avatar，然后是avatar_url
    QString avatarUrl = contact.value("avatar").toString();
    if (avatarUrl.isEmpty()) {
        avatarUrl = contact.value("avatar_url").toString();
    }
    data["avatar_url"] = avatarUrl;
    
    // 处理在线状态
    bool isOnline = contact.value("is_online", false).toBool();
    if (!isOnline) {
        QString status = contact.value("status", "").toString();
        isOnline = (status == "online");
    }
    data["is_online"] = isOnline;
    
    // 其他字段
    data["last_message"] = contact.value("last_message", "");
    data["last_message_time"] = contact.value("last_message_time", "");
    data["unread_count"] = contact.value("unread_count", 0);
    data["created_at"] = contact.value("created_at", "");
    data["updated_at"] = contact.value("updated_at", "");
    data["is_group"] = contact.value("is_group", false);
    
    return data;
}

QString RecentContactsManager::getRecentContactsFilePath()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    
    // 获取当前用户ID，用于区分不同用户的最近联系人数据
    auto networkClient = NetworkClient::instance();
    qint64 currentUserId = 0;
    if (networkClient) {
        currentUserId = networkClient->userId();
    }
    
    QString fileName = currentUserId > 0 ? 
        QString("recent_contacts_%1.json").arg(currentUserId) : 
        "recent_contacts.json";
    return dataDir + "/" + fileName;
}