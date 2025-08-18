#ifndef RECENTCONTACTSMANAGER_H
#define RECENTCONTACTSMANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QTimer>

/**
 * @brief 最近联系人管理器
 * 
 * 管理最近联系人数据，提供QML接口
 */
class RecentContactsManager : public QObject
{
    Q_OBJECT
    
    // QML属性
    Q_PROPERTY(QVariantList recentContacts READ recentContacts NOTIFY recentContactsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    
public:
    explicit RecentContactsManager(QObject *parent = nullptr);
    
    // QML属性访问器
    QVariantList recentContacts() const { return _recentContacts; }
    bool isLoading() const { return _isLoading; }
    
    // QML可调用方法
    Q_INVOKABLE void addRecentContact(const QVariantMap& contact);
    Q_INVOKABLE void removeRecentContact(qint64 userId);
    Q_INVOKABLE void updateLastMessage(qint64 userId, const QString& message, const QString& time);
    Q_INVOKABLE void updateUnreadCount(qint64 userId, int count);
    Q_INVOKABLE void clearRecentContacts();
    Q_INVOKABLE void refreshRecentContacts();
    Q_INVOKABLE void filterByFriendList(const QVariantList& friendList);
    Q_INVOKABLE void clearInvalidContacts();
    Q_INVOKABLE void cleanExpiredInvalidContacts(); // 清理过期的无效联系人
    Q_INVOKABLE void removeInvalidContact(qint64 userId); // 移除指定的无效联系人
    Q_INVOKABLE void loadDataAfterLogin(); // 登录后加载数据
    QString getRecentContactsFilePath();
    
    // 获取单例实例
    static RecentContactsManager* instance();

signals:
    // 属性变化信号
    void recentContactsChanged();
    void isLoadingChanged();
    
    // 操作结果信号
    void contactAdded(const QVariantMap& contact);
    void contactRemoved(qint64 userId);
    void contactUpdated(qint64 userId);

private slots:
    void onAutoSaveTimer();
    void onCleanupTimer(); // 定期清理定时器回调

private:
    // 数据成员
    QVariantList _recentContacts;
    bool _isLoading;
    
    // 辅助成员
    QTimer* _autoSaveTimer;
    QTimer* _cleanupTimer; // 定期清理定时器
    
    // 单例相关
    static RecentContactsManager* s_instance;
    static QMutex s_instanceMutex;
    
    // 私有方法
    void setIsLoading(bool loading);
    int findContactIndex(qint64 userId);
    void moveToTop(int index);
    void saveToLocal();
    void loadFromLocal();
    QVariantMap createContactData(const QVariantMap& contact);
    
    mutable QMutex _mutex;
};

#endif // RECENTCONTACTSMANAGER_H