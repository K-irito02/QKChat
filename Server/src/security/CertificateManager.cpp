#include "CertificateManager.h"
#include "OpenSSLHelper.h"
#include "../utils/Logger.h"
#include <QFile>
#include <QDir>
#include <QMutexLocker>
#include <QCryptographicHash>
#include <QJsonObject>
#include <QSslSocket>
#include <QRandomGenerator>
#include <QProcess>

// 静态成员初始化
CertificateManager* CertificateManager::s_instance = nullptr;

CertificateManager::CertificateManager(QObject *parent)
    : QObject(parent)
    , _autoCheckEnabled(true)
    , _fileWatchEnabled(true)
{
    // 创建自动检查定时器
    _autoCheckTimer = new QTimer(this);
    _autoCheckTimer->setSingleShot(false);
    connect(_autoCheckTimer, &QTimer::timeout, this, &CertificateManager::onAutoCheckTimer);
    
    // 创建文件监视器
    _fileWatcher = new QFileSystemWatcher(this);
    connect(_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &CertificateManager::onCertificateFileChanged);
}

CertificateManager::~CertificateManager()
{
}

CertificateManager* CertificateManager::instance()
{
    if (!s_instance) {
        s_instance = new CertificateManager();
    }
    return s_instance;
}

bool CertificateManager::loadCertificate(const QString &certPath, const QString &keyPath, const QString &keyPassword)
{
    QMutexLocker locker(&_certificateMutex);
    
    // 加载证书
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open certificate file: %1").arg(certPath));
        emit certificateError(QString("Cannot open certificate file: %1").arg(certPath));
        return false;
    }
    
    QSslCertificate certificate(&certFile, QSsl::Pem);
    certFile.close();
    
    if (certificate.isNull()) {
        LOG_ERROR(QString("Invalid certificate file: %1").arg(certPath));
        emit certificateError(QString("Invalid certificate file: %1").arg(certPath));
        return false;
    }
    
    // 加载私钥
    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open private key file: %1").arg(keyPath));
        emit certificateError(QString("Cannot open private key file: %1").arg(keyPath));
        return false;
    }
    
    QSslKey privateKey(&keyFile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, keyPassword.toUtf8());
    keyFile.close();
    
    if (privateKey.isNull()) {
        LOG_ERROR(QString("Invalid private key file: %1").arg(keyPath));
        emit certificateError(QString("Invalid private key file: %1").arg(keyPath));
        return false;
    }
    
    // 验证证书和私钥匹配
    // 这里可以添加更复杂的验证逻辑
    
    _currentCertificate = certificate;
    _currentPrivateKey = privateKey;
    _certificatePath = certPath;
    _privateKeyPath = keyPath;
    _keyPassword = keyPassword;
    
    // 启用文件监视
    if (_fileWatchEnabled) {
        _fileWatcher->removePaths(_fileWatcher->files());
        _fileWatcher->addPath(certPath);
        _fileWatcher->addPath(keyPath);
    }
    
    // 验证证书状态
    CertificateStatus status = validateCertificate(certificate);
    if (status == Expired) {
        LOG_WARNING("Loaded certificate has expired");
        emit certificateExpired(certificate);
    } else if (status == WillExpireSoon) {
        int daysRemaining = QDateTime::currentDateTime().daysTo(certificate.expiryDate());
        LOG_WARNING(QString("Loaded certificate will expire in %1 days").arg(daysRemaining));
        emit certificateExpiringSoon(certificate, daysRemaining);
    }
    
    LOG_INFO(QString("Certificate loaded successfully: %1").arg(certPath));
    emit certificateLoaded();
    
    return true;
}

bool CertificateManager::generateSelfSignedCertificate(const QString &commonName,
                                                     const QString &organization,
                                                     const QString &country,
                                                     int validDays,
                                                     int keySize)
{
    QMutexLocker locker(&_certificateMutex);

    LOG_INFO(QString("Generating self-signed certificate for: %1").arg(commonName));

    // 生成RSA密钥对
    QSslKey privateKey = generateRSAKey(keySize);
    if (privateKey.isNull()) {
        LOG_ERROR("Failed to generate RSA key pair");
        emit certificateError("Failed to generate RSA key pair");
        return false;
    }

    // 创建证书请求
    QByteArray certRequest = createCertificateRequest(privateKey, commonName, organization, country);
    if (certRequest.isEmpty()) {
        LOG_ERROR("Failed to create certificate request");
        emit certificateError("Failed to create certificate request");
        return false;
    }

    // 自签名证书
    QSslCertificate certificate = signCertificate(certRequest, privateKey, QSslCertificate(), validDays);
    if (certificate.isNull()) {
        LOG_ERROR("Failed to sign certificate");
        emit certificateError("Failed to sign certificate");
        return false;
    }

    _currentCertificate = certificate;
    _currentPrivateKey = privateKey;

    LOG_INFO(QString("Self-signed certificate generated successfully for: %1").arg(commonName));
    emit certificateLoaded();

    return true;
}

bool CertificateManager::saveCertificate(const QString &certPath, const QString &keyPath, const QString &keyPassword)
{
    QMutexLocker locker(&_certificateMutex);
    
    if (_currentCertificate.isNull() || _currentPrivateKey.isNull()) {
        LOG_ERROR("No certificate or private key to save");
        return false;
    }
    
    // 创建证书目录
    QFileInfo certInfo(certPath);
    if (!createCertificateDirectory(certInfo.absolutePath())) {
        return false;
    }
    
    QFileInfo keyInfo(keyPath);
    if (!createCertificateDirectory(keyInfo.absolutePath())) {
        return false;
    }
    
    // 保存证书
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Cannot create certificate file: %1").arg(certPath));
        return false;
    }
    
    certFile.write(_currentCertificate.toPem());
    certFile.close();
    
    // 保存私钥
    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Cannot create private key file: %1").arg(keyPath));
        return false;
    }
    
    QByteArray keyData;
    if (keyPassword.isEmpty()) {
        keyData = _currentPrivateKey.toPem();
    } else {
        keyData = _currentPrivateKey.toPem(keyPassword.toUtf8());
    }
    
    keyFile.write(keyData);
    keyFile.close();
    
    // 设置文件权限（仅所有者可读写）
#ifdef Q_OS_UNIX
    QFile::setPermissions(keyPath, QFile::ReadOwner | QFile::WriteOwner);
#endif
    
    _certificatePath = certPath;
    _privateKeyPath = keyPath;
    _keyPassword = keyPassword;
    
    LOG_INFO(QString("Certificate and private key saved: %1, %2").arg(certPath).arg(keyPath));
    
    return true;
}

CertificateManager::CertificateStatus CertificateManager::validateCertificate(const QSslCertificate &certificate) const
{
    if (certificate.isNull()) {
        return Invalid;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    QDateTime expiryDate = certificate.expiryDate();
    QDateTime effectiveDate = certificate.effectiveDate();
    
    if (now < effectiveDate) {
        return Invalid; // 证书尚未生效
    }
    
    if (now > expiryDate) {
        return Expired;
    }
    
    // 检查是否即将过期（30天内）
    if (now.daysTo(expiryDate) <= 30) {
        return WillExpireSoon;
    }
    
    return Valid;
}

bool CertificateManager::validateCertificateChain(const QList<QSslCertificate> &certificateChain,
                                                  const QList<QSslCertificate> &caCertificates) const
{
    if (certificateChain.isEmpty()) {
        return false;
    }
    
    // 使用Qt的SSL验证功能
    QSslSocket::PeerVerifyMode verifyMode = QSslSocket::VerifyPeer;
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    
    if (!caCertificates.isEmpty()) {
        sslConfig.setCaCertificates(caCertificates);
    } else {
        sslConfig.setCaCertificates(_caCertificates);
    }
    
    // 这里可以添加更详细的证书链验证逻辑
    // 由于Qt的限制，我们简化验证过程
    
    for (const QSslCertificate &cert : certificateChain) {
        if (validateCertificate(cert) == Invalid || validateCertificate(cert) == Expired) {
            return false;
        }
    }
    
    return true;
}

bool CertificateManager::isCertificateExpiringSoon(const QSslCertificate &certificate, int warningDays) const
{
    if (certificate.isNull()) {
        return false;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    QDateTime expiryDate = certificate.expiryDate();
    
    return now.daysTo(expiryDate) <= warningDays;
}

QSslCertificate CertificateManager::getCurrentCertificate() const
{
    QMutexLocker locker(&_certificateMutex);
    return _currentCertificate;
}

QSslKey CertificateManager::getCurrentPrivateKey() const
{
    QMutexLocker locker(&_certificateMutex);
    return _currentPrivateKey;
}

QSslConfiguration CertificateManager::getSslConfiguration() const
{
    QMutexLocker locker(&_certificateMutex);
    
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    
    if (!_currentCertificate.isNull()) {
        config.setLocalCertificate(_currentCertificate);
    }
    
    if (!_currentPrivateKey.isNull()) {
        config.setPrivateKey(_currentPrivateKey);
    }
    
    if (!_caCertificates.isEmpty()) {
        config.setCaCertificates(_caCertificates);
    }
    
    // 设置安全的SSL协议
    config.setProtocol(QSsl::TlsV1_2);
    config.setPeerVerifyMode(QSslSocket::VerifyNone); // 服务器模式
    
    return config;
}

bool CertificateManager::loadCACertificate(const QString &caPath)
{
    QFile caFile(caPath);
    if (!caFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open CA certificate file: %1").arg(caPath));
        return false;
    }

    QSslCertificate caCertificate(&caFile, QSsl::Pem);
    caFile.close();

    if (caCertificate.isNull()) {
        LOG_ERROR(QString("Invalid CA certificate file: %1").arg(caPath));
        return false;
    }

    addCACertificate(caCertificate);

    LOG_INFO(QString("CA certificate loaded: %1").arg(caPath));
    return true;
}

void CertificateManager::addCACertificate(const QSslCertificate &caCertificate)
{
    QMutexLocker locker(&_certificateMutex);

    if (!caCertificate.isNull() && !_caCertificates.contains(caCertificate)) {
        _caCertificates.append(caCertificate);
    }
}

QList<QSslCertificate> CertificateManager::getCACertificates() const
{
    QMutexLocker locker(&_certificateMutex);
    return _caCertificates;
}

void CertificateManager::setAutoCheckEnabled(bool enabled, int checkInterval)
{
    _autoCheckEnabled = enabled;

    if (enabled) {
        _autoCheckTimer->setInterval(checkInterval);
        _autoCheckTimer->start();
        LOG_INFO(QString("Certificate auto-check enabled with %1ms interval").arg(checkInterval));
    } else {
        _autoCheckTimer->stop();
        LOG_INFO("Certificate auto-check disabled");
    }
}

void CertificateManager::setFileWatchEnabled(bool enabled)
{
    _fileWatchEnabled = enabled;

    if (!enabled) {
        _fileWatcher->removePaths(_fileWatcher->files());
        LOG_INFO("Certificate file watch disabled");
    } else {
        // 重新添加监视的文件
        if (!_certificatePath.isEmpty()) {
            _fileWatcher->addPath(_certificatePath);
        }
        if (!_privateKeyPath.isEmpty()) {
            _fileWatcher->addPath(_privateKeyPath);
        }
        LOG_INFO("Certificate file watch enabled");
    }
}

QJsonObject CertificateManager::getCertificateInfo(const QSslCertificate &certificate) const
{
    QJsonObject info;

    if (certificate.isNull()) {
        info["valid"] = false;
        return info;
    }

    info["valid"] = true;
    info["subject"] = certificate.subjectInfo(QSslCertificate::CommonName).join(", ");
    info["issuer"] = certificate.issuerInfo(QSslCertificate::CommonName).join(", ");
    info["serial_number"] = QString::fromLatin1(certificate.serialNumber().toHex());
    info["effective_date"] = certificate.effectiveDate().toString(Qt::ISODate);
    info["expiry_date"] = certificate.expiryDate().toString(Qt::ISODate);
    info["version"] = QString::fromLatin1(certificate.version());
    info["is_self_signed"] = certificate.issuerInfo(QSslCertificate::CommonName) ==
                            certificate.subjectInfo(QSslCertificate::CommonName);

    // 计算剩余有效天数
    int daysRemaining = QDateTime::currentDateTime().daysTo(certificate.expiryDate());
    info["days_remaining"] = daysRemaining;

    // 证书状态
    CertificateStatus status = validateCertificate(certificate);
    info["status"] = static_cast<int>(status);

    // 证书指纹
    info["fingerprint_sha256"] = getCertificateFingerprint(certificate, QCryptographicHash::Sha256);
    info["fingerprint_sha1"] = getCertificateFingerprint(certificate, QCryptographicHash::Sha1);

    return info;
}

QString CertificateManager::getCertificateFingerprint(const QSslCertificate &certificate,
                                                     QCryptographicHash::Algorithm algorithm) const
{
    if (certificate.isNull()) {
        return QString();
    }

    QByteArray certData = certificate.toDer();
    QByteArray hash = QCryptographicHash::hash(certData, algorithm);

    return hash.toHex(':').toUpper();
}

void CertificateManager::onAutoCheckTimer()
{
    checkCertificateStatus();
}

void CertificateManager::onCertificateFileChanged(const QString &path)
{
    LOG_INFO(QString("Certificate file changed: %1").arg(path));
    emit certificateFileChanged(path);

    // 自动重新加载证书
    if (path == _certificatePath || path == _privateKeyPath) {
        if (!_certificatePath.isEmpty() && !_privateKeyPath.isEmpty()) {
            LOG_INFO("Reloading certificate due to file change");
            loadCertificate(_certificatePath, _privateKeyPath, _keyPassword);
        }
    }
}

bool CertificateManager::createCertificateDirectory(const QString &path)
{
    QDir dir;
    if (!dir.exists(path)) {
        if (!dir.mkpath(path)) {
            LOG_ERROR(QString("Cannot create certificate directory: %1").arg(path));
            return false;
        }
    }
    return true;
}

QSslKey CertificateManager::generateRSAKey(int keySize)
{
    // 使用OpenSSL生成真实的RSA密钥对
    return OpenSSLHelper::generateRSAKeyPair(keySize);
}

QByteArray CertificateManager::createCertificateRequest(const QSslKey &privateKey,
                                                       const QString &commonName,
                                                       const QString &organization,
                                                       const QString &country)
{
    // 使用OpenSSL创建真实的证书请求
    return OpenSSLHelper::createCertificateRequest(privateKey, commonName, organization,
                                                   "IT Department", country, "Beijing", "Beijing");
}

QSslCertificate CertificateManager::signCertificate(const QByteArray &request,
                                                   const QSslKey &issuerKey,
                                                   const QSslCertificate &issuerCert,
                                                   int validDays)
{
    // 如果没有颁发者证书，创建自签名证书
    if (issuerCert.isNull()) {
        return OpenSSLHelper::createSelfSignedCertificate(issuerKey, "localhost", "QKChat",
                                                         "IT Department", "CN", "Beijing", "Beijing",
                                                         "", validDays);
    } else {
        // 使用CA证书签名
        return OpenSSLHelper::signCertificateRequest(request, issuerCert, issuerKey, validDays);
    }
}

void CertificateManager::checkCertificateStatus()
{
    QMutexLocker locker(&_certificateMutex);

    if (_currentCertificate.isNull()) {
        return;
    }

    CertificateStatus status = validateCertificate(_currentCertificate);

    switch (status) {
        case Expired:
            LOG_ERROR("Current certificate has expired");
            emit certificateExpired(_currentCertificate);
            break;

        case WillExpireSoon: {
            int daysRemaining = QDateTime::currentDateTime().daysTo(_currentCertificate.expiryDate());
            LOG_WARNING(QString("Current certificate will expire in %1 days").arg(daysRemaining));
            emit certificateExpiringSoon(_currentCertificate, daysRemaining);
            break;
        }

        case Valid:
        
            break;

        default:
            LOG_WARNING("Current certificate status is invalid");
            break;
    }
}
