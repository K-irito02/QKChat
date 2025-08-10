#ifndef CRYPTO_H
#define CRYPTO_H

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>

/**
 * @brief 加密工具类（服务器端）
 * 
 * 提供密码哈希、盐值生成、数据加密等安全相关的工具方法。
 * 使用标准的加密算法确保数据安全性。
 */
class Crypto
{
public:
    /**
     * @brief 生成随机盐值
     * @param length 盐值长度，默认32字符
     * @return 随机盐值字符串
     */
    static QString generateSalt(int length = 32);
    
    /**
     * @brief 使用SHA-256算法对密码进行哈希
     * @param password 原始密码
     * @param salt 盐值
     * @return 哈希后的密码
     */
    static QString hashPassword(const QString &password, const QString &salt);
    
    /**
     * @brief 验证密码是否正确
     * @param password 原始密码
     * @param salt 盐值
     * @param hash 存储的哈希值
     * @return 密码是否正确
     */
    static bool verifyPassword(const QString &password, const QString &salt, const QString &hash);
    
    /**
     * @brief 生成随机字符串
     * @param length 字符串长度
     * @param includeSymbols 是否包含特殊符号
     * @return 随机字符串
     */
    static QString generateRandomString(int length, bool includeSymbols = false);
    

    
    /**
     * @brief 生成会话令牌
     * @param userId 用户ID
     * @param timestamp 时间戳
     * @return 会话令牌
     */
    static QString generateSessionToken(qint64 userId, qint64 timestamp = 0);
    
    /**
     * @brief 验证会话令牌
     * @param token 会话令牌
     * @param userId 用户ID
     * @param maxAge 最大有效期（秒）
     * @return 令牌是否有效
     */
    static bool verifySessionToken(const QString &token, qint64 userId, int maxAge = 3600);
    
    /**
     * @brief 使用MD5算法计算数据哈希
     * @param data 输入数据
     * @return MD5哈希值
     */
    static QString md5Hash(const QByteArray &data);
    
    /**
     * @brief 使用SHA-256算法计算数据哈希
     * @param data 输入数据
     * @return SHA-256哈希值
     */
    static QString sha256Hash(const QByteArray &data);
    
    /**
     * @brief 简单的字符串编码（Base64）
     * @param data 原始数据
     * @return 编码后的字符串
     */
    static QString encodeBase64(const QByteArray &data);
    
    /**
     * @brief 简单的字符串解码（Base64）
     * @param encoded 编码后的字符串
     * @return 解码后的数据
     */
    static QByteArray decodeBase64(const QString &encoded);
    
    /**
     * @brief 生成API密钥
     * @param length 密钥长度
     * @return API密钥
     */
    static QString generateApiKey(int length = 64);
    
    /**
     * @brief 加密敏感数据
     * @param data 原始数据
     * @param key 加密密钥
     * @return 加密后的数据
     */
    static QString encryptData(const QString &data, const QString &key);
    
    /**
     * @brief 解密敏感数据
     * @param encryptedData 加密后的数据
     * @param key 解密密钥
     * @return 原始数据
     */
    static QString decryptData(const QString &encryptedData, const QString &key);

private:
    // 私有构造函数，防止实例化
    Crypto() = default;
    
    // 字符集常量
    static const QString LOWERCASE_CHARS;
    static const QString UPPERCASE_CHARS;
    static const QString DIGIT_CHARS;
    static const QString SYMBOL_CHARS;
    
    // 会话令牌密钥
    static const QString SESSION_SECRET;
};

#endif // CRYPTO_H
