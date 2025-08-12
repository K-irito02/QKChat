#include "OpenSSLHelper.h"
#include "../utils/Logger.h"

// OpenSSL头文件包含顺序很重要，避免冲突
#include <openssl/opensslconf.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>


#include <QCryptographicHash>

// 静态成员初始化
bool OpenSSLHelper::s_initialized = false;

bool OpenSSLHelper::initializeOpenSSL()
{
    if (s_initialized) {
        return true;
    }
    
    // 初始化OpenSSL库
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // 初始化随机数生成器
    if (RAND_poll() == 0) {
        LOG_ERROR("Failed to initialize OpenSSL random number generator");
        return false;
    }
    
    s_initialized = true;

    
    return true;
}

void OpenSSLHelper::cleanupOpenSSL()
{
    if (!s_initialized) {
        return;
    }
    
    EVP_cleanup();
    ERR_free_strings();
    
    s_initialized = false;
    LOG_INFO("OpenSSL library cleaned up");
}

QSslKey OpenSSLHelper::generateRSAKeyPair(int keySize)
{
    if (!s_initialized && !initializeOpenSSL()) {
        LOG_ERROR("OpenSSL not initialized");
        return QSslKey();
    }
    
    LOG_INFO(QString("Generating RSA key pair with %1 bits").arg(keySize));
    
    // 创建EVP_PKEY上下文
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
        LOG_ERROR("Failed to create EVP_PKEY context");
        return QSslKey();
    }
    
    // 初始化密钥生成
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        LOG_ERROR("Failed to initialize key generation");
        EVP_PKEY_CTX_free(ctx);
        return QSslKey();
    }
    
    // 设置RSA密钥大小
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, keySize) <= 0) {
        LOG_ERROR("Failed to set RSA key size");
        EVP_PKEY_CTX_free(ctx);
        return QSslKey();
    }
    
    // 生成密钥对
    EVP_PKEY *pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        LOG_ERROR("Failed to generate RSA key pair");
        EVP_PKEY_CTX_free(ctx);
        return QSslKey();
    }
    
    EVP_PKEY_CTX_free(ctx);
    
    // 转换为QSslKey
    QSslKey sslKey = evpKeyToQSslKey(pkey);
    EVP_PKEY_free(pkey);
    
    if (sslKey.isNull()) {
        LOG_ERROR("Failed to convert EVP_PKEY to QSslKey");
        return QSslKey();
    }
    

    return sslKey;
}

QByteArray OpenSSLHelper::createCertificateRequest(const QSslKey &privateKey,
                                                   const QString &commonName,
                                                   const QString &organization,
                                                   const QString &organizationalUnit,
                                                   const QString &country,
                                                   const QString &state,
                                                   const QString &city,
                                                   const QString &email)
{
    if (!s_initialized && !initializeOpenSSL()) {
        LOG_ERROR("OpenSSL not initialized");
        return QByteArray();
    }
    
    LOG_INFO(QString("Creating certificate request for: %1").arg(commonName));
    
    // 转换私钥
    EVP_PKEY *pkey = qSslKeyToEvpKey(privateKey);
    if (!pkey) {
        LOG_ERROR("Failed to convert private key");
        return QByteArray();
    }
    
    // 创建证书请求
    X509_REQ *req = X509_REQ_new();
    if (!req) {
        LOG_ERROR("Failed to create X509_REQ");
        EVP_PKEY_free(pkey);
        return QByteArray();
    }
    
    // 设置版本
    X509_REQ_set_version(req, 0); // 版本1
    
    // 设置主题
    X509_NAME *name = X509_REQ_get_subject_name(req);
    if (!setX509Name(name, commonName, organization, organizationalUnit, 
                     country, state, city, email)) {
        LOG_ERROR("Failed to set certificate request subject");
        X509_REQ_free(req);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }
    
    // 设置公钥
    if (X509_REQ_set_pubkey(req, pkey) != 1) {
        LOG_ERROR("Failed to set public key in certificate request");
        X509_REQ_free(req);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }
    
    // 签名证书请求
    if (X509_REQ_sign(req, pkey, EVP_sha256()) == 0) {
        LOG_ERROR("Failed to sign certificate request");
        X509_REQ_free(req);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }
    
    // 转换为PEM格式
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio) {
        LOG_ERROR("Failed to create BIO");
        X509_REQ_free(req);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }
    
    if (PEM_write_bio_X509_REQ(bio, req) != 1) {
        LOG_ERROR("Failed to write certificate request to BIO");
        BIO_free(bio);
        X509_REQ_free(req);
        EVP_PKEY_free(pkey);
        return QByteArray();
    }
    
    // 读取PEM数据
    char *data;
    long len = BIO_get_mem_data(bio, &data);
    QByteArray pemData(data, len);
    
    // 清理资源
    BIO_free(bio);
    X509_REQ_free(req);
    EVP_PKEY_free(pkey);
    

    return pemData;
}

QSslCertificate OpenSSLHelper::createSelfSignedCertificate(const QSslKey &privateKey,
                                                          const QString &commonName,
                                                          const QString &organization,
                                                          const QString &organizationalUnit,
                                                          const QString &country,
                                                          const QString &state,
                                                          const QString &city,
                                                          const QString &email,
                                                          int validDays,
                                                          long serialNumber)
{
    if (!s_initialized && !initializeOpenSSL()) {
        LOG_ERROR("OpenSSL not initialized");
        return QSslCertificate();
    }
    
    LOG_INFO(QString("Creating self-signed certificate for: %1").arg(commonName));
    
    // 转换私钥
    EVP_PKEY *pkey = qSslKeyToEvpKey(privateKey);
    if (!pkey) {
        LOG_ERROR("Failed to convert private key");
        return QSslCertificate();
    }
    
    // 创建证书
    X509 *cert = X509_new();
    if (!cert) {
        LOG_ERROR("Failed to create X509 certificate");
        EVP_PKEY_free(pkey);
        return QSslCertificate();
    }
    
    // 设置版本（X.509 v3）
    X509_set_version(cert, 2);
    
    // 设置序列号
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serialNumber);
    
    // 设置有效期
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), validDays * 24 * 3600);
    
    // 设置公钥
    X509_set_pubkey(cert, pkey);
    
    // 设置主题和颁发者（自签名证书两者相同）
    X509_NAME *name = X509_get_subject_name(cert);
    if (!setX509Name(name, commonName, organization, organizationalUnit,
                     country, state, city, email)) {
        LOG_ERROR("Failed to set certificate subject");
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return QSslCertificate();
    }
    
    X509_set_issuer_name(cert, name);
    
    // 添加扩展
    addCertificateExtension(cert, NID_basic_constraints, "critical,CA:FALSE");
    addCertificateExtension(cert, NID_key_usage, "critical,digitalSignature,keyEncipherment,keyAgreement");
    addCertificateExtension(cert, NID_ext_key_usage, "serverAuth,clientAuth");
    addCertificateExtension(cert, NID_subject_alt_name, QString("DNS:%1,IP:127.0.0.1").arg(commonName).toUtf8().constData());
    
    // 添加签名算法扩展以提高兼容性
    // 注意：NID_signature_algorithms不是标准的X.509扩展，移除这个扩展
    // 签名算法在SSL握手时协商，不需要在证书中指定
    
    // 签名证书 - 使用SHA256以提高安全性和兼容性
    // 对于TLS 1.2，使用SHA256签名算法
    if (X509_sign(cert, pkey, EVP_sha256()) == 0) {
        LOG_ERROR("Failed to sign certificate with SHA256");
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return QSslCertificate();
    }
    
    // 转换为QSslCertificate
    QSslCertificate sslCert = x509ToQSslCertificate(cert);
    
    // 清理资源
    X509_free(cert);
    EVP_PKEY_free(pkey);
    
    if (sslCert.isNull()) {
        LOG_ERROR("Failed to convert X509 to QSslCertificate");
        return QSslCertificate();
    }
    

    return sslCert;
}

QSslKey OpenSSLHelper::evpKeyToQSslKey(EVP_PKEY *pkey)
{
    if (!pkey) {
        return QSslKey();
    }

    // 创建BIO用于输出PEM格式
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio) {
        return QSslKey();
    }

    // 写入私钥到BIO
    if (PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
        BIO_free(bio);
        return QSslKey();
    }

    // 读取PEM数据
    char *data;
    long len = BIO_get_mem_data(bio, &data);
    QByteArray pemData(data, len);

    BIO_free(bio);

    // 创建QSslKey
    return QSslKey(pemData, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
}

EVP_PKEY* OpenSSLHelper::qSslKeyToEvpKey(const QSslKey &sslKey)
{
    if (sslKey.isNull()) {
        return nullptr;
    }

    QByteArray pemData = sslKey.toPem();

    // 创建BIO从PEM数据读取
    BIO *bio = BIO_new_mem_buf(pemData.constData(), pemData.length());
    if (!bio) {
        return nullptr;
    }

    // 读取私钥
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    return pkey;
}

QSslCertificate OpenSSLHelper::x509ToQSslCertificate(void *x509_ptr)
{
    X509 *x509 = static_cast<X509*>(x509_ptr);
    if (!x509) {
        return QSslCertificate();
    }

    // 创建BIO用于输出PEM格式
    BIO *bio = BIO_new(BIO_s_mem());
    if (!bio) {
        return QSslCertificate();
    }

    // 写入证书到BIO
    if (PEM_write_bio_X509(bio, x509) != 1) {
        BIO_free(bio);
        return QSslCertificate();
    }

    // 读取PEM数据
    char *data;
    long len = BIO_get_mem_data(bio, &data);
    QByteArray pemData(data, len);

    BIO_free(bio);

    // 创建QSslCertificate
    return QSslCertificate(pemData, QSsl::Pem);
}

void* OpenSSLHelper::qSslCertificateToX509(const QSslCertificate &certificate)
{
    if (certificate.isNull()) {
        return nullptr;
    }

    QByteArray pemData = certificate.toPem();

    // 创建BIO从PEM数据读取
    BIO *bio = BIO_new_mem_buf(pemData.constData(), pemData.length());
    if (!bio) {
        return nullptr;
    }

    // 读取证书
    X509 *x509 = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    return static_cast<void*>(x509);
}

bool OpenSSLHelper::setX509Name(void *name_ptr,
                                const QString &commonName,
                                const QString &organization,
                                const QString &organizationalUnit,
                                const QString &country,
                                const QString &state,
                                const QString &city,
                                const QString &email)
{
    X509_NAME *name = static_cast<X509_NAME*>(name_ptr);

    if (!commonName.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(commonName.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    if (!organization.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(organization.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    if (!organizationalUnit.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(organizationalUnit.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    if (!country.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(country.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    if (!state.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(state.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    if (!city.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(city.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    if (!email.isEmpty()) {
        if (X509_NAME_add_entry_by_txt(name, "emailAddress", MBSTRING_ASC,
                                      reinterpret_cast<const unsigned char*>(email.toUtf8().constData()),
                                      -1, -1, 0) != 1) {
            return false;
        }
    }

    return true;
}

bool OpenSSLHelper::addCertificateExtension(void *cert_ptr, int nid, const char *value)
{
    X509 *cert = static_cast<X509*>(cert_ptr);
    X509_EXTENSION *ext = nullptr;
    X509V3_CTX ctx;

    // 初始化扩展上下文
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

    // 创建扩展
    ext = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
    if (!ext) {
        return false;
    }

    // 添加扩展到证书
    if (X509_add_ext(cert, ext, -1) != 1) {
        X509_EXTENSION_free(ext);
        return false;
    }

    X509_EXTENSION_free(ext);
    return true;
}

QSslCertificate OpenSSLHelper::signCertificateRequest(const QByteArray &csr,
                                                     const QSslCertificate &caCert,
                                                     const QSslKey &caPrivateKey,
                                                     int validDays,
                                                     long serialNumber)
{
    if (!s_initialized && !initializeOpenSSL()) {
        LOG_ERROR("OpenSSL not initialized");
        return QSslCertificate();
    }

    if (csr.isEmpty() || caCert.isNull() || caPrivateKey.isNull()) {
        LOG_ERROR("Invalid parameters for certificate signing");
        return QSslCertificate();
    }

    LOG_INFO("Signing certificate request");

    // 将Qt对象转换为OpenSSL对象
    EVP_PKEY *caPkey = qSslKeyToEvpKey(caPrivateKey);
    if (!caPkey) {
        LOG_ERROR("Failed to convert CA private key");
        return QSslCertificate();
    }

    X509 *caX509 = static_cast<X509*>(qSslCertificateToX509(caCert));
    if (!caX509) {
        LOG_ERROR("Failed to convert CA certificate");
        EVP_PKEY_free(caPkey);
        return QSslCertificate();
    }

    // 创建BIO从CSR数据读取
    BIO *bio = BIO_new_mem_buf(csr.constData(), csr.length());
    if (!bio) {
        LOG_ERROR("Failed to create BIO for CSR");
        EVP_PKEY_free(caPkey);
        X509_free(caX509);
        return QSslCertificate();
    }

    // 读取CSR
    X509_REQ *req = PEM_read_bio_X509_REQ(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!req) {
        LOG_ERROR("Failed to read CSR");
        EVP_PKEY_free(caPkey);
        X509_free(caX509);
        return QSslCertificate();
    }

    // 创建新证书
    X509 *cert = X509_new();
    if (!cert) {
        LOG_ERROR("Failed to create certificate");
        X509_REQ_free(req);
        EVP_PKEY_free(caPkey);
        X509_free(caX509);
        return QSslCertificate();
    }

    // 设置证书版本
    X509_set_version(cert, 2); // 版本3

    // 设置序列号
    ASN1_INTEGER_set(X509_get_serialNumber(cert), serialNumber);

    // 复制主题名称
    X509_set_subject_name(cert, X509_REQ_get_subject_name(req));

    // 设置颁发者名称（CA证书的主题名称）
    X509_set_issuer_name(cert, X509_get_subject_name(caX509));

    // 设置有效期
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), validDays * 24 * 60 * 60);

    // 复制公钥
    EVP_PKEY *pubkey = X509_REQ_get_pubkey(req);
    if (!pubkey) {
        LOG_ERROR("Failed to get public key from CSR");
        X509_free(cert);
        X509_REQ_free(req);
        EVP_PKEY_free(caPkey);
        X509_free(caX509);
        return QSslCertificate();
    }

    X509_set_pubkey(cert, pubkey);
    EVP_PKEY_free(pubkey);

    // 添加基本约束扩展
    addCertificateExtension(cert, NID_basic_constraints, "CA:FALSE");

    // 添加密钥用途扩展
    addCertificateExtension(cert, NID_key_usage, "digitalSignature,keyEncipherment");

    // 添加扩展密钥用途
    addCertificateExtension(cert, NID_ext_key_usage, "serverAuth,clientAuth");

    // 签名证书
    if (X509_sign(cert, caPkey, EVP_sha256()) == 0) {
        LOG_ERROR("Failed to sign certificate");
        X509_free(cert);
        X509_REQ_free(req);
        EVP_PKEY_free(caPkey);
        X509_free(caX509);
        return QSslCertificate();
    }

    // 转换为QSslCertificate
    QSslCertificate result = x509ToQSslCertificate(cert);

    // 清理资源
    X509_free(cert);
    X509_REQ_free(req);
    EVP_PKEY_free(caPkey);
    X509_free(caX509);

    if (result.isNull()) {
        LOG_ERROR("Failed to convert signed certificate");
        return QSslCertificate();
    }


    return result;
}
