#ifndef CHATMESSAGEMANAGER_H
#define CHATMESSAGEMANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QMutex>

/**
 * @brief 聊天消息管理器
 * 
 * 管理聊天消息数据，提供QML接口用于消息操作
 */
class ChatMessageManager : public QObject
{
    Q_OBJECT
    
    // QML属性
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QVariantMap currentChatUser READ currentChatUser WRITE setCurrentChatUser NOTIFY currentChatUserChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMoreHistory READ hasMoreHistory NOTIFY hasMoreHistoryChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    
public:
    explicit ChatMessageManager(QObject *parent = nullptr);
    
    // QML属性访问器
    QVariantList messages() const { return _messages; }
    QVariantMap currentChatUser() const { return _currentChatUser; }
    bool isLoading() const { return _isLoading; }
    bool hasMoreHistory() const { return _hasMoreHistory; }
    int unreadCount() const { return _unreadCount; }
    
    // QML可调用方法
    Q_INVOKABLE void setCurrentChatUser(const QVariantMap& user);
    Q_INVOKABLE void sendMessage(const QString& content, const QString& type = "text");
    Q_INVOKABLE void loadChatHistory(int limit = 50, int offset = 0);
    Q_INVOKABLE void loadMoreHistory();
    Q_INVOKABLE void markMessagesAsRead();
    Q_INVOKABLE void clearMessages();
    Q_INVOKABLE void clearMessagesForUser(qint64 userId);
    Q_INVOKABLE void refreshMessages();
    
    // 数据处理方法
    Q_INVOKABLE void handleMessageReceived(const QJsonObject& message);
    Q_INVOKABLE void handleMessageSent(const QString& messageId, bool success);
    Q_INVOKABLE void handleChatHistoryReceived(qint64 userId, const QJsonArray& messages);
    Q_INVOKABLE void handleMessageStatusUpdated(const QString& messageId, const QString& status);
    
    // 获取单例实例
    static ChatMessageManager* instance();
    
    // 获取当前用户ID
    qint64 getCurrentUserId() const;

signals:
    // 属性变化信号
    void messagesChanged();
    void currentChatUserChanged();
    void isLoadingChanged();
    void hasMoreHistoryChanged();
    void unreadCountChanged();
    
    // 操作结果信号
    void messageSendResult(bool success, const QString& message);
    void newMessageReceived(const QVariantMap& message);
    void messageStatusChanged(const QString& messageId, const QString& status);

private slots:
    void onAutoRefreshTimer();

private:
    // 数据成员
    QVariantList _messages;
    QVariantMap _currentChatUser;
    bool _isLoading;
    bool _hasMoreHistory;
    int _unreadCount;
    
    // 辅助成员
    QTimer* _autoRefreshTimer;
    int _currentOffset;
    static const int DEFAULT_LIMIT = 50;
    
    // 单例相关
    static ChatMessageManager* s_instance;
    static QMutex s_instanceMutex;
    
    // 私有方法
    void setIsLoading(bool loading);
    void setHasMoreHistory(bool hasMore);
    void setUnreadCount(int count);
    QVariantMap createMessageData(const QJsonObject& message);
    void addMessage(const QVariantMap& message, bool prepend = false);
    void updateMessageStatus(const QString& messageId, const QString& status);
    int findMessageIndex(const QString& messageId);
    void sortMessagesByTime();
    QString formatMessageTime(const QDateTime& dateTime);
    
    mutable QMutex _mutex;
};

#endif // CHATMESSAGEMANAGER_H