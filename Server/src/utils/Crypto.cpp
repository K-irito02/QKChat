#include "Crypto.h"
#include <QDateTime>


// 字符集常量定义
const QString Crypto::LOWERCASE_CHARS = "abcdefghijklmnopqrstuvwxyz";
const QString Crypto::UPPERCASE_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const QString Crypto::DIGIT_CHARS = "0123456789";
const QString Crypto::SYMBOL_CHARS = "!@#$%^&*()_+-=[]{}|;:,.<>?";

// 会话令牌密钥（实际项目中应该从配置文件读取）
const QString Crypto::SESSION_SECRET = "QKChat_Server_Secret_Key_2025";

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



QString Crypto::generateSessionToken(qint64 userId, qint64 timestamp)
{
    if (timestamp == 0) {
        timestamp = QDateTime::currentSecsSinceEpoch();
    }
    
    QString data = QString("%1:%2:%3").arg(userId).arg(timestamp).arg(SESSION_SECRET);
    QByteArray hash = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Sha256);
    
    QString token = QString("%1:%2:%3").arg(userId).arg(timestamp).arg(hash.toHex());
    return encodeBase64(token.toUtf8());
}

bool Crypto::verifySessionToken(const QString &token, qint64 userId, int maxAge)
{
    try {
        QByteArray decodedData = decodeBase64(token);
        QString decodedToken = QString::fromUtf8(decodedData);
        
        QStringList parts = decodedToken.split(':');
        if (parts.size() != 3) {
            return false;
        }
        
        qint64 tokenUserId = parts[0].toLongLong();
        qint64 tokenTimestamp = parts[1].toLongLong();
        QString tokenHash = parts[2];
        
        // 验证用户ID
        if (tokenUserId != userId) {
            return false;
        }
        
        // 验证时间戳
        qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();
        if (currentTimestamp - tokenTimestamp > maxAge) {
            return false;
        }
        
        // 验证哈希
        QString expectedData = QString("%1:%2:%3").arg(tokenUserId).arg(tokenTimestamp).arg(SESSION_SECRET);
        QByteArray expectedHash = QCryptographicHash::hash(expectedData.toUtf8(), QCryptographicHash::Sha256);
        
        return tokenHash.compare(expectedHash.toHex(), Qt::CaseInsensitive) == 0;
        
    } catch (...) {
        return false;
    }
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

QString Crypto::generateApiKey(int length)
{
    return generateRandomString(length, false);
}

QString Crypto::encryptData(const QString &data, const QString &key)
{
    // 简单的XOR加密（实际项目中应使用更强的加密算法）
    QByteArray dataBytes = data.toUtf8();
    QByteArray keyBytes = key.toUtf8();
    QByteArray result;
    
    for (int i = 0; i < dataBytes.size(); ++i) {
        char encrypted = dataBytes[i] ^ keyBytes[i % keyBytes.size()];
        result.append(encrypted);
    }
    
    return encodeBase64(result);
}

QString Crypto::decryptData(const QString &encryptedData, const QString &key)
{
    // 简单的XOR解密
    QByteArray dataBytes = decodeBase64(encryptedData);
    QByteArray keyBytes = key.toUtf8();
    QByteArray result;
    
    for (int i = 0; i < dataBytes.size(); ++i) {
        char decrypted = dataBytes[i] ^ keyBytes[i % keyBytes.size()];
        result.append(decrypted);
    }
    
    return QString::fromUtf8(result);
}
