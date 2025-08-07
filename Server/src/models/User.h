#ifndef USER_H
#define USER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>

/**
 * @brief 用户数据模型类（服务器端）
 * 
 * 封装用户相关的数据和操作，提供用户信息的存储、序列化和反序列化功能。
 * 服务器端版本包含更多的内部字段，如密码哈希、盐值等敏感信息。
 */
class User : public QObject
{
    Q_OBJECT

public:
    explicit User(QObject *parent = nullptr);
    explicit User(const QJsonObject &json, QObject *parent = nullptr);
    
    // Getter methods
    qint64 id() const { return _id; }
    QString username() const { return _username; }
    QString email() const { return _email; }
    QString displayName() const { return _displayName; }
    QString passwordHash() const { return _passwordHash; }
    QString salt() const { return _salt; }
    QString avatarUrl() const { return _avatarUrl; }
    QString status() const { return _status; }
    QString theme() const { return _theme; }
    bool isVerified() const { return _isVerified; }
    bool isActive() const { return _isActive; }
    QDateTime createdAt() const { return _createdAt; }
    QDateTime updatedAt() const { return _updatedAt; }
    QDateTime lastLogin() const { return _lastLogin; }
    
    // Setter methods
    void setId(qint64 id) { _id = id; }
    void setUsername(const QString &username) { _username = username; }
    void setEmail(const QString &email) { _email = email; }
    void setDisplayName(const QString &displayName) { _displayName = displayName; }
    void setPasswordHash(const QString &passwordHash) { _passwordHash = passwordHash; }
    void setSalt(const QString &salt) { _salt = salt; }
    void setAvatarUrl(const QString &avatarUrl) { _avatarUrl = avatarUrl; }
    void setStatus(const QString &status) { _status = status; }
    void setTheme(const QString &theme) { _theme = theme; }
    void setIsVerified(bool isVerified) { _isVerified = isVerified; }
    void setIsActive(bool isActive) { _isActive = isActive; }
    void setCreatedAt(const QDateTime &createdAt) { _createdAt = createdAt; }
    void setUpdatedAt(const QDateTime &updatedAt) { _updatedAt = updatedAt; }
    void setLastLogin(const QDateTime &lastLogin) { _lastLogin = lastLogin; }
    
    /**
     * @brief 从JSON对象加载用户数据
     * @param json JSON对象
     */
    void fromJson(const QJsonObject &json);
    
    /**
     * @brief 将用户数据转换为JSON对象（客户端安全版本）
     * @return JSON对象（不包含敏感信息）
     */
    QJsonObject toClientJson() const;
    
    /**
     * @brief 将用户数据转换为JSON对象（完整版本）
     * @return JSON对象（包含所有字段）
     */
    QJsonObject toFullJson() const;
    
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
    
    /**
     * @brief 验证密码
     * @param password 原始密码
     * @return 密码是否正确
     */
    bool verifyPassword(const QString &password) const;
    
    /**
     * @brief 设置密码
     * @param password 原始密码
     */
    void setPassword(const QString &password);
    
    /**
     * @brief 更新最后登录时间为当前时间
     */
    void updateLastLogin();
    
    /**
     * @brief 获取用户显示名称（优先显示名，否则用户名）
     * @return 显示名称
     */
    QString getDisplayName() const;
    
    /**
     * @brief 检查用户是否可以登录
     * @return 是否可以登录
     */
    bool canLogin() const;

private:
    /**
     * @brief 生成随机盐值
     * @return 盐值
     */
    QString generateSalt() const;
    
    /**
     * @brief 哈希密码
     * @param password 原始密码
     * @param salt 盐值
     * @return 哈希后的密码
     */
    QString hashPassword(const QString &password, const QString &salt) const;

private:
    qint64 _id;
    QString _username;
    QString _email;
    QString _displayName;
    QString _passwordHash;
    QString _salt;
    QString _avatarUrl;
    QString _status;
    QString _theme;
    bool _isVerified;
    bool _isActive;
    QDateTime _createdAt;
    QDateTime _updatedAt;
    QDateTime _lastLogin;
};

#endif // USER_H
