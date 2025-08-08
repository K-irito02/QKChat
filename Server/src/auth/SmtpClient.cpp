#include "SmtpClient.h"
#include "../utils/Logger.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QUuid>

SmtpClient::SmtpClient(QObject *parent)
    : QObject(parent)
    , _socket(nullptr)
    , _port(587)
    , _useTls(true)
    , _useStartTls(true)
    , _state(Disconnected)
    , _expectedResponseCode(0)
    , _connectionTimeout(30000)
    , _maxRetries(3)
    , _tlsStarted(false)
    , _authenticated(false)
{
    // 创建SSL套接字
    _socket = new QSslSocket(this);
    
    // 连接信号
    connect(_socket, &QSslSocket::connected, this, &SmtpClient::onConnected);
    connect(_socket, &QSslSocket::disconnected, this, &SmtpClient::onDisconnected);
    connect(_socket, &QSslSocket::readyRead, this, &SmtpClient::onReadyRead);
    connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::errorOccurred),
            this, &SmtpClient::onSocketError);
    connect(_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            this, &SmtpClient::onSslErrors);
    
    // 创建定时器
    _connectionTimer = new QTimer(this);
    _connectionTimer->setSingleShot(true);
    connect(_connectionTimer, &QTimer::timeout, this, &SmtpClient::onConnectionTimeout);
    
    _queueTimer = new QTimer(this);
    _queueTimer->setSingleShot(true);
    connect(_queueTimer, &QTimer::timeout, this, &SmtpClient::processQueue);
}

SmtpClient::~SmtpClient()
{
    disconnectFromServer();
}

void SmtpClient::configure(const QString &host, int port, const QString &username, 
                          const QString &password, bool useTls, bool useStartTls)
{
    _host = host;
    _port = port;
    _username = username;
    _password = password;
    _useTls = useTls;
    _useStartTls = useStartTls;
    
    LOG_INFO(QString("SMTP client configured: %1:%2 (TLS: %3, STARTTLS: %4)")
             .arg(_host).arg(_port).arg(_useTls).arg(_useStartTls));
}

QString SmtpClient::sendEmail(const QString &to, const QString &subject, const QString &body, 
                             bool isHtml, const QString &fromName)
{
    EmailMessage message;
    message.from = _username;
    message.fromName = fromName.isEmpty() ? "QKChat Server" : fromName;
    message.to = to;
    message.subject = subject;
    message.body = body;
    message.isHtml = isHtml;
    message.messageId = generateMessageId();
    
    return sendEmail(message);
}

QString SmtpClient::sendEmail(const EmailMessage &message)
{
    QMutexLocker locker(&_queueMutex);
    
    EmailMessage msg = message;
    if (msg.messageId.isEmpty()) {
        msg.messageId = generateMessageId();
    }
    
    _emailQueue.enqueue(msg);
    
    LOG_INFO(QString("Email queued: %1 -> %2 (Subject: %3)")
             .arg(msg.from).arg(msg.to).arg(msg.subject));
    
    // 如果当前没有在发送邮件，立即开始处理队列
    if (_state == Disconnected || _state == Error) {
        _queueTimer->start(100); // 延迟100ms开始处理
    }
    
    return msg.messageId;
}

int SmtpClient::queueSize() const
{
    QMutexLocker locker(&_queueMutex);
    return _emailQueue.size();
}

bool SmtpClient::connectToServer()
{
    if (_state != Disconnected && _state != Error) {
        LOG_WARNING("SMTP client already connected or connecting");
        return false;
    }
    
    if (_host.isEmpty() || _username.isEmpty()) {
        LOG_ERROR("SMTP configuration incomplete");
        setState(Error);
        return false;
    }
    
    LOG_INFO(QString("Connecting to SMTP server: %1:%2").arg(_host).arg(_port));
    
    setState(Connecting);
    _tlsStarted = false;
    _authenticated = false;
    _serverCapabilities.clear();
    
    // 启动连接超时定时器
    _connectionTimer->start(_connectionTimeout);
    
    // 连接到服务器
    if (_useTls && !_useStartTls) {
        // 直接使用SSL连接
        _socket->connectToHostEncrypted(_host, _port);
    } else {
        // 普通连接或STARTTLS
        _socket->connectToHost(_host, _port);
    }
    
    return true;
}

void SmtpClient::disconnectFromServer()
{
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        sendCommand("QUIT");
        _socket->waitForBytesWritten(3000);
        _socket->disconnectFromHost();
        
        if (_socket->state() != QAbstractSocket::UnconnectedState) {
            _socket->waitForDisconnected(3000);
        }
    }
    
    _connectionTimer->stop();
    setState(Disconnected);
    
    LOG_INFO("Disconnected from SMTP server");
}

void SmtpClient::onConnected()
{
    _connectionTimer->stop();
    setState(Connected);
    _expectedResponseCode = 220; // 期待服务器欢迎消息
    
    LOG_INFO("Connected to SMTP server");
}

void SmtpClient::onDisconnected()
{
    setState(Disconnected);
    _tlsStarted = false;
    _authenticated = false;
    
    LOG_INFO("Disconnected from SMTP server");
    
    // 如果队列中还有邮件，尝试重连
    if (!_emailQueue.isEmpty()) {
        _queueTimer->start(5000); // 5秒后重试
    }
}

void SmtpClient::onReadyRead()
{
    QByteArray data = _socket->readAll();
    QString response = QString::fromUtf8(data).trimmed();
    

    
    _lastResponse = response;
    handleSmtpResponse(response);
}

void SmtpClient::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = _socket->errorString();
    LOG_ERROR(QString("SMTP socket error: %1").arg(errorString));
    
    setState(Error);
    emit smtpError(errorString);
    
    // 如果当前有邮件在发送，标记为失败
    if (!_currentEmail.messageId.isEmpty()) {
        finishCurrentEmail(false, errorString);
    }
}

void SmtpClient::onSslErrors(const QList<QSslError> &errors)
{
    QStringList errorStrings;
    for (const QSslError &error : errors) {
        errorStrings << error.errorString();
    }
    
    QString errorString = errorStrings.join("; ");
    LOG_WARNING(QString("SMTP SSL errors: %1").arg(errorString));
    
    // 在生产环境中应该根据具体错误决定是否忽略
    _socket->ignoreSslErrors();
}

void SmtpClient::onConnectionTimeout()
{
    LOG_ERROR("SMTP connection timeout");
    
    setState(Error);
    _socket->abort();
    
    if (!_currentEmail.messageId.isEmpty()) {
        finishCurrentEmail(false, "Connection timeout");
    }
}

void SmtpClient::processQueue()
{
    QMutexLocker locker(&_queueMutex);
    
    if (_emailQueue.isEmpty()) {
        return;
    }
    
    if (_state == Disconnected || _state == Error) {
        locker.unlock();
        connectToServer();
        return;
    }
    
    if (_state == Authenticated && _currentEmail.messageId.isEmpty()) {
        _currentEmail = _emailQueue.dequeue();
        locker.unlock();
        sendCurrentEmail();
    }
}

void SmtpClient::handleSmtpResponse(const QString &response)
{
    // 解析响应代码
    QRegularExpression regex(R"(^(\d{3})\s*(.*))");
    QRegularExpressionMatch match = regex.match(response);
    
    if (!match.hasMatch()) {
        LOG_WARNING(QString("Invalid SMTP response format: %1").arg(response));
        return;
    }
    
    int responseCode = match.captured(1).toInt();
    QString responseText = match.captured(2);
    
    // 检查是否是期待的响应代码
    if (_expectedResponseCode > 0 && responseCode != _expectedResponseCode) {
        if (responseCode >= 400) {
            LOG_ERROR(QString("SMTP error response: %1").arg(response));
            
            if (!_currentEmail.messageId.isEmpty()) {
                finishCurrentEmail(false, response);
            } else {
                setState(Error);
                emit smtpError(response);
            }
            return;
        }
    }
    
    // 根据当前状态和响应代码处理
    switch (_state) {
        case Connected:
            if (responseCode == 220) {
                // 服务器欢迎消息，发送EHLO
                sendCommand(QString("EHLO %1").arg(_host));
                _expectedResponseCode = 250;
            } else if (responseCode == 250) {
                // EHLO响应
                _serverCapabilities += responseText + "\n";
                
                // 检查是否支持STARTTLS
                if (_useStartTls && !_tlsStarted && _serverCapabilities.contains("STARTTLS", Qt::CaseInsensitive)) {
                    startTls();
                } else {
                    authenticate();
                }
            }
            break;
            
        case Authenticating:
            if (responseCode == 235) {
                // 认证成功
                setState(Authenticated);
                _authenticated = true;
                LOG_INFO("SMTP authentication successful");
                
                // 开始处理邮件队列
                processQueue();
            } else if (responseCode == 334) {
                // 继续认证过程（BASE64编码的挑战）
                // 这里简化处理，实际项目中需要处理CRAM-MD5等复杂认证
            }
            break;
            
        case Sending:
            handleEmailSendingResponse(responseCode, responseText);
            break;
            
        default:
            break;
    }
}

void SmtpClient::sendCommand(const QString &command)
{
    if (_socket->state() != QAbstractSocket::ConnectedState) {
        LOG_ERROR("Cannot send SMTP command: not connected");
        return;
    }

    QString cmd = command + "\r\n";
    _socket->write(cmd.toUtf8());
    _socket->flush();


}

void SmtpClient::startTls()
{
    LOG_INFO("Starting TLS encryption");

    sendCommand("STARTTLS");
    _expectedResponseCode = 220;

    // 等待服务器响应
    if (_socket->waitForReadyRead(5000)) {
        if (_lastResponse.startsWith("220")) {
            _socket->startClientEncryption();
            _tlsStarted = true;

            // TLS握手完成后重新发送EHLO
            sendCommand(QString("EHLO %1").arg(_host));
            _expectedResponseCode = 250;
        }
    }
}

void SmtpClient::authenticate()
{
    if (_username.isEmpty() || _password.isEmpty()) {
        LOG_WARNING("No SMTP credentials provided, skipping authentication");
        setState(Authenticated);
        _authenticated = true;
        processQueue();
        return;
    }

    LOG_INFO("Starting SMTP authentication");
    setState(Authenticating);

    // 使用LOGIN认证方式
    sendCommand("AUTH LOGIN");
    _expectedResponseCode = 334;

    // 等待服务器响应
    if (_socket->waitForReadyRead(5000)) {
        if (_lastResponse.startsWith("334")) {
            // 发送用户名（Base64编码）
            QString encodedUsername = base64Encode(_username);
            sendCommand(encodedUsername);

            if (_socket->waitForReadyRead(5000)) {
                if (_lastResponse.startsWith("334")) {
                    // 发送密码（Base64编码）
                    QString encodedPassword = base64Encode(_password);
                    sendCommand(encodedPassword);
                    _expectedResponseCode = 235;
                }
            }
        }
    }
}

void SmtpClient::sendCurrentEmail()
{
    if (_currentEmail.messageId.isEmpty()) {
        LOG_ERROR("No current email to send");
        return;
    }

    LOG_INFO(QString("Sending email: %1").arg(_currentEmail.messageId));
    setState(Sending);

    // 发送MAIL FROM命令
    sendCommand(QString("MAIL FROM:<%1>").arg(_currentEmail.from));
    _expectedResponseCode = 250;
}

void SmtpClient::handleEmailSendingResponse(int responseCode, const QString &responseText)
{
    static int sendingStep = 0; // 0: MAIL FROM, 1: RCPT TO, 2: DATA, 3: Content

    if (responseCode >= 400) {
        finishCurrentEmail(false, QString("%1 %2").arg(responseCode).arg(responseText));
        return;
    }

    switch (sendingStep) {
        case 0: // MAIL FROM响应
            if (responseCode == 250) {
                sendCommand(QString("RCPT TO:<%1>").arg(_currentEmail.to));
                sendingStep = 1;
            }
            break;

        case 1: // RCPT TO响应
            if (responseCode == 250) {
                sendCommand("DATA");
                sendingStep = 2;
                _expectedResponseCode = 354;
            }
            break;

        case 2: // DATA响应
            if (responseCode == 354) {
                // 发送邮件内容
                QString emailContent = formatEmailContent(_currentEmail);
                _socket->write(emailContent.toUtf8());
                _socket->write("\r\n.\r\n"); // 邮件结束标记
                _socket->flush();
                sendingStep = 3;
                _expectedResponseCode = 250;
            }
            break;

        case 3: // 邮件发送完成
            if (responseCode == 250) {
                finishCurrentEmail(true);
            }
            sendingStep = 0;
            break;
    }
}

void SmtpClient::finishCurrentEmail(bool success, const QString &error)
{
    if (_currentEmail.messageId.isEmpty()) {
        return;
    }

    QString messageId = _currentEmail.messageId;

    if (success) {
        LOG_INFO(QString("Email sent successfully: %1").arg(messageId));
        emit emailSent(messageId);
    } else {
        LOG_ERROR(QString("Email failed: %1 - %2").arg(messageId).arg(error));

        // 检查是否需要重试
        if (_currentEmail.retryCount < _maxRetries) {
            _currentEmail.retryCount++;
            LOG_INFO(QString("Retrying email: %1 (attempt %2/%3)")
                     .arg(messageId).arg(_currentEmail.retryCount).arg(_maxRetries));

            QMutexLocker locker(&_queueMutex);
            _emailQueue.prepend(_currentEmail); // 重新加入队列头部
            locker.unlock();

            _currentEmail = EmailMessage(); // 清空当前邮件
            _queueTimer->start(5000); // 5秒后重试
            return;
        }

        emit emailFailed(messageId, error);
    }

    // 清空当前邮件
    _currentEmail = EmailMessage();

    // 继续处理队列
    _queueTimer->start(100);
}

void SmtpClient::setState(SmtpState state)
{
    if (_state != state) {
        _state = state;
        emit stateChanged(state);
    }
}

QString SmtpClient::generateMessageId()
{
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return QString("qkchat_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(uuid);
}

QString SmtpClient::encodeHeader(const QString &text)
{
    // 简化的邮件头编码，实际项目中应该使用RFC 2047编码
    if (text.contains(QRegularExpression("[^\\x00-\\x7F]"))) {
        // 包含非ASCII字符，使用UTF-8编码
        return QString("=?UTF-8?B?%1?=").arg(QString::fromUtf8(text.toUtf8().toBase64()));
    }
    return text;
}

QString SmtpClient::formatEmailContent(const EmailMessage &message)
{
    QString content;

    // 邮件头
    content += QString("From: %1 <%2>\r\n")
               .arg(encodeHeader(message.fromName))
               .arg(message.from);
    content += QString("To: %1\r\n").arg(message.to);
    content += QString("Subject: %1\r\n").arg(encodeHeader(message.subject));
    content += QString("Date: %1\r\n").arg(QDateTime::currentDateTime().toString(Qt::RFC2822Date));
    content += QString("Message-ID: <%1@qkchat.local>\r\n").arg(message.messageId);
    content += "MIME-Version: 1.0\r\n";

    if (message.isHtml) {
        content += "Content-Type: text/html; charset=UTF-8\r\n";
    } else {
        content += "Content-Type: text/plain; charset=UTF-8\r\n";
    }

    content += "Content-Transfer-Encoding: 8bit\r\n";
    content += "\r\n"; // 空行分隔头和正文

    // 邮件正文
    content += message.body;

    return content;
}

QString SmtpClient::base64Encode(const QString &data)
{
    return QString::fromUtf8(data.toUtf8().toBase64());
}
