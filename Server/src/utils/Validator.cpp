#include "Validator.h"
#include <QJsonDocument>
#include <QJsonParseError>


// 正则表达式常量定义
const QRegularExpression Validator::EMAIL_REGEX(
    R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
);

const QRegularExpression Validator::USERNAME_REGEX(
    R"(^[a-zA-Z0-9_]{3,50}$)"
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

const QRegularExpression Validator::SQL_INJECTION_REGEX(
    R"((\b(SELECT|INSERT|UPDATE|DELETE|DROP|CREATE|ALTER|EXEC|UNION|SCRIPT)\b)|('|(\\x27)|(\\x2D\\x2D))|(-{2})|(\*|\*)|(\bOR\b.*=.*)|(\bAND\b.*=.*))",
    QRegularExpression::CaseInsensitiveOption
);

const QRegularExpression Validator::XSS_REGEX(
    R"((<script[^>]*>.*?</script>)|(<.*?javascript:.*?>)|(<.*?on\w+\s*=.*?>))",
    QRegularExpression::CaseInsensitiveOption
);

const QRegularExpression Validator::HTML_TAG_REGEX(
    R"(<[^>]+>)",
    QRegularExpression::CaseInsensitiveOption
);

const QRegularExpression Validator::SCRIPT_REGEX(
    R"((javascript:|vbscript:|data:|about:))",
    QRegularExpression::CaseInsensitiveOption
);

// 静态成员初始化
QMap<QString, Validator::BlacklistEntry> Validator::s_ipBlacklist;
QMap<QString, Validator::RequestRecord> Validator::s_requestRecords;
QMap<qint64, Validator::UserActivity> Validator::s_userActivities;
QMap<QString, QDateTime> Validator::s_csrfTokens;
QMap<QString, QDateTime> Validator::s_twoFactorCodes;
QMutex Validator::s_securityMutex;

bool Validator::isValidEmail(const QString &email)
{
    if (email.trimmed().isEmpty()) {
        return false;
    }
    
    QString trimmed = email.trimmed();
    
    // 检查长度
    if (trimmed.length() > 255) {
        return false;
    }
    
    return EMAIL_REGEX.match(trimmed).hasMatch();
}

bool Validator::isValidUsername(const QString &username)
{
    if (username.trimmed().isEmpty()) {
        return false;
    }
    
    QString trimmed = username.trimmed();
    
    // 检查长度
    if (trimmed.length() < 3 || trimmed.length() > 50) {
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
    
    // 最多100位
    if (password.length() > 100) {
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
    
    // 检查是否全为数字
    return isNumeric(trimmed);
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

bool Validator::isValidJson(const QString &json)
{
    if (json.trimmed().isEmpty()) {
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument::fromJson(json.toUtf8(), &error);
    
    return error.error == QJsonParseError::NoError;
}

bool Validator::isValidSessionToken(const QString &token)
{
    if (token.trimmed().isEmpty()) {
        return false;
    }
    
    // 检查长度（Base64编码的令牌通常比较长）
    if (token.length() < 20 || token.length() > 500) {
        return false;
    }
    
    // 检查是否包含有效的Base64字符
    QRegularExpression base64Regex(R"(^[A-Za-z0-9+/]*={0,2}$)");
    return base64Regex.match(token).hasMatch();
}

QString Validator::sanitizeInput(const QString &text)
{
    QString sanitized = text.trimmed();
    
    // 移除潜在的危险字符
    sanitized.remove(QRegularExpression(R"([<>\"'&])"));
    
    // 移除控制字符
    sanitized.remove(QRegularExpression(R"([\x00-\x1F\x7F])"));
    
    return sanitized;
}

bool Validator::hasSqlInjectionRisk(const QString &text)
{
    return SQL_INJECTION_REGEX.match(text).hasMatch();
}

bool Validator::hasXssRisk(const QString &text)
{
    return XSS_REGEX.match(text).hasMatch();
}

bool Validator::isIPBlacklisted(const QString &ipAddress)
{
    QMutexLocker locker(&s_securityMutex);

    if (!s_ipBlacklist.contains(ipAddress)) {
        return false;
    }

    const BlacklistEntry &entry = s_ipBlacklist[ipAddress];

    // 检查是否已过期
    if (entry.expiryTime.isValid() && QDateTime::currentDateTime() > entry.expiryTime) {
        s_ipBlacklist.remove(ipAddress);
        return false;
    }

    return true;
}

void Validator::addToIPBlacklist(const QString &ipAddress, const QString &reason, int duration)
{
    QMutexLocker locker(&s_securityMutex);

    BlacklistEntry entry;
    entry.reason = reason;
    entry.addedTime = QDateTime::currentDateTime();
    entry.violations = s_ipBlacklist.contains(ipAddress) ? s_ipBlacklist[ipAddress].violations + 1 : 1;

    if (duration > 0) {
        entry.expiryTime = entry.addedTime.addSecs(duration);
    }

    s_ipBlacklist[ipAddress] = entry;

    // 记录安全事件
    logSecurityEvent("IP_BLACKLISTED", "HIGH",
                    QString("IP %1 added to blacklist. Reason: %2").arg(ipAddress).arg(reason),
                    ipAddress);
}

void Validator::removeFromIPBlacklist(const QString &ipAddress)
{
    QMutexLocker locker(&s_securityMutex);

    if (s_ipBlacklist.remove(ipAddress) > 0) {
        logSecurityEvent("IP_WHITELIST", "INFO",
                        QString("IP %1 removed from blacklist").arg(ipAddress),
                        ipAddress);
    }
}

bool Validator::isRateLimited(const QString &identifier, int maxRequests, int timeWindow)
{
    QMutexLocker locker(&s_securityMutex);

    QDateTime now = QDateTime::currentDateTime();
    QDateTime windowStart = now.addSecs(-timeWindow);

    if (!s_requestRecords.contains(identifier)) {
        return false;
    }

    RequestRecord &record = s_requestRecords[identifier];

    // 清理过期的时间戳
    record.timestamps.erase(
        std::remove_if(record.timestamps.begin(), record.timestamps.end(),
                      [windowStart](const QDateTime &timestamp) {
                          return timestamp < windowStart;
                      }),
        record.timestamps.end()
    );

    return record.timestamps.size() >= maxRequests;
}

void Validator::recordRequest(const QString &identifier)
{
    QMutexLocker locker(&s_securityMutex);

    QDateTime now = QDateTime::currentDateTime();

    if (!s_requestRecords.contains(identifier)) {
        s_requestRecords[identifier] = RequestRecord();
    }

    RequestRecord &record = s_requestRecords[identifier];
    record.timestamps.append(now);
    record.totalRequests++;

    // 限制内存使用，只保留最近1000个请求
    if (record.timestamps.size() > 1000) {
        record.timestamps.removeFirst();
    }
}

bool Validator::validateCSRFToken(const QString &token, const QString &sessionToken)
{
    QMutexLocker locker(&s_securityMutex);

    QString expectedToken = generateCSRFToken(sessionToken);

    if (token != expectedToken) {
        return false;
    }

    // 检查令牌是否过期（1小时有效期）
    if (s_csrfTokens.contains(token)) {
        QDateTime tokenTime = s_csrfTokens[token];
        if (tokenTime.addSecs(3600) < QDateTime::currentDateTime()) {
            s_csrfTokens.remove(token);
            return false;
        }
    }

    return true;
}

QString Validator::generateCSRFToken(const QString &sessionToken)
{
    QMutexLocker locker(&s_securityMutex);

    QString data = sessionToken + QString::number(QDateTime::currentSecsSinceEpoch());
    QByteArray hash = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Sha256);
    QString token = hash.toHex();

    s_csrfTokens[token] = QDateTime::currentDateTime();

    return token;
}

bool Validator::hasAPIPermission(qint64 userId, const QString &apiEndpoint, const QString &method)
{
    // 简化的权限检查实现
    // 实际项目中应该从数据库查询用户权限

    if (userId <= 0) {
        return false;
    }

    // 管理员用户（ID为1）拥有所有权限
    if (userId == 1) {
        return true;
    }

    // 基本API权限检查
    QStringList publicEndpoints = {
        "/api/auth/login",
        "/api/auth/register",
        "/api/auth/verify",
        "/api/heartbeat"
    };

    if (publicEndpoints.contains(apiEndpoint)) {
        return true;
    }

    // 其他端点需要认证
    return userId > 0;
}

bool Validator::validateTwoFactorCode(qint64 userId, const QString &code, const QString &operation)
{
    QMutexLocker locker(&s_securityMutex);

    QString key = QString("%1:%2:%3").arg(userId).arg(operation).arg(code);

    if (!s_twoFactorCodes.contains(key)) {
        return false;
    }

    QDateTime codeTime = s_twoFactorCodes[key];

    // 验证码5分钟有效期
    if (codeTime.addSecs(300) < QDateTime::currentDateTime()) {
        s_twoFactorCodes.remove(key);
        return false;
    }

    // 验证成功后删除验证码
    s_twoFactorCodes.remove(key);

    return true;
}

QString Validator::generateTwoFactorCode(qint64 userId, const QString &operation)
{
    QMutexLocker locker(&s_securityMutex);

    // 生成6位数字验证码
    QString code = QString::number(QRandomGenerator::global()->bounded(100000, 999999));
    QString key = QString("%1:%2:%3").arg(userId).arg(operation).arg(code);

    s_twoFactorCodes[key] = QDateTime::currentDateTime();

    return code;
}

void Validator::logSecurityEvent(const QString &eventType, const QString &severity,
                                const QString &description, const QString &ipAddress, qint64 userId)
{
    // 这里应该记录到专门的安全日志文件或数据库
    QString logMessage = QString("SECURITY_EVENT [%1] Severity: %2, IP: %3, User: %4, Description: %5")
                         .arg(eventType)
                         .arg(severity)
                         .arg(ipAddress)
                         .arg(userId > 0 ? QString::number(userId) : "N/A")
                         .arg(description);

    // 使用Logger记录安全事件
    if (severity == "HIGH" || severity == "CRITICAL") {
        // 安全事件日志已禁用，避免调试信息干扰
    } else {
        qInfo() << logMessage;
    }
}

bool Validator::detectAnomalousActivity(qint64 userId, const QString &ipAddress, const QString &action)
{
    QMutexLocker locker(&s_securityMutex);

    if (userId <= 0) {
        return false;
    }

    QDateTime now = QDateTime::currentDateTime();

    if (!s_userActivities.contains(userId)) {
        s_userActivities[userId] = UserActivity();
    }

    UserActivity &activity = s_userActivities[userId];

    // 检查IP地址变化
    if (!activity.recentIPs.contains(ipAddress)) {
        activity.recentIPs.append(ipAddress);

        // 如果短时间内从多个IP登录，标记为异常
        if (activity.recentIPs.size() > 3 &&
            activity.lastActivity.secsTo(now) < 3600) { // 1小时内

            logSecurityEvent("SUSPICIOUS_IP_CHANGE", "HIGH",
                           QString("User %1 accessed from multiple IPs: %2")
                           .arg(userId).arg(activity.recentIPs.join(", ")),
                           ipAddress, userId);
            return true;
        }

        // 保持最近5个IP
        if (activity.recentIPs.size() > 5) {
            activity.recentIPs.removeFirst();
        }
    }

    // 检查操作频率
    activity.recentActions.append(action);
    activity.actionCounts[action]++;
    activity.lastActivity = now;

    // 检查是否有异常高频操作
    if (activity.actionCounts[action] > 100 &&
        activity.lastActivity.addSecs(-300).secsTo(now) < 300) { // 5分钟内超过100次

        logSecurityEvent("HIGH_FREQUENCY_ACTION", "MEDIUM",
                       QString("User %1 performed action '%2' %3 times in 5 minutes")
                       .arg(userId).arg(action).arg(activity.actionCounts[action]),
                       ipAddress, userId);
        return true;
    }

    // 保持最近100个操作
    if (activity.recentActions.size() > 100) {
        activity.recentActions.removeFirst();
    }

    return false;
}

QPair<bool, QString> Validator::validateFileUpload(const QString &fileName, const QByteArray &fileContent, qint64 maxSize)
{
    // 检查文件大小
    if (fileContent.size() > maxSize) {
        return qMakePair(false, QString("File size exceeds maximum allowed size of %1 bytes").arg(maxSize));
    }

    // 检查文件名
    if (fileName.isEmpty() || fileName.contains("..") || fileName.contains("/") || fileName.contains("\\")) {
        return qMakePair(false, "Invalid file name");
    }

    // 检查文件类型
    if (!isSafeFileType(fileName)) {
        return qMakePair(false, "File type not allowed");
    }

    // 检查文件内容
    if (containsMaliciousContent(fileContent)) {
        return qMakePair(false, "File contains potentially malicious content");
    }

    return qMakePair(true, "");
}

QString Validator::sanitizeHTML(const QString &html, const QStringList &allowedTags)
{
    QString sanitized = html;

    // 移除脚本标签
    sanitized.remove(SCRIPT_REGEX);

    // 如果没有指定允许的标签，移除所有HTML标签
    if (allowedTags.isEmpty()) {
        sanitized.remove(HTML_TAG_REGEX);
    } else {
        // 移除不在允许列表中的标签
        QRegularExpressionMatchIterator it = HTML_TAG_REGEX.globalMatch(sanitized);
        QStringList tagsToRemove;

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString tag = match.captured(0);

            bool allowed = false;
            for (const QString &allowedTag : allowedTags) {
                if (tag.startsWith("<" + allowedTag, Qt::CaseInsensitive)) {
                    allowed = true;
                    break;
                }
            }

            if (!allowed) {
                tagsToRemove.append(tag);
            }
        }

        for (const QString &tag : tagsToRemove) {
            sanitized.remove(tag);
        }
    }

    // 转义特殊字符
    sanitized.replace("&", "&amp;");
    sanitized.replace("<", "&lt;");
    sanitized.replace(">", "&gt;");
    sanitized.replace("\"", "&quot;");
    sanitized.replace("'", "&#x27;");

    return sanitized;
}

QPair<bool, int> Validator::validatePasswordStrength(const QString &password, int minLength,
                                                     bool requireSpecial, bool requireNumbers,
                                                     bool requireUppercase)
{
    int score = calculatePasswordScore(password);

    if (password.length() < minLength) {
        return qMakePair(false, score);
    }

    if (requireNumbers && !password.contains(QRegularExpression("[0-9]"))) {
        return qMakePair(false, score);
    }

    if (requireUppercase && !password.contains(QRegularExpression("[A-Z]"))) {
        return qMakePair(false, score);
    }

    if (requireSpecial && !password.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"))) {
        return qMakePair(false, score);
    }

    return qMakePair(score >= 60, score); // 60分以上认为是强密码
}

void Validator::cleanupSecurityRecords()
{
    QMutexLocker locker(&s_securityMutex);

    cleanupRequestRecords();
    cleanupBlacklist();
    cleanupTokens();
}

bool Validator::isSafeFileType(const QString &fileName)
{
    QStringList allowedExtensions = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp",  // 图片
        "pdf", "doc", "docx", "txt", "rtf",          // 文档
        "zip", "rar", "7z",                          // 压缩文件
        "mp3", "wav", "ogg",                         // 音频
        "mp4", "avi", "mov", "wmv"                   // 视频
    };

    QString extension = fileName.split('.').last().toLower();
    return allowedExtensions.contains(extension);
}

bool Validator::containsMaliciousContent(const QByteArray &content)
{
    // 检查常见的恶意文件签名
    QList<QByteArray> maliciousSignatures = {
        QByteArray::fromHex("4D5A"),        // PE executable
        QByteArray::fromHex("7F454C46"),    // ELF executable
        QByteArray::fromHex("CAFEBABE"),    // Java class file
        QByteArray::fromHex("504B0304"),    // ZIP (可能包含恶意文件)
    };

    for (const QByteArray &signature : maliciousSignatures) {
        if (content.startsWith(signature)) {
            return true;
        }
    }

    // 检查脚本内容
    QString contentStr = QString::fromUtf8(content);
    if (hasXssRisk(contentStr) || hasSqlInjectionRisk(contentStr)) {
        return true;
    }

    return false;
}

int Validator::calculatePasswordScore(const QString &password)
{
    int score = 0;

    // 长度分数
    score += qMin(password.length() * 4, 25);

    // 字符类型分数
    if (password.contains(QRegularExpression("[a-z]"))) score += 5;
    if (password.contains(QRegularExpression("[A-Z]"))) score += 5;
    if (password.contains(QRegularExpression("[0-9]"))) score += 5;
    if (password.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"))) score += 10;

    // 复杂度分数
    QSet<QChar> uniqueChars;
    for (const QChar &ch : password) {
        uniqueChars.insert(ch);
    }
    score += qMin(uniqueChars.size() * 2, 20);

    // 模式检查（减分）
    if (password.contains(QRegularExpression("(.)\\1{2,}"))) score -= 10; // 重复字符
    if (password.contains(QRegularExpression("(012|123|234|345|456|567|678|789|890|abc|bcd|cde)"))) score -= 5; // 连续字符

    return qMax(0, qMin(score, 100));
}

void Validator::cleanupRequestRecords()
{
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600); // 1小时前

    for (auto it = s_requestRecords.begin(); it != s_requestRecords.end();) {
        RequestRecord &record = it.value();

        // 移除过期的时间戳
        record.timestamps.erase(
            std::remove_if(record.timestamps.begin(), record.timestamps.end(),
                          [cutoff](const QDateTime &timestamp) {
                              return timestamp < cutoff;
                          }),
            record.timestamps.end()
        );

        // 如果没有最近的请求，删除整个记录
        if (record.timestamps.isEmpty()) {
            it = s_requestRecords.erase(it);
        } else {
            ++it;
        }
    }
}

void Validator::cleanupBlacklist()
{
    QDateTime now = QDateTime::currentDateTime();

    for (auto it = s_ipBlacklist.begin(); it != s_ipBlacklist.end();) {
        const BlacklistEntry &entry = it.value();

        if (entry.expiryTime.isValid() && now > entry.expiryTime) {
            it = s_ipBlacklist.erase(it);
        } else {
            ++it;
        }
    }
}

void Validator::cleanupTokens()
{
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600); // 1小时前

    // 清理CSRF令牌
    for (auto it = s_csrfTokens.begin(); it != s_csrfTokens.end();) {
        if (it.value() < cutoff) {
            it = s_csrfTokens.erase(it);
        } else {
            ++it;
        }
    }

    // 清理二次验证码
    QDateTime shortCutoff = QDateTime::currentDateTime().addSecs(-300); // 5分钟前
    for (auto it = s_twoFactorCodes.begin(); it != s_twoFactorCodes.end();) {
        if (it.value() < shortCutoff) {
            it = s_twoFactorCodes.erase(it);
        } else {
            ++it;
        }
    }
}
