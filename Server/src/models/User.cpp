#include "User.h"
#include "../utils/Crypto.h"
#include <QJsonDocument>


User::User(QObject *parent)
    : QObject(parent)
    , _id(0)
    , _status("offline")
    , _theme("light")
    , _isVerified(false)
    , _isActive(true)
{
}

User::User(const QJsonObject &json, QObject *parent)
    : QObject(parent)
    , _id(0)
    , _status("offline")
    , _theme("light")
    , _isVerified(false)
    , _isActive(true)
{
    fromJson(json);
}

void User::fromJson(const QJsonObject &json)
{
    _id = json["id"].toVariant().toLongLong();
    _username = json["username"].toString();
    _email = json["email"].toString();
    _displayName = json["display_name"].toString();
    _passwordHash = json["password_hash"].toString();
    _salt = json["salt"].toString();
    _avatarUrl = json["avatar_url"].toString();
    _status = json["status"].toString();
    _theme = json["theme"].toString();
    _isVerified = json["is_verified"].toBool();
    _isActive = json["is_active"].toBool();
    
    if (json.contains("created_at")) {
        _createdAt = QDateTime::fromString(json["created_at"].toString(), Qt::ISODate);
    }
    
    if (json.contains("updated_at")) {
        _updatedAt = QDateTime::fromString(json["updated_at"].toString(), Qt::ISODate);
    }
    
    if (json.contains("last_login")) {
        _lastLogin = QDateTime::fromString(json["last_login"].toString(), Qt::ISODate);
    }
}

QJsonObject User::toClientJson() const
{
    QJsonObject json;
    json["id"] = _id;
    json["username"] = _username;
    json["email"] = _email;
    json["display_name"] = _displayName;
    json["avatar_url"] = _avatarUrl;
    json["status"] = _status;
    json["theme"] = _theme;
    json["is_verified"] = _isVerified;
    
    if (_createdAt.isValid()) {
        json["created_at"] = _createdAt.toString(Qt::ISODate);
    }
    
    if (_lastLogin.isValid()) {
        json["last_login"] = _lastLogin.toString(Qt::ISODate);
    }
    
    return json;
}

QJsonObject User::toFullJson() const
{
    QJsonObject json = toClientJson();
    json["password_hash"] = _passwordHash;
    json["salt"] = _salt;
    json["is_active"] = _isActive;
    
    if (_updatedAt.isValid()) {
        json["updated_at"] = _updatedAt.toString(Qt::ISODate);
    }
    
    return json;
}

bool User::isValid() const
{
    return _id > 0 && !_username.isEmpty() && !_email.isEmpty();
}

void User::clear()
{
    _id = 0;
    _username.clear();
    _email.clear();
    _displayName.clear();
    _passwordHash.clear();
    _salt.clear();
    _avatarUrl.clear();
    _status = "offline";
    _theme = "light";
    _isVerified = false;
    _isActive = true;
    _createdAt = QDateTime();
    _updatedAt = QDateTime();
    _lastLogin = QDateTime();
}

void User::copyFrom(const User &other)
{
    _id = other._id;
    _username = other._username;
    _email = other._email;
    _displayName = other._displayName;
    _passwordHash = other._passwordHash;
    _salt = other._salt;
    _avatarUrl = other._avatarUrl;
    _status = other._status;
    _theme = other._theme;
    _isVerified = other._isVerified;
    _isActive = other._isActive;
    _createdAt = other._createdAt;
    _updatedAt = other._updatedAt;
    _lastLogin = other._lastLogin;
}

bool User::verifyPassword(const QString &password) const
{
    if (_passwordHash.isEmpty() || _salt.isEmpty()) {
        return false;
    }
    
    QString computedHash = hashPassword(password, _salt);
    return computedHash.compare(_passwordHash, Qt::CaseInsensitive) == 0;
}

void User::setPassword(const QString &password)
{
    _salt = generateSalt();
    _passwordHash = hashPassword(password, _salt);
}

void User::updateLastLogin()
{
    _lastLogin = QDateTime::currentDateTime();
}

QString User::getDisplayName() const
{
    return _displayName.isEmpty() ? _username : _displayName;
}

bool User::canLogin() const
{
    return _isActive && _isVerified && !_passwordHash.isEmpty();
}

QString User::generateSalt() const
{
    return Crypto::generateSalt(32);
}

QString User::hashPassword(const QString &password, const QString &salt) const
{
    return Crypto::hashPassword(password, salt);
}
