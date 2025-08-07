#include "AuthResponse.h"
#include <QJsonDocument>
#include <QDebug>

AuthResponse::AuthResponse(QObject *parent)
    : QObject(parent)
    , _success(false)
    , _user(nullptr)
{
}

AuthResponse::AuthResponse(const QJsonObject &json, QObject *parent)
    : QObject(parent)
    , _success(false)
    , _user(nullptr)
{
    fromJson(json);
}

void AuthResponse::setSuccess(bool success)
{
    if (_success != success) {
        _success = success;
        emit successChanged();
    }
}

void AuthResponse::setMessage(const QString &message)
{
    if (_message != message) {
        _message = message;
        emit messageChanged();
    }
}

void AuthResponse::setErrorCode(const QString &errorCode)
{
    if (_errorCode != errorCode) {
        _errorCode = errorCode;
        emit errorCodeChanged();
    }
}

void AuthResponse::setUser(User* user)
{
    if (_user != user) {
        if (_user && _user->parent() == this) {
            _user->deleteLater();
        }
        _user = user;
        if (_user && _user->parent() != this) {
            _user->setParent(this);
        }
        emit userChanged();
    }
}

void AuthResponse::setSessionToken(const QString &sessionToken)
{
    if (_sessionToken != sessionToken) {
        _sessionToken = sessionToken;
        emit sessionTokenChanged();
    }
}

void AuthResponse::fromJson(const QJsonObject &json)
{
    setSuccess(json["success"].toBool());
    setMessage(json["message"].toString());
    setErrorCode(json["error_code"].toString());
    setSessionToken(json["session_token"].toString());
    
    if (json.contains("user") && json["user"].isObject()) {
        User* newUser = new User(json["user"].toObject(), this);
        setUser(newUser);
    }
}

QJsonObject AuthResponse::toJson() const
{
    QJsonObject json;
    json["success"] = _success;
    json["message"] = _message;
    json["error_code"] = _errorCode;
    json["session_token"] = _sessionToken;
    
    if (_user) {
        json["user"] = _user->toJson();
    }
    
    return json;
}

bool AuthResponse::isValid() const
{
    return !_message.isEmpty();
}

void AuthResponse::clear()
{
    setSuccess(false);
    setMessage("");
    setErrorCode("");
    setSessionToken("");
    setUser(nullptr);
}

void AuthResponse::setError(const QString &errorCode, const QString &message)
{
    setSuccess(false);
    setErrorCode(errorCode);
    setMessage(message);
    setSessionToken("");
    setUser(nullptr);
}

void AuthResponse::setSuccess(const QString &message, User* user, const QString &sessionToken)
{
    setSuccess(true);
    setMessage(message);
    setErrorCode("");
    setSessionToken(sessionToken);
    setUser(user);
}
