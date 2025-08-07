#ifndef AUTHRESPONSE_H
#define AUTHRESPONSE_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "User.h"

/**
 * @brief 认证响应数据模型类
 * 
 * 封装服务器认证响应的数据结构，包括成功状态、错误信息、用户数据和会话令牌。
 * 用于处理登录、注册等认证操作的服务器响应。
 */
class AuthResponse : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool success READ success WRITE setSuccess NOTIFY successChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString errorCode READ errorCode WRITE setErrorCode NOTIFY errorCodeChanged)
    Q_PROPERTY(User* user READ user WRITE setUser NOTIFY userChanged)
    Q_PROPERTY(QString sessionToken READ sessionToken WRITE setSessionToken NOTIFY sessionTokenChanged)

public:
    explicit AuthResponse(QObject *parent = nullptr);
    explicit AuthResponse(const QJsonObject &json, QObject *parent = nullptr);
    
    // Getter methods
    bool success() const { return _success; }
    QString message() const { return _message; }
    QString errorCode() const { return _errorCode; }
    User* user() const { return _user; }
    QString sessionToken() const { return _sessionToken; }
    
    // Setter methods
    void setSuccess(bool success);
    void setMessage(const QString &message);
    void setErrorCode(const QString &errorCode);
    void setUser(User* user);
    void setSessionToken(const QString &sessionToken);
    
    /**
     * @brief 从JSON对象加载响应数据
     * @param json JSON对象
     */
    void fromJson(const QJsonObject &json);
    
    /**
     * @brief 将响应数据转换为JSON对象
     * @return JSON对象
     */
    QJsonObject toJson() const;
    
    /**
     * @brief 检查响应是否有效
     * @return 是否有效
     */
    bool isValid() const;
    
    /**
     * @brief 清空响应数据
     */
    void clear();
    
    /**
     * @brief 设置错误响应
     * @param errorCode 错误代码
     * @param message 错误消息
     */
    void setError(const QString &errorCode, const QString &message);
    
    /**
     * @brief 设置成功响应
     * @param message 成功消息
     * @param user 用户数据
     * @param sessionToken 会话令牌
     */
    void setSuccess(const QString &message, User* user, const QString &sessionToken = "");

signals:
    void successChanged();
    void messageChanged();
    void errorCodeChanged();
    void userChanged();
    void sessionTokenChanged();

private:
    bool _success;
    QString _message;
    QString _errorCode;
    User* _user;
    QString _sessionToken;
};

#endif // AUTHRESPONSE_H
