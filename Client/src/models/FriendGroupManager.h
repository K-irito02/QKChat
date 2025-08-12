#ifndef FRIENDGROUPMANAGER_H
#define FRIENDGROUPMANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>
#include <QTimer>

/**
 * @brief 好友分组管理器
 * 
 * 管理好友分组数据，提供QML接口用于分组操作
 */
class FriendGroupManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    // QML属性
    Q_PROPERTY(QVariantList friendGroups READ friendGroups NOTIFY friendGroupsChanged)
    Q_PROPERTY(QVariantList recentContacts READ recentContacts NOTIFY recentContactsChanged)
    Q_PROPERTY(QVariantList chatGroups READ chatGroups NOTIFY chatGroupsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    
public:
    explicit FriendGroupManager(QObject *parent = nullptr);
    
    // QML属性访问器
    QVariantList friendGroups() const { return _friendGroups; }
    QVariantList recentContacts() const { return _recentContacts; }
    QVariantList chatGroups() const { return _chatGroups; }
    bool isLoading() const { return _isLoading; }
    
    // QML可调用方法
    Q_INVOKABLE void loadFriendGroups();
    Q_INVOKABLE void loadRecentContacts();
    Q_INVOKABLE void loadChatGroups();
    Q_INVOKABLE void createFriendGroup(const QString& groupName);
    Q_INVOKABLE void renameFriendGroup(int groupId, const QString& newName);
    Q_INVOKABLE void deleteFriendGroup(int groupId);
    Q_INVOKABLE void moveFriendToGroup(int friendId, int groupId);
    Q_INVOKABLE void expandGroup(const QString& groupId, bool expanded);
    Q_INVOKABLE void refreshData();
    
    // 数据处理方法
    void handleFriendGroupsReceived(const QJsonArray& groups);
    void handleFriendListReceived(const QJsonArray& friends);
    void handleRecentContactsReceived(const QJsonArray& contacts);
    void handleChatGroupsReceived(const QJsonArray& groups);
    
    // 操作结果处理
    void handleGroupCreated(const QString& groupName, bool success);
    void handleGroupRenamed(int groupId, const QString& newName, bool success);
    void handleGroupDeleted(int groupId, bool success);
    void handleFriendMoved(int friendId, int groupId, bool success);

signals:
    // 属性变化信号
    void friendGroupsChanged();
    void recentContactsChanged();
    void chatGroupsChanged();
    void isLoadingChanged();
    
    // 操作结果信号
    void operationCompleted(const QString& operation, bool success, const QString& message);
    void dataRefreshed();

private slots:
    void onRefreshTimer();

private:
    // 数据成员
    QVariantList _friendGroups;
    QVariantList _recentContacts;
    QVariantList _chatGroups;
    bool _isLoading;
    
    // 辅助成员
    QTimer* _refreshTimer;
    QJsonArray _rawFriendGroups;
    QJsonArray _rawFriendList;
    
    // 私有方法
    void setIsLoading(bool loading);
    void updateFriendGroupsData();
    QVariantMap createGroupData(const QJsonObject& group, const QJsonArray& members);
    QVariantMap createMemberData(const QJsonObject& member);
    QVariantMap createRecentContactData(const QJsonObject& contact);
    QVariantMap createChatGroupData(const QJsonObject& group);
    
    // 数据查找方法
    int findGroupIndex(int groupId);
    int findMemberIndex(int groupIndex, int memberId);
    void sortGroupsByOrder();
};

#endif // FRIENDGROUPMANAGER_H
