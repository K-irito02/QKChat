#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <QString>
#include <QRegularExpression>

/**
 * @brief 数据验证工具类
 * 
 * 提供各种数据格式验证功能，包括邮箱、用户名、密码等的格式检查。
 * 确保用户输入数据的有效性和安全性。
 */
class Validator
{
public:
    /**
     * @brief 验证邮箱地址格式
     * @param email 邮箱地址
     * @return 是否有效
     */
    static bool isValidEmail(const QString &email);
    
    /**
     * @brief 验证用户名格式
     * @param username 用户名
     * @return 是否有效
     */
    static bool isValidUsername(const QString &username);
    
    /**
     * @brief 验证密码格式
     * @param password 密码
     * @return 是否有效
     */
    static bool isValidPassword(const QString &password);
    
    /**
     * @brief 验证验证码格式
     * @param code 验证码
     * @param length 期望长度，默认6位
     * @return 是否有效
     */
    static bool isValidVerificationCode(const QString &code, int length = 6);
    
    /**
     * @brief 验证手机号码格式（中国大陆）
     * @param phone 手机号码
     * @return 是否有效
     */
    static bool isValidPhoneNumber(const QString &phone);
    
    /**
     * @brief 验证IP地址格式
     * @param ip IP地址
     * @return 是否有效
     */
    static bool isValidIPAddress(const QString &ip);
    
    /**
     * @brief 验证端口号
     * @param port 端口号
     * @return 是否有效
     */
    static bool isValidPort(int port);
    
    /**
     * @brief 验证URL格式
     * @param url URL地址
     * @return 是否有效
     */
    static bool isValidUrl(const QString &url);
    
    /**
     * @brief 检查字符串长度是否在范围内
     * @param text 文本
     * @param minLength 最小长度
     * @param maxLength 最大长度
     * @return 是否在范围内
     */
    static bool isLengthInRange(const QString &text, int minLength, int maxLength);
    
    /**
     * @brief 检查字符串是否只包含字母数字
     * @param text 文本
     * @return 是否只包含字母数字
     */
    static bool isAlphanumeric(const QString &text);
    
    /**
     * @brief 检查字符串是否只包含数字
     * @param text 文本
     * @return 是否只包含数字
     */
    static bool isNumeric(const QString &text);
    
    /**
     * @brief 检查字符串是否包含特殊字符
     * @param text 文本
     * @return 是否包含特殊字符
     */
    static bool containsSpecialChars(const QString &text);
    
    /**
     * @brief 获取邮箱验证错误信息
     * @param email 邮箱地址
     * @return 错误信息，空字符串表示有效
     */
    static QString getEmailValidationError(const QString &email);
    
    /**
     * @brief 获取用户名验证错误信息
     * @param username 用户名
     * @return 错误信息，空字符串表示有效
     */
    static QString getUsernameValidationError(const QString &username);
    
    /**
     * @brief 获取密码验证错误信息
     * @param password 密码
     * @return 错误信息，空字符串表示有效
     */
    static QString getPasswordValidationError(const QString &password);
    
    /**
     * @brief 清理和标准化输入文本
     * @param text 输入文本
     * @return 清理后的文本
     */
    static QString sanitizeInput(const QString &text);

private:
    // 私有构造函数，防止实例化
    Validator() = default;
    
    // 正则表达式常量
    static const QRegularExpression EMAIL_REGEX;
    static const QRegularExpression USERNAME_REGEX;
    static const QRegularExpression PHONE_REGEX;
    static const QRegularExpression IP_REGEX;
    static const QRegularExpression URL_REGEX;
};

#endif // VALIDATOR_H
