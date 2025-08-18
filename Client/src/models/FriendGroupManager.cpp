#include "FriendGroupManager.h"
#include "RecentContactsManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QThread>

FriendGroupManager::FriendGroupManager(QObject *parent)
    : QObject(parent)
    , _isLoading(false)
    , _refreshTimer(new QTimer(this))
{
    // 设置定时刷新
    _refreshTimer->setInterval(30000); // 30秒刷新一次
    _refreshTimer->setSingleShot(false);
    connect(_refreshTimer, &QTimer::timeout, this, &FriendGroupManager::onRefreshTimer);
    
    // 初始化数据
    refreshData();
}

void FriendGroupManager::loadFriendGroups()
{
    setIsLoading(true);
    // 通过ChatNetworkClient请求数据
    // 这里应该调用网络请求，暂时保持为空等待网络客户端集成
    // 注意：分组数据应该通过ChatNetworkClient.getFriendGroups()获取
    setIsLoading(false);
}

void FriendGroupManager::createFriendGroup(const QString& groupName)
{
    if (groupName.trimmed().isEmpty()) {
        emit operationCompleted("create", false, "分组名称不能为空");
        return;
    }
    
    setIsLoading(true);
    // 这里应该调用ChatNetworkClient::createFriendGroup
    // 暂时保持为空等待网络客户端集成
    setIsLoading(false);
}

void FriendGroupManager::renameFriendGroup(int groupId, const QString& newName)
{
    if (newName.trimmed().isEmpty()) {
        emit operationCompleted("rename", false, "分组名称不能为空");
        return;
    }
    
    setIsLoading(true);
    // 这里应该调用ChatNetworkClient::renameFriendGroup
    // 暂时保持为空等待网络客户端集成
    setIsLoading(false);
}

void FriendGroupManager::deleteFriendGroup(int groupId)
{
    setIsLoading(true);
    // 这里应该调用ChatNetworkClient::deleteFriendGroup
    // 暂时保持为空等待网络客户端集成
    setIsLoading(false);
}

void FriendGroupManager::moveFriendToGroup(int friendId, int groupId)
{
    setIsLoading(true);
    // 这里应该调用ChatNetworkClient::moveFriendToGroup
    // 暂时保持为空等待网络客户端集成
    setIsLoading(false);
}

void FriendGroupManager::expandGroup(const QString& groupId, bool expanded)
{
    // 更新本地展开状态
    for (int i = 0; i < _friendGroups.size(); ++i) {
        QVariantMap group = _friendGroups[i].toMap();
        if (group["id"].toString() == groupId) {
            group["expanded"] = expanded;
            _friendGroups[i] = group;
            emit friendGroupsChanged();
            break;
        }
    }
}

void FriendGroupManager::refreshData()
{

    
    // 重新处理现有的好友和分组数据
    updateFriendGroupsData();
    
    // 启动定时刷新
    if (!_refreshTimer->isActive()) {
        _refreshTimer->start();
    }
    

}

void FriendGroupManager::loadRecentContacts()
{
    // 通过ChatNetworkClient请求最近联系数据
    // 暂时保持为空等待网络客户端集成
}

void FriendGroupManager::loadChatGroups()
{
    // 通过ChatNetworkClient请求群组数据
    // 暂时保持为空等待网络客户端集成
}

void FriendGroupManager::handleFriendGroupsReceived(const QJsonArray& groups)
{

    _rawFriendGroups = groups;
    updateFriendGroupsData();

}

void FriendGroupManager::handleFriendListReceived(const QJsonArray& friends)
{
    _rawFriendList = friends;
    updateFriendGroupsData();
    
    // 过滤最近联系人，只保留有效的好友
    auto recentManager = RecentContactsManager::instance();
    if (recentManager) {
        QVariantList friendList;
        for (const QJsonValue& value : friends) {
            friendList.append(value.toObject().toVariantMap());
        }
        recentManager->filterByFriendList(friendList);
    }
}

void FriendGroupManager::handleRecentContactsReceived(const QJsonArray& contacts)
{
    QVariantList groupList;

    // 创建一个默认的最近联系分组
    QVariantMap recentGroup;
    recentGroup["id"] = "recent_default";
    recentGroup["name"] = "最近联系";
    recentGroup["order"] = 0;
    recentGroup["isDefault"] = true;
    recentGroup["expanded"] = true;

    QVariantList memberList;
    for (const QJsonValue& value : contacts) {
        QJsonObject contact = value.toObject();
        memberList.append(createRecentContactData(contact));
    }
    recentGroup["members"] = memberList;

    groupList.append(recentGroup);

    _recentContacts = groupList;
    emit recentContactsChanged();
}

void FriendGroupManager::handleChatGroupsReceived(const QJsonArray& groups)
{
    QVariantList categoryList;

    // 创建一个默认的群组分类
    QVariantMap groupCategory;
    groupCategory["id"] = "groups_default";
    groupCategory["name"] = "我的群组";
    groupCategory["order"] = 0;
    groupCategory["isDefault"] = true;
    groupCategory["expanded"] = true;

    QVariantList memberList;
    for (const QJsonValue& value : groups) {
        QJsonObject group = value.toObject();
        memberList.append(createChatGroupData(group));
    }
    groupCategory["members"] = memberList;

    categoryList.append(groupCategory);

    _chatGroups = categoryList;
    emit chatGroupsChanged();
}

void FriendGroupManager::handleGroupCreated(const QString& groupName, bool success)
{
    if (success) {
        // 添加新分组到本地数据
        QJsonObject newGroup;
        newGroup["id"] = _rawFriendGroups.size() + 1;
        newGroup["group_name"] = groupName;
        newGroup["group_order"] = _rawFriendGroups.size();
        newGroup["friend_count"] = 0;
        
        _rawFriendGroups.append(newGroup);
        updateFriendGroupsData();
        
        emit operationCompleted("create", true, QString("分组 \"%1\" 创建成功").arg(groupName));
    } else {
        emit operationCompleted("create", false, QString("创建分组 \"%1\" 失败").arg(groupName));
    }
}

void FriendGroupManager::handleGroupRenamed(int groupId, const QString& newName, bool success)
{
    if (success) {
        // 更新本地数据
        for (int i = 0; i < _rawFriendGroups.size(); ++i) {
            QJsonObject group = _rawFriendGroups[i].toObject();
            if (group["id"].toInt() == groupId) {
                group["group_name"] = newName;
                _rawFriendGroups[i] = group;
                break;
            }
        }
        updateFriendGroupsData();
        
        emit operationCompleted("rename", true, QString("分组重命名为 \"%1\" 成功").arg(newName));
    } else {
        emit operationCompleted("rename", false, "分组重命名失败");
    }
}

void FriendGroupManager::handleGroupDeleted(int groupId, bool success)
{
    if (success) {
        // 从本地数据中移除分组
        for (int i = 0; i < _rawFriendGroups.size(); ++i) {
            QJsonObject group = _rawFriendGroups[i].toObject();
            if (group["id"].toInt() == groupId) {
                _rawFriendGroups.removeAt(i);
                break;
            }
        }
        
        // 将该分组的好友移动到默认分组
        for (int i = 0; i < _rawFriendList.size(); ++i) {
            QJsonObject friend_ = _rawFriendList[i].toObject();
            if (friend_["group_id"].toInt() == groupId) {
                friend_["group_id"] = 1; // 移动到默认分组
                _rawFriendList[i] = friend_;
            }
        }
        
        updateFriendGroupsData();
        emit operationCompleted("delete", true, "分组删除成功");
    } else {
        emit operationCompleted("delete", false, "分组删除失败");
    }
}

void FriendGroupManager::handleFriendMoved(int friendId, int groupId, bool success)
{
    if (success) {
        // 更新本地数据
        for (int i = 0; i < _rawFriendList.size(); ++i) {
            QJsonObject friend_ = _rawFriendList[i].toObject();
            if (friend_["id"].toInt() == friendId) {
                friend_["group_id"] = groupId;
                _rawFriendList[i] = friend_;
                break;
            }
        }
        updateFriendGroupsData();
        
        emit operationCompleted("move", true, "好友移动成功");
    } else {
        emit operationCompleted("move", false, "好友移动失败");
    }
}

void FriendGroupManager::onRefreshTimer()
{
    // 定时刷新数据
    loadFriendGroups();
}

void FriendGroupManager::setIsLoading(bool loading)
{
    if (_isLoading != loading) {
        _isLoading = loading;
        emit isLoadingChanged();
    }
}

void FriendGroupManager::updateFriendGroupsData()
{
    QVariantList groupList;
    
    // 如果没有分组数据，创建一个默认分组
    if (_rawFriendGroups.isEmpty()) {
        QJsonObject defaultGroup;
        defaultGroup["id"] = 1;
        defaultGroup["group_name"] = "默认分组";
        defaultGroup["group_order"] = 0;
        defaultGroup["friend_count"] = _rawFriendList.size();
        _rawFriendGroups.append(defaultGroup);
    }
    
    // 按分组组织好友数据
    for (const QJsonValue& groupValue : _rawFriendGroups) {
        QJsonObject group = groupValue.toObject();
        int groupId = group["id"].toInt();
        
        // 查找该分组的好友
        QJsonArray groupMembers;
        for (const QJsonValue& friendValue : _rawFriendList) {
            QJsonObject friend_ = friendValue.toObject();
            // 从服务器数据中获取group_id，如果没有则使用默认分组
            int friendGroupId = 1; // 默认分组ID
            if (friend_.contains("group_id") && !friend_["group_id"].isNull()) {
                friendGroupId = friend_["group_id"].toInt();
            }
            
            // 检查好友是否属于当前分组
            if (friendGroupId == groupId) {
                groupMembers.append(friend_);
                qDebug() << "Added friend" << friend_["username"].toString() << "to group" << group["group_name"].toString();
            }
        }
        
        // 更新分组的friend_count
        QJsonObject updatedGroup = group;
        updatedGroup["friend_count"] = groupMembers.size();
        
        groupList.append(createGroupData(updatedGroup, groupMembers));
    }
    
    // 处理没有分组的好友（group_id为null或0，或者group_id不在现有分组中）
    QJsonArray ungroupedMembers;
    for (const QJsonValue& friendValue : _rawFriendList) {
        QJsonObject friend_ = friendValue.toObject();
        bool foundGroup = false;
        
        // 获取好友的分组ID
        int friendGroupId = 1; // 默认分组ID
        if (friend_.contains("group_id") && !friend_["group_id"].isNull()) {
            friendGroupId = friend_["group_id"].toInt();
        }
        
        // 检查好友的分组ID是否在现有分组中
        for (const QJsonValue& groupValue : _rawFriendGroups) {
            QJsonObject group = groupValue.toObject();
            int groupId = group["id"].toInt();
            if (friendGroupId == groupId) {
                foundGroup = true;
                break;
            }
        }
        
        // 如果没有找到对应的分组，添加到未分组列表
        if (!foundGroup) {
            ungroupedMembers.append(friend_);
            qDebug() << "Found ungrouped friend:" << friend_["username"].toString() << "with group_id:" << friendGroupId;
        }
    }
    
    // 如果有未分组的好友，创建一个默认分组
    if (!ungroupedMembers.isEmpty()) {
        QJsonObject defaultGroup;
        defaultGroup["id"] = 1;
        defaultGroup["group_name"] = "默认分组";
        defaultGroup["group_order"] = 0;
        defaultGroup["friend_count"] = ungroupedMembers.size();
        
        groupList.append(createGroupData(defaultGroup, ungroupedMembers));
        qDebug() << "Created default group with" << ungroupedMembers.size() << "ungrouped friends";
    }
    
    _friendGroups = groupList;

    emit friendGroupsChanged();
}

QVariantMap FriendGroupManager::createGroupData(const QJsonObject& group, const QJsonArray& members)
{
    QVariantMap groupData;
    groupData["id"] = group["id"].toInt();
    groupData["name"] = group["group_name"].toString();
    groupData["order"] = group["group_order"].toInt();
    groupData["isDefault"] = group["group_name"].toString() == "默认分组";
    groupData["expanded"] = true; // 默认展开
    
    QVariantList memberList;
    for (const QJsonValue& memberValue : members) {
        QJsonObject member = memberValue.toObject();
        memberList.append(createMemberData(member));
    }
    groupData["members"] = memberList;
    
    return groupData;
}

QVariantMap FriendGroupManager::createMemberData(const QJsonObject& member)
{
    QVariantMap memberData;
    memberData["id"] = member["friend_id"].toVariant().toLongLong(); // 使用friend_id作为id
    memberData["username"] = member["username"].toString();
    memberData["name"] = member["display_name"].toString();
    memberData["displayName"] = member["display_name"].toString();
    memberData["avatar"] = member["avatar_url"].toString();
    memberData["status"] = member["online_status"].toString(); // 使用online_status
    memberData["signature"] = member["note"].toString(); // 使用note作为signature
    
    // 从服务器数据中获取group_id，如果没有则使用默认分组
    if (member.contains("group_id") && !member["group_id"].isNull()) {
        memberData["groupId"] = member["group_id"].toVariant().toLongLong();
    } else {
        memberData["groupId"] = 1; // 默认分组ID
    }
    
    return memberData;
}

QVariantMap FriendGroupManager::createRecentContactData(const QJsonObject& contact)
{
    QVariantMap contactData;
    contactData["id"] = contact["id"].toInt();
    contactData["name"] = contact["name"].toString();
    contactData["avatar"] = contact["avatar"].toString();
    contactData["lastMessage"] = contact["last_message"].toString();
    contactData["lastTime"] = contact["last_time"].toString();
    contactData["unreadCount"] = contact["unread_count"].toInt();
    contactData["type"] = contact["type"].toString(); // "friend" or "group"
    
    return contactData;
}

QVariantMap FriendGroupManager::createChatGroupData(const QJsonObject& group)
{
    QVariantMap groupData;
    groupData["id"] = group["id"].toInt();
    groupData["name"] = group["name"].toString();
    groupData["avatar"] = group["avatar"].toString();
    groupData["description"] = group["description"].toString();
    groupData["memberCount"] = group["member_count"].toInt();
    groupData["type"] = "group";

    return groupData;
}

int FriendGroupManager::findGroupIndex(int groupId)
{
    for (int i = 0; i < _friendGroups.size(); ++i) {
        QVariantMap group = _friendGroups[i].toMap();
        if (group["id"].toInt() == groupId) {
            return i;
        }
    }
    return -1;
}

int FriendGroupManager::findMemberIndex(int groupIndex, int memberId)
{
    if (groupIndex < 0 || groupIndex >= _friendGroups.size()) {
        return -1;
    }

    QVariantMap group = _friendGroups[groupIndex].toMap();
    QVariantList members = group["members"].toList();

    for (int i = 0; i < members.size(); ++i) {
        QVariantMap member = members[i].toMap();
        if (member["id"].toInt() == memberId) {
            return i;
        }
    }
    return -1;
}

void FriendGroupManager::sortGroupsByOrder()
{
    std::sort(_friendGroups.begin(), _friendGroups.end(),
              [](const QVariant& a, const QVariant& b) {
                  return a.toMap()["order"].toInt() < b.toMap()["order"].toInt();
              });
}
