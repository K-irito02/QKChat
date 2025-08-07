#include "Crypto.h"
#include <QRegularExpression>
#include <QDebug>

// 字符集常量定义
const QString Crypto::LOWERCASE_CHARS = "abcdefghijklmnopqrstuvwxyz";
const QString Crypto::UPPERCASE_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const QString Crypto::DIGIT_CHARS = "0123456789";
const QString Crypto::SYMBOL_CHARS = "!@#$%^&*()_+-=[]{}|;:,.<>?";

QString Crypto::generateSalt(int length)
{
    const QString chars = LOWERCASE_CHARS + UPPERCASE_CHARS + DIGIT_CHARS;
    QString salt;
    salt.reserve(length);
    
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        salt.append(chars.at(index));
    }
    
    return salt;
}

QString Crypto::hashPassword(const QString &password, const QString &salt)
{
    QByteArray data = (password + salt).toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

bool Crypto::verifyPassword(const QString &password, const QString &salt, const QString &hash)
{
    QString computedHash = hashPassword(password, salt);
    return computedHash.compare(hash, Qt::CaseInsensitive) == 0;
}

QString Crypto::generateRandomString(int length, bool includeSymbols)
{
    QString chars = LOWERCASE_CHARS + UPPERCASE_CHARS + DIGIT_CHARS;
    if (includeSymbols) {
        chars += SYMBOL_CHARS;
    }
    
    QString result;
    result.reserve(length);
    
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        result.append(chars.at(index));
    }
    
    return result;
}

QString Crypto::generateVerificationCode(int length)
{
    QString code;
    code.reserve(length);
    
    for (int i = 0; i < length; ++i) {
        int digit = QRandomGenerator::global()->bounded(10);
        code.append(QString::number(digit));
    }
    
    return code;
}

QString Crypto::md5Hash(const QByteArray &data)
{
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    return hash.toHex();
}

QString Crypto::sha256Hash(const QByteArray &data)
{
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return hash.toHex();
}

QString Crypto::encodeBase64(const QByteArray &data)
{
    return data.toBase64();
}

QByteArray Crypto::decodeBase64(const QString &encoded)
{
    return QByteArray::fromBase64(encoded.toUtf8());
}

int Crypto::checkPasswordStrength(const QString &password)
{
    if (password.length() < 6) {
        return 0; // 很弱
    }
    
    int score = 0;
    
    // 长度检查
    if (password.length() >= 8) score++;
    if (password.length() >= 12) score++;
    
    // 字符类型检查
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSymbol = false;
    
    for (const QChar &ch : password) {
        if (ch.isLower()) hasLower = true;
        else if (ch.isUpper()) hasUpper = true;
        else if (ch.isDigit()) hasDigit = true;
        else hasSymbol = true;
    }
    
    if (hasLower) score++;
    if (hasUpper) score++;
    if (hasDigit) score++;
    if (hasSymbol) score++;
    
    // 复杂度检查
    QRegularExpression patterns[] = {
        QRegularExpression("[a-z]"),     // 小写字母
        QRegularExpression("[A-Z]"),     // 大写字母
        QRegularExpression("[0-9]"),     // 数字
        QRegularExpression("[^a-zA-Z0-9]") // 特殊字符
    };
    
    int patternCount = 0;
    for (const auto &pattern : patterns) {
        if (pattern.match(password).hasMatch()) {
            patternCount++;
        }
    }
    
    // 根据总分计算强度等级
    if (score <= 2) return 1; // 弱
    if (score <= 4) return 2; // 中等
    if (score <= 6) return 3; // 强
    return 4; // 很强
}

QString Crypto::getPasswordStrengthDescription(int strength)
{
    switch (strength) {
        case 0: return "很弱";
        case 1: return "弱";
        case 2: return "中等";
        case 3: return "强";
        case 4: return "很强";
        default: return "未知";
    }
}
