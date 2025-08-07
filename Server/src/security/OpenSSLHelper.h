#ifndef OPENSSLHELPER_H
#define OPENSSLHELPER_H

#include <QString>
#include <QByteArray>
#include <QSslKey>
#include <QSslCertificate>

// OpenSSL前向声明（仅用于Qt类型转换）
typedef struct evp_pkey_st EVP_PKEY;
typedef struct bignum_st BIGNUM;

/**
 * @brief OpenSSL辅助工具类
 * 
 * 提供真实的RSA密钥生成、证书请求创建和证书签名功能。
 * 使用OpenSSL库实现加密操作。
 */
class OpenSSLHelper
{
public:
    /**
     * @brief 初始化OpenSSL库
     * @return 初始化是否成功
     */
    static bool initializeOpenSSL();
    
    /**
     * @brief 清理OpenSSL库
     */
    static void cleanupOpenSSL();
    
    /**
     * @brief 生成RSA密钥对
     * @param keySize RSA密钥大小（位）
     * @return Qt SSL私钥对象
     */
    static QSslKey generateRSAKeyPair(int keySize = 2048);
    
    /**
     * @brief 创建证书签名请求（CSR）
     * @param privateKey 私钥
     * @param commonName 通用名称
     * @param organization 组织名称
     * @param organizationalUnit 组织单位
     * @param country 国家代码
     * @param state 州/省
     * @param city 城市
     * @param email 邮箱地址
     * @return CSR的PEM格式数据
     */
    static QByteArray createCertificateRequest(const QSslKey &privateKey,
                                              const QString &commonName,
                                              const QString &organization = "",
                                              const QString &organizationalUnit = "",
                                              const QString &country = "",
                                              const QString &state = "",
                                              const QString &city = "",
                                              const QString &email = "");
    
    /**
     * @brief 创建自签名证书
     * @param privateKey 私钥
     * @param commonName 通用名称
     * @param organization 组织名称
     * @param organizationalUnit 组织单位
     * @param country 国家代码
     * @param state 州/省
     * @param city 城市
     * @param email 邮箱地址
     * @param validDays 有效天数
     * @param serialNumber 序列号
     * @return Qt SSL证书对象
     */
    static QSslCertificate createSelfSignedCertificate(const QSslKey &privateKey,
                                                      const QString &commonName,
                                                      const QString &organization = "QKChat",
                                                      const QString &organizationalUnit = "IT Department",
                                                      const QString &country = "CN",
                                                      const QString &state = "Beijing",
                                                      const QString &city = "Beijing",
                                                      const QString &email = "",
                                                      int validDays = 365,
                                                      long serialNumber = 1);
    
    /**
     * @brief 使用CA证书签名CSR
     * @param csr 证书签名请求
     * @param caCert CA证书
     * @param caPrivateKey CA私钥
     * @param validDays 有效天数
     * @param serialNumber 序列号
     * @return 签名后的证书
     */
    static QSslCertificate signCertificateRequest(const QByteArray &csr,
                                                 const QSslCertificate &caCert,
                                                 const QSslKey &caPrivateKey,
                                                 int validDays = 365,
                                                 long serialNumber = 1);
    
    /**
     * @brief 验证证书签名
     * @param certificate 要验证的证书
     * @param caCertificate CA证书
     * @return 验证是否成功
     */
    static bool verifyCertificateSignature(const QSslCertificate &certificate,
                                          const QSslCertificate &caCertificate);
    
    /**
     * @brief 获取证书的公钥指纹
     * @param certificate 证书
     * @return SHA256指纹
     */
    static QString getCertificateFingerprint(const QSslCertificate &certificate);
    
    /**
     * @brief 检查私钥和证书是否匹配
     * @param privateKey 私钥
     * @param certificate 证书
     * @return 是否匹配
     */
    static bool isKeyPairMatching(const QSslKey &privateKey, const QSslCertificate &certificate);

private:
    /**
     * @brief 将EVP_PKEY转换为QSslKey
     * @param pkey OpenSSL密钥对象
     * @return Qt SSL密钥对象
     */
    static QSslKey evpKeyToQSslKey(EVP_PKEY *pkey);
    
    /**
     * @brief 将QSslKey转换为EVP_PKEY
     * @param sslKey Qt SSL密钥对象
     * @return OpenSSL密钥对象
     */
    static EVP_PKEY* qSslKeyToEvpKey(const QSslKey &sslKey);
    
    /**
     * @brief 将X509转换为QSslCertificate
     * @param x509 OpenSSL证书对象
     * @return Qt SSL证书对象
     */
    static QSslCertificate x509ToQSslCertificate(void *x509);
    
    /**
     * @brief 将QSslCertificate转换为X509
     * @param certificate Qt SSL证书对象
     * @return OpenSSL证书对象
     */
    static void* qSslCertificateToX509(const QSslCertificate &certificate);
    
    /**
     * @brief 设置证书主题信息
     * @param name X509_NAME对象
     * @param commonName 通用名称
     * @param organization 组织名称
     * @param organizationalUnit 组织单位
     * @param country 国家代码
     * @param state 州/省
     * @param city 城市
     * @param email 邮箱地址
     * @return 设置是否成功
     */
    static bool setX509Name(void *name,
                           const QString &commonName,
                           const QString &organization,
                           const QString &organizationalUnit,
                           const QString &country,
                           const QString &state,
                           const QString &city,
                           const QString &email);
    
    /**
     * @brief 添加证书扩展
     * @param cert 证书对象
     * @param nid 扩展类型
     * @param value 扩展值
     * @return 添加是否成功
     */
    static bool addCertificateExtension(void *cert, int nid, const char *value);

private:
    static bool s_initialized;
};

#endif // OPENSSLHELPER_H
