#include "EmailService.h"
#include "VerificationCodeManager.h"
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
#include <QUuid>
#include <QTimer>

EmailService::EmailService(QObject *parent)
    : QObject(parent)
    , _databaseManager(DatabaseManager::instance())
    , _redisClient(RedisClient::instance())
    , _smtpPort(587)
    , _useTLS(true)
    , _initialized(false)
    , _codeExpiration(5)
{
    // 创建SMTP客户端
    _smtpClient = new SmtpClient(this);
    connect(_smtpClient, &SmtpClient::emailSent, this, &EmailService::onEmailSent);
    connect(_smtpClient, &SmtpClient::emailFailed, this, &EmailService::onEmailFailed);

    // 移除自动清理定时器，由客户端控制验证码发送
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
    // QQ邮箱465端口使用直接SSL连接，587端口使用STARTTLS
    bool useStartTls = (_smtpPort == 587);
    
    // QQ邮箱特殊配置：465端口必须使用SSL，不能使用STARTTLS
    if (_smtpPort == 465) {
        useStartTls = false;
        _useTLS = true;
    }
    

    
    _smtpClient->configure(_smtpServer, _smtpPort, _username, _password, _useTLS, useStartTls);
    _smtpClient->setConnectionTimeout(30000);

    _initialized = true;



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
    
    // 使用 VerificationCodeManager 生成并保存验证码（包含频率限制）
    VerificationCodeManager* codeManager = VerificationCodeManager::instance();
    if (!codeManager) {
        LOG_ERROR("VerificationCodeManager not available");
        return DatabaseError;
    }
    
    QString code = codeManager->generateAndSaveCodeInternal(email, static_cast<VerificationCodeManager::CodeType>(codeType));
    if (code.isEmpty()) {
        LOG_ERROR(QString("Failed to generate verification code for email: %1").arg(email));
        return DatabaseError;
    }
    
    return sendVerificationCode(email, code, codeType);
}

EmailService::SendResult EmailService::sendVerificationCode(const QString &email, const QString &code, CodeType codeType)
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
    
    // 获取邮件内容
    QString subject = getEmailSubject(codeType);
    QString content = getEmailTemplate(codeType, code);
    
    // 发送验证码邮件
    bool success = sendVerificationCodeEmail(email, subject, content, true);
    

    
    if (success) {
    
        emit emailSent(email, Success);
        return Success;
    } else {
        LOG_ERROR(QString("Failed to send verification code to: %1").arg(email));
        emit emailSent(email, SmtpError);
        return SmtpError;
    }
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
    
    bool success = sendEmailInternal(email, subject, content, isHtml);
    
    if (success) {
        emit emailSent(email, Success);
        return Success;
    } else {
        emit emailSent(email, SmtpError);
        return SmtpError;
    }
}



QString EmailService::getSendResultDescription(SendResult result)
{
    switch (result) {
        case Success:
            return "验证码已发送，请注意查收";
        case InvalidEmail:
            return "邮箱地址无效，请检查邮箱格式";
        case RateLimited:
            return "验证码发送频繁，请稍后再试";
        case SmtpError:
            return "验证码发送失败，请重试";
        case NetworkError:
            return "网络连接错误，请检查网络后重试";
        case ConfigError:
            return "邮件服务配置错误，请联系管理员";
        case DatabaseError:
            return "数据库错误，请重试";
        default:
            return "验证码发送失败，请重试";
    }
}



void EmailService::setCodeExpiration(int minutes)
{
    _codeExpiration = minutes;
    

}

// 移除自动清理功能，由客户端控制验证码发送





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
            <p>&copy; 2025 QKChat. All rights reserved.</p>
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





bool EmailService::sendEmailInternal(const QString &email, const QString &subject,
                                    const QString &content, bool isHtml)
{
    if (!_initialized) {
        LOG_ERROR("Email service not initialized");
        return false;
    }

    // 使用真实的SMTP客户端发送邮件
    QString messageId = _smtpClient->sendEmail(email, subject, content, isHtml, "QKChat Server");

    if (!messageId.isEmpty()) {
        return true;
    } else {
        LOG_ERROR("Failed to queue email for sending");
        return false;
    }
}

bool EmailService::sendVerificationCodeEmail(const QString &email, const QString &subject,
                                           const QString &content, bool isHtml)
{
    if (!_initialized) {
        LOG_ERROR("Email service not initialized");
        return false;
    }

    // 创建验证码邮件消息
    SmtpClient::EmailMessage message;
    message.from = _username;
    message.fromName = "QKChat Server";
    message.to = email;
    message.subject = subject;
    message.body = content;
    message.isHtml = isHtml;
    message.isVerificationCode = true;
    message.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QString messageId = _smtpClient->sendEmail(message);

    if (!messageId.isEmpty()) {
        return true;
    } else {
        LOG_ERROR("Failed to queue verification code email for sending");
        return false;
    }
}



void EmailService::onEmailSent(const QString &messageId)
{

    // 这里可以添加发送成功后的处理逻辑，如更新数据库状态
}

void EmailService::onEmailFailed(const QString &messageId, const QString &error)
{
    LOG_ERROR(QString("Email failed to send: %1 - %2").arg(messageId).arg(error));
    // 邮件发送失败，由客户端决定是否重试
}
