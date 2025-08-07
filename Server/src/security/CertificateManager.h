#ifndef CERTIFICATEMANAGER_H
#define CERTIFICATEMANAGER_H

#include <QObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslConfiguration>
#include <QTimer>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QMutex>

/**
 * @brief TLS证书管理器类
 * 
 * 负责管理SSL/TLS证书的加载、验证、生成和自动更新。
 * 支持自签名证书生成、证书链验证、过期检查等功能。
 */
class CertificateManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 证书状态枚举
     */
    enum CertificateStatus {
        Valid,
        Expired,
        Invalid,
        NotFound,
        WillExpireSoon
    };
    Q_ENUM(CertificateStatus)

    /**
     * @brief 证书类型枚举
     */
    enum CertificateType {
        SelfSigned,
        CA,
        Server,
        Client
    };
    Q_ENUM(CertificateType)

    explicit CertificateManager(QObject *parent = nullptr);
    ~CertificateManager();
    
    /**
     * @brief 获取单例实例
     * @return 证书管理器实例
     */
    static CertificateManager* instance();
    
    /**
     * @brief 加载证书和私钥
     * @param certPath 证书文件路径
     * @param keyPath 私钥文件路径
     * @param keyPassword 私钥密码（可选）
     * @return 加载是否成功
     */
    bool loadCertificate(const QString &certPath, const QString &keyPath, const QString &keyPassword = "");
    
    /**
     * @brief 生成自签名证书
     * @param commonName 通用名称
     * @param organization 组织名称
     * @param country 国家代码
     * @param validDays 有效天数
     * @param keySize RSA密钥大小
     * @return 生成是否成功
     */
    bool generateSelfSignedCertificate(const QString &commonName, 
                                      const QString &organization = "QKChat",
                                      const QString &country = "CN",
                                      int validDays = 365,
                                      int keySize = 2048);
    
    /**
     * @brief 保存证书和私钥到文件
     * @param certPath 证书文件路径
     * @param keyPath 私钥文件路径
     * @param keyPassword 私钥密码（可选）
     * @return 保存是否成功
     */
    bool saveCertificate(const QString &certPath, const QString &keyPath, const QString &keyPassword = "");
    
    /**
     * @brief 验证证书
     * @param certificate 要验证的证书
     * @return 验证结果
     */
    CertificateStatus validateCertificate(const QSslCertificate &certificate) const;
    
    /**
     * @brief 验证证书链
     * @param certificateChain 证书链
     * @param caCertificates CA证书列表
     * @return 验证是否成功
     */
    bool validateCertificateChain(const QList<QSslCertificate> &certificateChain,
                                 const QList<QSslCertificate> &caCertificates = QList<QSslCertificate>()) const;
    
    /**
     * @brief 检查证书是否即将过期
     * @param certificate 证书
     * @param warningDays 提前警告天数
     * @return 是否即将过期
     */
    bool isCertificateExpiringSoon(const QSslCertificate &certificate, int warningDays = 30) const;
    
    /**
     * @brief 获取当前证书
     * @return 当前证书
     */
    QSslCertificate getCurrentCertificate() const;
    
    /**
     * @brief 获取当前私钥
     * @return 当前私钥
     */
    QSslKey getCurrentPrivateKey() const;
    
    /**
     * @brief 获取SSL配置
     * @return SSL配置对象
     */
    QSslConfiguration getSslConfiguration() const;
    
    /**
     * @brief 加载CA证书
     * @param caPath CA证书文件路径
     * @return 加载是否成功
     */
    bool loadCACertificate(const QString &caPath);
    
    /**
     * @brief 添加CA证书
     * @param caCertificate CA证书
     */
    void addCACertificate(const QSslCertificate &caCertificate);
    
    /**
     * @brief 获取CA证书列表
     * @return CA证书列表
     */
    QList<QSslCertificate> getCACertificates() const;
    
    /**
     * @brief 启用证书自动检查
     * @param enabled 是否启用
     * @param checkInterval 检查间隔（毫秒）
     */
    void setAutoCheckEnabled(bool enabled, int checkInterval = 86400000); // 24小时
    
    /**
     * @brief 启用证书文件监视
     * @param enabled 是否启用
     */
    void setFileWatchEnabled(bool enabled);
    
    /**
     * @brief 获取证书信息
     * @param certificate 证书
     * @return 证书信息JSON对象
     */
    QJsonObject getCertificateInfo(const QSslCertificate &certificate) const;
    
    /**
     * @brief 获取证书指纹
     * @param certificate 证书
     * @param algorithm 哈希算法
     * @return 证书指纹
     */
    QString getCertificateFingerprint(const QSslCertificate &certificate, 
                                     QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha256) const;

signals:
    /**
     * @brief 证书加载成功信号
     */
    void certificateLoaded();
    
    /**
     * @brief 证书即将过期信号
     * @param certificate 证书
     * @param daysRemaining 剩余天数
     */
    void certificateExpiringSoon(const QSslCertificate &certificate, int daysRemaining);
    
    /**
     * @brief 证书已过期信号
     * @param certificate 证书
     */
    void certificateExpired(const QSslCertificate &certificate);
    
    /**
     * @brief 证书文件改变信号
     * @param filePath 文件路径
     */
    void certificateFileChanged(const QString &filePath);
    
    /**
     * @brief 证书错误信号
     * @param error 错误信息
     */
    void certificateError(const QString &error);

private slots:
    void onAutoCheckTimer();
    void onCertificateFileChanged(const QString &path);

private:
    /**
     * @brief 创建证书目录
     * @param path 目录路径
     * @return 创建是否成功
     */
    bool createCertificateDirectory(const QString &path);
    
    /**
     * @brief 生成RSA密钥对
     * @param keySize 密钥大小
     * @return 私钥
     */
    QSslKey generateRSAKey(int keySize = 2048);
    
    /**
     * @brief 创建证书请求
     * @param privateKey 私钥
     * @param commonName 通用名称
     * @param organization 组织名称
     * @param country 国家代码
     * @return 证书请求
     */
    QByteArray createCertificateRequest(const QSslKey &privateKey,
                                       const QString &commonName,
                                       const QString &organization,
                                       const QString &country);
    
    /**
     * @brief 签名证书
     * @param request 证书请求
     * @param issuerKey 签发者私钥
     * @param issuerCert 签发者证书（自签名时为空）
     * @param validDays 有效天数
     * @return 签名后的证书
     */
    QSslCertificate signCertificate(const QByteArray &request,
                                   const QSslKey &issuerKey,
                                   const QSslCertificate &issuerCert = QSslCertificate(),
                                   int validDays = 365);
    
    /**
     * @brief 检查所有证书状态
     */
    void checkCertificateStatus();

private:
    static CertificateManager* s_instance;
    
    QSslCertificate _currentCertificate;
    QSslKey _currentPrivateKey;
    QList<QSslCertificate> _caCertificates;
    
    QString _certificatePath;
    QString _privateKeyPath;
    QString _keyPassword;
    
    QTimer* _autoCheckTimer;
    QFileSystemWatcher* _fileWatcher;
    
    bool _autoCheckEnabled;
    bool _fileWatchEnabled;
    
    mutable QMutex _certificateMutex;
};

#endif // CERTIFICATEMANAGER_H
