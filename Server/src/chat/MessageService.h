#ifndef MESSAGESERVICE_H
#define MESSAGESERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMutex>
#include <QDateTime>
#include <QUuid>
#include "../utils/Logger.h"

/**
 * @brief 消息管理服务类
 * 负责处理消息发送、接收、存储、推送等功能
 */
class MessageService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 消息类型枚举
     */
    enum MessageType {
        Text,
        Image,
        File,
        Audio,
        Video,
        System,
        Location,
        Contact
    };
    Q_ENUM(MessageType)

    /**
     * @brief 消息投递状态枚举
     */
    enum DeliveryStatus {
        Sent,
        Delivered,
        Read,
        Failed
    };
    Q_ENUM(DeliveryStatus)

    /**
     * @brief 消息信息结构
     */
    struct MessageInfo {
        qint64 id;
        QString messageId;
        qint64 senderId;
        qint64 receiverId;
        MessageType type;
        QString content;
        QString fileUrl;
        qint64 fileSize;
        QString fileHash;
        DeliveryStatus status;
        QDateTime createdAt;
        QDateTime updatedAt;
        
        MessageInfo() : id(-1), senderId(-1), receiverId(-1), type(Text), fileSize(0), status(Sent) {}
    };

    explicit MessageService(QObject *parent = nullptr);
    ~MessageService();

    /**
     * @brief 获取单例实例
     */
    static MessageService* instance();

    /**
     * @brief 初始化服务
     */
    bool initialize();

    /**
     * @brief 发送消息
     * @param senderId 发送者ID
     * @param receiverId 接收者ID
     * @param type 消息类型
     * @param content 消息内容
     * @param fileUrl 文件URL（可选）
     * @param fileSize 文件大小（可选）
     * @param fileHash 文件哈希（可选）
     * @return 消息ID，失败返回空字符串
     */
    QString sendMessage(qint64 senderId, qint64 receiverId, MessageType type, const QString& content,
                       const QString& fileUrl = QString(), qint64 fileSize = 0, const QString& fileHash = QString());

    /**
     * @brief 获取聊天历史
     * @param userId1 用户1 ID
     * @param userId2 用户2 ID
     * @param limit 消息数量限制
     * @param offset 偏移量
     * @return 消息列表JSON数组
     */
    QJsonArray getChatHistory(qint64 userId1, qint64 userId2, int limit = 50, int offset = 0);

    /**
     * @brief 获取用户的聊天会话列表
     * @param userId 用户ID
     * @return 会话列表JSON数组
     */
    QJsonArray getChatSessions(qint64 userId);

    /**
     * @brief 标记消息为已读
     * @param userId 用户ID
     * @param messageId 消息ID
     * @return 是否成功
     */
    bool markMessageAsRead(qint64 userId, const QString& messageId);

    /**
     * @brief 批量标记消息为已读
     * @param userId 用户ID
     * @param messageIds 消息ID列表
     * @return 成功标记的数量
     */
    int markMessagesAsRead(qint64 userId, const QStringList& messageIds);

    /**
     * @brief 获取未读消息数量
     * @param userId 用户ID
     * @param fromUserId 来自特定用户的未读消息（可选）
     * @return 未读消息数量
     */
    int getUnreadMessageCount(qint64 userId, qint64 fromUserId = -1);

    /**
     * @brief 获取离线消息
     * @param userId 用户ID
     * @return 离线消息列表JSON数组
     */
    QJsonArray getOfflineMessages(qint64 userId);

    /**
     * @brief 删除消息
     * @param userId 用户ID（只能删除自己发送的消息）
     * @param messageId 消息ID
     * @return 是否成功
     */
    bool deleteMessage(qint64 userId, const QString& messageId);

    /**
     * @brief 撤回消息
     * @param userId 用户ID
     * @param messageId 消息ID
     * @return 是否成功
     */
    bool recallMessage(qint64 userId, const QString& messageId);

    /**
     * @brief 搜索消息
     * @param userId 用户ID
     * @param keyword 搜索关键词
     * @param chatUserId 特定聊天用户ID（可选）
     * @param limit 结果限制
     * @return 搜索结果JSON数组
     */
    QJsonArray searchMessages(qint64 userId, const QString& keyword, qint64 chatUserId = -1, int limit = 20);

    /**
     * @brief 更新消息投递状态
     * @param messageId 消息ID
     * @param status 新状态
     * @return 是否成功
     */
    bool updateMessageStatus(const QString& messageId, DeliveryStatus status);

    /**
     * @brief 推送消息给在线用户
     * @param userId 用户ID
     * @param message 消息JSON对象
     * @return 是否成功推送
     */
    bool pushMessageToUser(qint64 userId, const QJsonObject& message);

    /**
     * @brief 将消息加入离线队列
     * @param userId 用户ID
     * @param messageId 消息数据库ID
     * @param priority 优先级
     * @return 是否成功
     */
    bool addToOfflineQueue(qint64 userId, qint64 messageId, int priority = 1);

    /**
     * @brief 处理离线消息队列
     * @param userId 用户ID
     * @return 处理的消息数量
     */
    int processOfflineQueue(qint64 userId);

    /**
     * @brief 消息类型转字符串
     */
    static QString messageTypeToString(MessageType type);

    /**
     * @brief 字符串转消息类型
     */
    static MessageType stringToMessageType(const QString& typeStr);

    /**
     * @brief 投递状态转字符串
     */
    static QString deliveryStatusToString(DeliveryStatus status);

    /**
     * @brief 字符串转投递状态
     */
    static DeliveryStatus stringToDeliveryStatus(const QString& statusStr);

signals:
    /**
     * @brief 新消息信号
     * @param senderId 发送者ID
     * @param receiverId 接收者ID
     * @param messageId 消息ID
     * @param message 消息JSON对象
     */
    void newMessage(qint64 senderId, qint64 receiverId, const QString& messageId, const QJsonObject& message);

    /**
     * @brief 消息状态更新信号
     * @param messageId 消息ID
     * @param oldStatus 旧状态
     * @param newStatus 新状态
     */
    void messageStatusUpdated(const QString& messageId, DeliveryStatus oldStatus, DeliveryStatus newStatus);

    /**
     * @brief 消息已读信号
     * @param userId 用户ID
     * @param messageId 消息ID
     */
    void messageRead(qint64 userId, const QString& messageId);

private:
    /**
     * @brief 获取数据库连接
     */
    QSqlDatabase getDatabase();

    /**
     * @brief 生成消息ID
     */
    QString generateMessageId();

    /**
     * @brief 获取消息信息
     */
    MessageInfo getMessageInfo(const QString& messageId);

    /**
     * @brief 构建消息JSON对象
     */
    QJsonObject buildMessageJson(const MessageInfo& messageInfo);

    /**
     * @brief 检查用户是否为好友
     */
    bool areUsersFriends(qint64 userId1, qint64 userId2);

    static MessageService* s_instance;
    static QMutex s_instanceMutex;
    
    bool _initialized;
    mutable QMutex _mutex;
};

#endif // MESSAGESERVICE_H
