#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <QString>
#include <QRegularExpression>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

/**
 * @brief 数据验证工具类（服务器端）
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
     * @brief 验证JSON格式
     * @param json JSON字符串
     * @return 是否有效
     */
    static bool isValidJson(const QString &json);
    
    /**
     * @brief 验证会话令牌格式
     * @param token 会话令牌
     * @return 是否有效
     */
    static bool isValidSessionToken(const QString &token);
    
    /**
     * @brief 清理和标准化输入文本
     * @param text 输入文本
     * @return 清理后的文本
     */
    static QString sanitizeInput(const QString &text);
    
    /**
     * @brief 检查是否包含SQL注入风险
     * @param text 输入文本
     * @return 是否有风险
     */
    static bool hasSqlInjectionRisk(const QString &text);
    
    /**
     * @brief 检查是否包含XSS风险
     * @param text 输入文本
     * @return 是否有风险
     */
    static bool hasXssRisk(const QString &text);

    /**
     * @brief 检查IP地址是否在黑名单中
     * @param ipAddress IP地址
     * @return 是否在黑名单中
     */
    static bool isIPBlacklisted(const QString &ipAddress);

    /**
     * @brief 添加IP到黑名单
     * @param ipAddress IP地址
     * @param reason 原因
     * @param duration 持续时间（秒），0表示永久
     */
    static void addToIPBlacklist(const QString &ipAddress, const QString &reason = "", int duration = 0);

    /**
     * @brief 从黑名单移除IP
     * @param ipAddress IP地址
     */
    static void removeFromIPBlacklist(const QString &ipAddress);

    /**
     * @brief 检查请求频率是否超限
     * @param identifier 标识符（IP地址或用户ID）
     * @param maxRequests 最大请求数
     * @param timeWindow 时间窗口（秒）
     * @return 是否超限
     */
    static bool isRateLimited(const QString &identifier, int maxRequests = 60, int timeWindow = 60);

    /**
     * @brief 记录请求
     * @param identifier 标识符
     */
    static void recordRequest(const QString &identifier);

    /**
     * @brief 验证CSRF令牌
     * @param token 提交的令牌
     * @param sessionToken 会话令牌
     * @return 验证是否成功
     */
    static bool validateCSRFToken(const QString &token, const QString &sessionToken);

    /**
     * @brief 生成CSRF令牌
     * @param sessionToken 会话令牌
     * @return CSRF令牌
     */
    static QString generateCSRFToken(const QString &sessionToken);

    /**
     * @brief 验证API权限
     * @param userId 用户ID
     * @param apiEndpoint API端点
     * @param method HTTP方法
     * @return 是否有权限
     */
    static bool hasAPIPermission(qint64 userId, const QString &apiEndpoint, const QString &method);

    /**
     * @brief 验证二次认证码
     * @param userId 用户ID
     * @param code 认证码
     * @param operation 操作类型
     * @return 验证是否成功
     */
    static bool validateTwoFactorCode(qint64 userId, const QString &code, const QString &operation);

    /**
     * @brief 生成二次认证码
     * @param userId 用户ID
     * @param operation 操作类型
     * @return 认证码
     */
    static QString generateTwoFactorCode(qint64 userId, const QString &operation);

    /**
     * @brief 记录安全事件
     * @param eventType 事件类型
     * @param severity 严重程度
     * @param description 描述
     * @param ipAddress IP地址
     * @param userId 用户ID（可选）
     */
    static void logSecurityEvent(const QString &eventType, const QString &severity,
                                const QString &description, const QString &ipAddress, qint64 userId = -1);

    /**
     * @brief 检测异常行为
     * @param userId 用户ID
     * @param ipAddress IP地址
     * @param action 操作
     * @return 是否异常
     */
    static bool detectAnomalousActivity(qint64 userId, const QString &ipAddress, const QString &action);

    /**
     * @brief 验证文件上传安全性
     * @param fileName 文件名
     * @param fileContent 文件内容
     * @param maxSize 最大大小
     * @return 验证结果和错误信息
     */
    static QPair<bool, QString> validateFileUpload(const QString &fileName, const QByteArray &fileContent, qint64 maxSize);

    /**
     * @brief 清理HTML内容
     * @param html HTML内容
     * @param allowedTags 允许的标签列表
     * @return 清理后的HTML
     */
    static QString sanitizeHTML(const QString &html, const QStringList &allowedTags = QStringList());

    /**
     * @brief 验证密码强度
     * @param password 密码
     * @param minLength 最小长度
     * @param requireSpecial 是否需要特殊字符
     * @param requireNumbers 是否需要数字
     * @param requireUppercase 是否需要大写字母
     * @return 验证结果和强度分数
     */
    static QPair<bool, int> validatePasswordStrength(const QString &password, int minLength = 8,
                                                     bool requireSpecial = true, bool requireNumbers = true,
                                                     bool requireUppercase = true);

    /**
     * @brief 清理过期的安全记录
     */
    static void cleanupSecurityRecords();

private:
    // 私有构造函数，防止实例化
    Validator() = default;

    /**
     * @brief IP黑名单条目结构
     */
    struct BlacklistEntry {
        QString reason;
        QDateTime addedTime;
        QDateTime expiryTime;
        int violations;
    };

    /**
     * @brief 请求记录结构
     */
    struct RequestRecord {
        QList<QDateTime> timestamps;
        int totalRequests;
    };

    /**
     * @brief 用户行为记录结构
     */
    struct UserActivity {
        QList<QString> recentActions;
        QMap<QString, int> actionCounts;
        QDateTime lastActivity;
        QStringList recentIPs;
    };

    // 正则表达式常量
    static const QRegularExpression EMAIL_REGEX;
    static const QRegularExpression USERNAME_REGEX;
    static const QRegularExpression PHONE_REGEX;
    static const QRegularExpression IP_REGEX;
    static const QRegularExpression URL_REGEX;
    static const QRegularExpression SQL_INJECTION_REGEX;
    static const QRegularExpression XSS_REGEX;
    static const QRegularExpression HTML_TAG_REGEX;
    static const QRegularExpression SCRIPT_REGEX;

    // 静态数据存储
    static QMap<QString, BlacklistEntry> s_ipBlacklist;
    static QMap<QString, RequestRecord> s_requestRecords;
    static QMap<qint64, UserActivity> s_userActivities;
    static QMap<QString, QDateTime> s_csrfTokens;
    static QMap<QString, QDateTime> s_twoFactorCodes;
    static QMutex s_securityMutex;

    /**
     * @brief 检查文件类型是否安全
     * @param fileName 文件名
     * @return 是否安全
     */
    static bool isSafeFileType(const QString &fileName);

    /**
     * @brief 检查文件内容是否包含恶意代码
     * @param content 文件内容
     * @return 是否包含恶意代码
     */
    static bool containsMaliciousContent(const QByteArray &content);

    /**
     * @brief 计算密码强度分数
     * @param password 密码
     * @return 强度分数（0-100）
     */
    static int calculatePasswordScore(const QString &password);

    /**
     * @brief 清理过期的请求记录
     */
    static void cleanupRequestRecords();

    /**
     * @brief 清理过期的黑名单条目
     */
    static void cleanupBlacklist();

    /**
     * @brief 清理过期的令牌
     */
    static void cleanupTokens();
};

#endif // VALIDATOR_H
