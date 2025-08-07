#include "User.h"
#include <QJsonDocument>
#include <QDebug>

User::User(QObject *parent)
    : QObject(parent)
    , _id(0)
    , _status("offline")
    , _theme("light")
{
}

User::User(const QJsonObject &json, QObject *parent)
    : QObject(parent)
    , _id(0)
    , _status("offline")
    , _theme("light")
{
    fromJson(json);
}

void User::setId(qint64 id)
{
    if (_id != id) {
        _id = id;
        emit idChanged();
    }
}

void User::setUsername(const QString &username)
{
    if (_username != username) {
        _username = username;
        emit usernameChanged();
    }
}

void User::setEmail(const QString &email)
{
    if (_email != email) {
        _email = email;
        emit emailChanged();
    }
}

void User::setDisplayName(const QString &displayName)
{
    if (_displayName != displayName) {
        _displayName = displayName;
        emit displayNameChanged();
    }
}

void User::setAvatarUrl(const QString &avatarUrl)
{
    if (_avatarUrl != avatarUrl) {
        _avatarUrl = avatarUrl;
        emit avatarUrlChanged();
    }
}

void User::setStatus(const QString &status)
{
    if (_status != status) {
        _status = status;
        emit statusChanged();
    }
}

void User::setTheme(const QString &theme)
{
    if (_theme != theme) {
        _theme = theme;
        emit themeChanged();
    }
}

void User::setCreatedAt(const QDateTime &createdAt)
{
    if (_createdAt != createdAt) {
        _createdAt = createdAt;
        emit createdAtChanged();
    }
}

void User::setLastLogin(const QDateTime &lastLogin)
{
    if (_lastLogin != lastLogin) {
        _lastLogin = lastLogin;
        emit lastLoginChanged();
    }
}

void User::fromJson(const QJsonObject &json)
{
    setId(json["id"].toVariant().toLongLong());
    setUsername(json["username"].toString());
    setEmail(json["email"].toString());
    setDisplayName(json["display_name"].toString());
    setAvatarUrl(json["avatar_url"].toString());
    setStatus(json["status"].toString());
    setTheme(json["theme"].toString());
    
    if (json.contains("created_at")) {
        setCreatedAt(QDateTime::fromString(json["created_at"].toString(), Qt::ISODate));
    }
    
    if (json.contains("last_login")) {
        setLastLogin(QDateTime::fromString(json["last_login"].toString(), Qt::ISODate));
    }
}

QJsonObject User::toJson() const
{
    QJsonObject json;
    json["id"] = _id;
    json["username"] = _username;
    json["email"] = _email;
    json["display_name"] = _displayName;
    json["avatar_url"] = _avatarUrl;
    json["status"] = _status;
    json["theme"] = _theme;
    
    if (_createdAt.isValid()) {
        json["created_at"] = _createdAt.toString(Qt::ISODate);
    }
    
    if (_lastLogin.isValid()) {
        json["last_login"] = _lastLogin.toString(Qt::ISODate);
    }
    
    return json;
}

bool User::isValid() const
{
    return _id > 0 && !_username.isEmpty() && !_email.isEmpty();
}

void User::clear()
{
    setId(0);
    setUsername("");
    setEmail("");
    setDisplayName("");
    setAvatarUrl("");
    setStatus("offline");
    setTheme("light");
    setCreatedAt(QDateTime());
    setLastLogin(QDateTime());
}

void User::copyFrom(const User &other)
{
    setId(other.id());
    setUsername(other.username());
    setEmail(other.email());
    setDisplayName(other.displayName());
    setAvatarUrl(other.avatarUrl());
    setStatus(other.status());
    setTheme(other.theme());
    setCreatedAt(other.createdAt());
    setLastLogin(other.lastLogin());
}
