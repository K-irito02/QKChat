#ifndef USER_H
#define USER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDateTime>

/**
 * @brief 用户数据模型类
 * 
 * 封装用户相关的数据和操作，提供用户信息的存储、序列化和反序列化功能。
 * 支持JSON格式的数据交换，便于网络传输和本地存储。
 */
class User : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 id READ id WRITE setId NOTIFY idChanged)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString email READ email WRITE setEmail NOTIFY emailChanged)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl WRITE setAvatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QDateTime createdAt READ createdAt WRITE setCreatedAt NOTIFY createdAtChanged)
    Q_PROPERTY(QDateTime lastLogin READ lastLogin WRITE setLastLogin NOTIFY lastLoginChanged)

public:
    explicit User(QObject *parent = nullptr);
    explicit User(const QJsonObject &json, QObject *parent = nullptr);
    
    // Getter methods
    qint64 id() const { return _id; }
    QString username() const { return _username; }
    QString email() const { return _email; }
    QString displayName() const { return _displayName; }
    QString avatarUrl() const { return _avatarUrl; }
    QString status() const { return _status; }
    QString theme() const { return _theme; }
    QDateTime createdAt() const { return _createdAt; }
    QDateTime lastLogin() const { return _lastLogin; }
    
    // Setter methods
    void setId(qint64 id);
    void setUsername(const QString &username);
    void setEmail(const QString &email);
    void setDisplayName(const QString &displayName);
    void setAvatarUrl(const QString &avatarUrl);
    void setStatus(const QString &status);
    void setTheme(const QString &theme);
    void setCreatedAt(const QDateTime &createdAt);
    void setLastLogin(const QDateTime &lastLogin);
    
    /**
     * @brief 从JSON对象加载用户数据
     * @param json JSON对象
     */
    void fromJson(const QJsonObject &json);
    
    /**
     * @brief 将用户数据转换为JSON对象
     * @return JSON对象
     */
    QJsonObject toJson() const;
    
    /**
     * @brief 检查用户数据是否有效
     * @return 是否有效
     */
    bool isValid() const;
    
    /**
     * @brief 清空用户数据
     */
    void clear();
    
    /**
     * @brief 复制用户数据
     * @param other 其他用户对象
     */
    void copyFrom(const User &other);

signals:
    void idChanged();
    void usernameChanged();
    void emailChanged();
    void displayNameChanged();
    void avatarUrlChanged();
    void statusChanged();
    void themeChanged();
    void createdAtChanged();
    void lastLoginChanged();

private:
    qint64 _id;
    QString _username;
    QString _email;
    QString _displayName;
    QString _avatarUrl;
    QString _status;
    QString _theme;
    QDateTime _createdAt;
    QDateTime _lastLogin;
};

#endif // USER_H
