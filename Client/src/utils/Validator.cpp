#include "Validator.h"


// 正则表达式常量定义
const QRegularExpression Validator::EMAIL_REGEX(
    R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
);

const QRegularExpression Validator::USERNAME_REGEX(
    R"(^[a-zA-Z0-9_]{3,20}$)"
);

const QRegularExpression Validator::PHONE_REGEX(
    R"(^1[3-9]\d{9}$)"
);

const QRegularExpression Validator::IP_REGEX(
    R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
);

const QRegularExpression Validator::URL_REGEX(
    R"(^https?://[^\s/$.?#].[^\s]*$)"
);

bool Validator::isValidEmail(const QString &email)
{
    if (email.trimmed().isEmpty()) {
        return false;
    }
    
    return EMAIL_REGEX.match(email.trimmed()).hasMatch();
}

bool Validator::isValidUsername(const QString &username)
{
    if (username.trimmed().isEmpty()) {
        return false;
    }
    
    QString trimmed = username.trimmed();
    
    // 检查长度
    if (trimmed.length() < 3 || trimmed.length() > 20) {
        return false;
    }
    
    // 检查格式（字母、数字、下划线）
    return USERNAME_REGEX.match(trimmed).hasMatch();
}

bool Validator::isValidPassword(const QString &password)
{
    if (password.isEmpty()) {
        return false;
    }
    
    // 最少6位
    if (password.length() < 6) {
        return false;
    }
    
    // 最多50位
    if (password.length() > 50) {
        return false;
    }
    
    return true;
}

bool Validator::isValidVerificationCode(const QString &code, int length)
{
    if (code.trimmed().isEmpty()) {
        return false;
    }
    
    QString trimmed = code.trimmed();
    
    // 检查长度
    if (trimmed.length() != length) {
        return false;
    }
    
    // 检查是否全为数字（使用正则表达式确保严格匹配）
    QRegularExpression numericRegex("^[0-9]+$");
    return numericRegex.match(trimmed).hasMatch();
}

bool Validator::isValidPhoneNumber(const QString &phone)
{
    if (phone.trimmed().isEmpty()) {
        return false;
    }
    
    return PHONE_REGEX.match(phone.trimmed()).hasMatch();
}

bool Validator::isValidIPAddress(const QString &ip)
{
    if (ip.trimmed().isEmpty()) {
        return false;
    }
    
    return IP_REGEX.match(ip.trimmed()).hasMatch();
}

bool Validator::isValidPort(int port)
{
    return port > 0 && port <= 65535;
}

bool Validator::isValidUrl(const QString &url)
{
    if (url.trimmed().isEmpty()) {
        return false;
    }
    
    return URL_REGEX.match(url.trimmed()).hasMatch();
}

bool Validator::isLengthInRange(const QString &text, int minLength, int maxLength)
{
    int length = text.length();
    return length >= minLength && length <= maxLength;
}

bool Validator::isAlphanumeric(const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }
    
    for (const QChar &ch : text) {
        if (!ch.isLetterOrNumber()) {
            return false;
        }
    }
    
    return true;
}

bool Validator::isNumeric(const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }
    
    for (const QChar &ch : text) {
        if (!ch.isDigit()) {
            return false;
        }
    }
    
    return true;
}

bool Validator::containsSpecialChars(const QString &text)
{
    QRegularExpression specialChars(R"([^a-zA-Z0-9])");
    return specialChars.match(text).hasMatch();
}

QString Validator::getEmailValidationError(const QString &email)
{
    if (email.trimmed().isEmpty()) {
        return "邮箱地址不能为空";
    }
    
    if (!isValidEmail(email)) {
        return "请输入有效的邮箱地址";
    }
    
    return "";
}

QString Validator::getUsernameValidationError(const QString &username)
{
    if (username.trimmed().isEmpty()) {
        return "用户名不能为空";
    }
    
    QString trimmed = username.trimmed();
    
    if (trimmed.length() < 3) {
        return "用户名至少3个字符";
    }
    
    if (trimmed.length() > 20) {
        return "用户名最多20个字符";
    }
    
    if (!USERNAME_REGEX.match(trimmed).hasMatch()) {
        return "用户名只能包含字母、数字和下划线";
    }
    
    return "";
}

QString Validator::getPasswordValidationError(const QString &password)
{
    if (password.isEmpty()) {
        return "密码不能为空";
    }
    
    if (password.length() < 6) {
        return "密码至少6个字符";
    }
    
    if (password.length() > 50) {
        return "密码最多50个字符";
    }
    
    return "";
}

QString Validator::sanitizeInput(const QString &text)
{
    QString sanitized = text.trimmed();
    
    // 移除潜在的危险字符
    sanitized.remove(QRegularExpression(R"([<>\"'&])"));
    
    return sanitized;
}
