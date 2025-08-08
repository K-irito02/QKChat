#include "EmailService.h"
#include "SmtpClient.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include "../utils/Validator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>

EmailService::EmailService(QObject *parent)
    : QObject(parent)
    , _databaseManager(DatabaseManager::instance())
    , _smtpPort(587)
    , _useTLS(true)
    , _initialized(false)
    , _sendInterval(60)
    , _maxPerHour(10)
    , _codeExpiration(5)
{
    // 创建SMTP客户端
    _smtpClient = new SmtpClient(this);
    connect(_smtpClient, &SmtpClient::emailSent, this, &EmailService::onEmailSent);
    connect(_smtpClient, &SmtpClient::emailFailed, this, &EmailService::onEmailFailed);

    // 创建清理定时器（每小时清理一次）
    _cleanupTimer = new QTimer(this);
    _cleanupTimer->setInterval(3600000); // 1小时
    _cleanupTimer->setSingleShot(false);
    connect(_cleanupTimer, &QTimer::timeout, this, &EmailService::onCleanupTimer);
    _cleanupTimer->start();
}

EmailService::~EmailService()
{
}

bool EmailService::initialize(const QString &smtpServer, int smtpPort,
                             const QString &username, const QString &password, bool useTLS)
{
    _smtpServer = smtpServer;
    _smtpPort = smtpPort;
    _username = username;
    _password = password;
    _useTLS = useTLS;

    // 验证配置
    if (_smtpServer.isEmpty() || _username.isEmpty() || _password.isEmpty()) {
        LOG_ERROR("Email service configuration is incomplete");
        return false;
    }

    if (!Validator::isValidEmail(_username)) {
        LOG_ERROR("Invalid email username in configuration");
        return false;
    }

    if (!Validator::isValidPort(_smtpPort)) {
        LOG_ERROR("Invalid SMTP port in configuration");
        return false;
    }

    // 配置SMTP客户端
    _smtpClient->configure(_smtpServer, _smtpPort, _username, _password, _useTLS, true);
    _smtpClient->setMaxRetries(3);
    _smtpClient->setConnectionTimeout(30000);

    _initialized = true;

    LOG_INFO(QString("Email service initialized: %1:%2 (TLS: %3)")
             .arg(_smtpServer).arg(_smtpPort).arg(_useTLS ? "Yes" : "No"));

    return true;
}

EmailService::SendResult EmailService::sendVerificationCode(const QString &email, CodeType codeType)
{
    if (!_initialized) {
        LOG_ERROR("Email service not initialized");
        return ConfigError;
    }
    
    // 验证邮箱格式
    if (!Validator::isValidEmail(email)) {
        LOG_WARNING(QString("Invalid email format: %1").arg(email));
        return InvalidEmail;
    }
    
    // 检查发送频率限制
    if (isRateLimited(email)) {
        LOG_WARNING(QString("Rate limited for email: %1").arg(email));
        return RateLimited;
    }
    
    // 生成验证码
    QString code = generateVerificationCode();
    
    // 保存验证码到数据库
    if (!saveVerificationCode(email, code, codeType)) {
        LOG_ERROR(QString("Failed to save verification code for email: %1").arg(email));
        return DatabaseError;
    }
    
    // 获取邮件内容
    QString subject = getEmailSubject(codeType);
    QString content = getEmailTemplate(codeType, code);
    
    // 发送邮件
    bool success = sendEmailInternal(email, subject, content, true);
    
    // 记录发送历史
    recordSendHistory(email, success);
    
    if (success) {
        LOG_INFO(QString("Verification code sent successfully to: %1").arg(email));
        emit emailSent(email, Success);
        return Success;
    } else {
        LOG_ERROR(QString("Failed to send verification code to: %1").arg(email));
        emit emailSent(email, SmtpError);
        return SmtpError;
    }
}

bool EmailService::verifyCode(const QString &email, const QString &code, CodeType codeType)
{
    if (!Validator::isValidEmail(email) || !Validator::isValidVerificationCode(code)) {
        return false;
    }
    
    QString sql = R"(
        SELECT id FROM verification_codes 
        WHERE email = ? AND code = ? AND type = ? AND expires_at > NOW() AND used_at IS NULL
        ORDER BY created_at DESC LIMIT 1
    )";
    
    QSqlQuery query = _databaseManager->executeQuery(sql, {email, code, codeTypeToString(codeType)});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error verifying code: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (query.next()) {
        // 标记验证码为已使用
        qint64 codeId = query.value(0).toLongLong();
        QString updateSql = "UPDATE verification_codes SET used_at = NOW() WHERE id = ?";
        _databaseManager->executeUpdate(updateSql, {codeId});
        
        LOG_INFO(QString("Verification code verified successfully for email: %1").arg(email));
        return true;
    }
    
    LOG_WARNING(QString("Invalid or expired verification code for email: %1").arg(email));
    return false;
}

EmailService::SendResult EmailService::sendCustomEmail(const QString &email, const QString &subject, 
                                                       const QString &content, bool isHtml)
{
    if (!_initialized) {
        return ConfigError;
    }
    
    if (!Validator::isValidEmail(email)) {
        return InvalidEmail;
    }
    
    if (isRateLimited(email)) {
        return RateLimited;
    }
    
    bool success = sendEmailInternal(email, subject, content, isHtml);
    recordSendHistory(email, success);
    
    if (success) {
        emit emailSent(email, Success);
        return Success;
    } else {
        emit emailSent(email, SmtpError);
        return SmtpError;
    }
}

bool EmailService::isRateLimited(const QString &email)
{
    return !checkSendFrequency(email);
}

QString EmailService::getSendResultDescription(SendResult result)
{
    switch (result) {
        case Success:
            return "邮件发送成功";
        case InvalidEmail:
            return "邮箱地址无效";
        case RateLimited:
            return "发送频率过快，请稍后再试";
        case SmtpError:
            return "SMTP服务器错误";
        case NetworkError:
            return "网络连接错误";
        case ConfigError:
            return "邮件服务配置错误";
        case DatabaseError:
            return "数据库错误";
        default:
            return "未知错误";
    }
}

void EmailService::setRateLimit(int intervalSeconds, int maxPerHour)
{
    _sendInterval = intervalSeconds;
    _maxPerHour = maxPerHour;
    
    LOG_INFO(QString("Email rate limit updated: %1s interval, %2 per hour")
             .arg(intervalSeconds).arg(maxPerHour));
}

void EmailService::setCodeExpiration(int minutes)
{
    _codeExpiration = minutes;
    
    LOG_INFO(QString("Verification code expiration updated: %1 minutes").arg(minutes));
}

void EmailService::cleanup()
{
    // 清理数据库中过期的验证码
    QString sql = "DELETE FROM verification_codes WHERE expires_at < NOW()";
    int deleted = _databaseManager->executeUpdate(sql);
    
    if (deleted > 0) {
        LOG_INFO(QString("Cleaned up %1 expired verification codes").arg(deleted));
    }
    
    // 清理内存中的发送历史（保留最近24小时）
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-86400);
    
    for (auto it = _sendHistory.begin(); it != _sendHistory.end();) {
        QList<QDateTime>& history = it.value();
        history.erase(std::remove_if(history.begin(), history.end(),
                                   [cutoff](const QDateTime& dt) { return dt < cutoff; }),
                     history.end());
        
        if (history.isEmpty()) {
            it = _sendHistory.erase(it);
        } else {
            ++it;
        }
    }
    
    // 清理最后发送时间记录
    for (auto it = _lastSendTime.begin(); it != _lastSendTime.end();) {
        if (it.value() < cutoff) {
            it = _lastSendTime.erase(it);
        } else {
            ++it;
        }
    }
}

void EmailService::onCleanupTimer()
{
    cleanup();
}

QString EmailService::generateVerificationCode()
{
    return Crypto::generateVerificationCode(6);
}

bool EmailService::saveVerificationCode(const QString &email, const QString &code, CodeType codeType)
{
    QString sql = R"(
        INSERT INTO verification_codes (email, code, type, expires_at, created_at)
        VALUES (?, ?, ?, DATE_ADD(NOW(), INTERVAL ? MINUTE), NOW())
    )";

    QVariantList params = {email, code, codeTypeToString(codeType), _codeExpiration};

    int result = _databaseManager->executeUpdate(sql, params);

    if (result > 0) {
    
        return true;
    } else {
        LOG_ERROR(QString("Failed to save verification code: %1").arg(_databaseManager->lastError()));
        return false;
    }
}

QString EmailService::getEmailTemplate(CodeType codeType, const QString &code)
{
    QString title;
    QString description;
    QString note;

    switch (codeType) {
        case Registration:
            title = "欢迎注册QKChat";
            description = "感谢您注册QKChat！请使用以下验证码完成注册：";
            note = "如果您没有注册QKChat账号，请忽略此邮件。";
            break;
        case PasswordReset:
            title = "重置密码";
            description = "您正在重置QKChat账号密码，请使用以下验证码：";
            note = "如果您没有申请重置密码，请立即联系我们。";
            break;
        case EmailChange:
            title = "更改邮箱";
            description = "您正在更改QKChat账号邮箱，请使用以下验证码：";
            note = "如果您没有申请更改邮箱，请立即联系我们。";
            break;
    }

    QString html = QString(R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%1</title>
    <style>
        body { font-family: Arial, sans-serif; line-height: 1.6; color: #333; margin: 0; padding: 20px; background-color: #f4f4f4; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        .header { text-align: center; margin-bottom: 30px; }
        .logo { font-size: 28px; font-weight: bold; color: #007AFF; margin-bottom: 10px; }
        .title { font-size: 24px; color: #333; margin-bottom: 20px; }
        .code-container { background: #f8f9fa; border: 2px dashed #007AFF; border-radius: 8px; padding: 20px; text-align: center; margin: 20px 0; }
        .code { font-size: 32px; font-weight: bold; color: #007AFF; letter-spacing: 5px; font-family: 'Courier New', monospace; }
        .description { font-size: 16px; margin-bottom: 20px; }
        .note { font-size: 14px; color: #666; margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; }
        .footer { text-align: center; margin-top: 30px; font-size: 12px; color: #999; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="logo">QKChat</div>
            <div class="title">%1</div>
        </div>

        <div class="description">%2</div>

        <div class="code-container">
            <div class="code">%3</div>
        </div>

        <div class="description">验证码有效期为 %4 分钟，请及时使用。</div>

        <div class="note">%5</div>

        <div class="footer">
            <p>此邮件由QKChat系统自动发送，请勿回复。</p>
            <p>&copy; 2024 QKChat. All rights reserved.</p>
        </div>
    </div>
</body>
</html>
    )").arg(title).arg(description).arg(code).arg(_codeExpiration).arg(note);

    return html;
}

QString EmailService::getEmailSubject(CodeType codeType)
{
    switch (codeType) {
        case Registration:
            return "QKChat - 注册验证码";
        case PasswordReset:
            return "QKChat - 密码重置验证码";
        case EmailChange:
            return "QKChat - 邮箱更改验证码";
        default:
            return "QKChat - 验证码";
    }
}

void EmailService::recordSendHistory(const QString &email, bool success)
{
    QDateTime now = QDateTime::currentDateTime();

    // 记录最后发送时间
    _lastSendTime[email] = now;

    // 记录发送历史
    if (!_sendHistory.contains(email)) {
        _sendHistory[email] = QList<QDateTime>();
    }
    _sendHistory[email].append(now);

    // 保持历史记录在合理范围内（最多保留最近100次）
    if (_sendHistory[email].size() > 100) {
        _sendHistory[email].removeFirst();
    }
}

bool EmailService::checkSendFrequency(const QString &email)
{
    QDateTime now = QDateTime::currentDateTime();

    // 检查发送间隔
    if (_lastSendTime.contains(email)) {
        QDateTime lastSend = _lastSendTime[email];
        if (lastSend.secsTo(now) < _sendInterval) {
            return false; // 发送间隔太短
        }
    }

    // 检查每小时发送次数
    if (_sendHistory.contains(email)) {
        QDateTime oneHourAgo = now.addSecs(-3600);
        QList<QDateTime>& history = _sendHistory[email];

        int recentCount = 0;
        for (const QDateTime& sendTime : history) {
            if (sendTime > oneHourAgo) {
                recentCount++;
            }
        }

        if (recentCount >= _maxPerHour) {
            return false; // 每小时发送次数超限
        }
    }

    return true;
}

bool EmailService::sendEmailInternal(const QString &email, const QString &subject,
                                    const QString &content, bool isHtml)
{
    if (!_initialized) {
        LOG_ERROR("Email service not initialized");
        return false;
    }

    LOG_INFO(QString("Sending email to: %1, Subject: %2").arg(email).arg(subject));

    // 使用真实的SMTP客户端发送邮件
    QString messageId = _smtpClient->sendEmail(email, subject, content, isHtml, "QKChat Server");

    if (!messageId.isEmpty()) {
        LOG_INFO(QString("Email queued for sending: %1").arg(messageId));
        return true;
    } else {
        LOG_ERROR("Failed to queue email for sending");
        return false;
    }
}

QString EmailService::codeTypeToString(CodeType codeType)
{
    switch (codeType) {
        case Registration:
            return "registration";
        case PasswordReset:
            return "password_reset";
        case EmailChange:
            return "email_change";
        default:
            return "registration";
    }
}

void EmailService::onEmailSent(const QString &messageId)
{
    LOG_INFO(QString("Email sent successfully: %1").arg(messageId));
    // 这里可以添加发送成功后的处理逻辑，如更新数据库状态
}

void EmailService::onEmailFailed(const QString &messageId, const QString &error)
{
    LOG_ERROR(QString("Email failed to send: %1 - %2").arg(messageId).arg(error));
    // 这里可以添加发送失败后的处理逻辑，如记录错误、重试等
}
