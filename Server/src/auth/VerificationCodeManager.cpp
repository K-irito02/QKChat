#include "VerificationCodeManager.h"
#include "../utils/Logger.h"
#include "../utils/Crypto.h"
#include "../utils/Validator.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QRandomGenerator>

VerificationCodeManager::VerificationCodeManager(QObject *parent)
    : QObject(parent)
    , _databaseManager(DatabaseManager::instance())
    , _redisClient(RedisClient::instance())
{
}

VerificationCodeManager::~VerificationCodeManager()
{
}

VerificationCodeManager* VerificationCodeManager::instance()
{
    static QMutex instanceMutex;
    static QAtomicPointer<VerificationCodeManager> instance;
    
    // 双重检查锁定模式，确保线程安全
    VerificationCodeManager* tmp = instance.loadAcquire();
    if (!tmp) {
        QMutexLocker locker(&instanceMutex);
        tmp = instance.loadAcquire();
        if (!tmp) {
            tmp = new VerificationCodeManager();
            instance.storeRelease(tmp);
        }
    }
    return tmp;
}

QString VerificationCodeManager::generateAndSaveCode(const QString &email, const QString &ipAddress, CodeType codeType, int expireMinutes)
{
    QMutexLocker locker(&_mutex);
    
    // 检查邮箱频率限制
    if (!isAllowedToSend(email, 60)) { // 默认60秒间隔
        LOG_WARNING(QString("Rate limited for email: %1").arg(email));
        return QString();
    }
    
    // 检查IP地址频率限制
    if (!isIPAllowedToSend(ipAddress, 30)) { // IP地址限制30秒间隔
        LOG_WARNING(QString("Rate limited for IP: %1").arg(ipAddress));
        return QString();
    }
    
    // 处理旧验证码：标记为已使用，避免覆盖问题
    invalidateOldCodes(email, codeType);
    
    // 生成验证码
    QString code = generateCode();
    if (code.isEmpty()) {
        LOG_ERROR("Failed to generate verification code");
        return QString();
    }
    
    // 验证生成的验证码格式
    if (!Validator::isValidVerificationCode(code, 6)) {
        LOG_ERROR(QString("Generated invalid verification code: %1").arg(code));
        return QString();
    }
    
    // 保存到数据库
    if (!saveToDatabase(email, code, codeType, expireMinutes)) {
        LOG_ERROR(QString("Failed to save verification code to database for email: %1").arg(email));
        return QString();
    }
    
    // 缓存到Redis
    if (!cacheToRedis(email, code, expireMinutes)) {
        LOG_WARNING(QString("Failed to cache verification code to Redis for email: %1").arg(email));
        // Redis缓存失败不影响整体流程
    }
    
    // 记录发送历史
    recordSendHistory(email);
    recordIPSendHistory(ipAddress);
    
    LOG_INFO(QString("Verification code generated and saved for email: %1, IP: %2, code: %3").arg(email).arg(ipAddress).arg(code));
    return code;
}

QString VerificationCodeManager::generateAndSaveCodeInternal(const QString &email, CodeType codeType, int expireMinutes)
{
    QMutexLocker locker(&_mutex);
    
    // 检查邮箱频率限制
    if (!isAllowedToSend(email, 60)) { // 默认60秒间隔
        LOG_WARNING(QString("Rate limited for email: %1").arg(email));
        return QString();
    }
    
    // 处理旧验证码：标记为已使用，避免覆盖问题
    invalidateOldCodes(email, codeType);
    
    // 生成验证码
    QString code = generateCode();
    if (code.isEmpty()) {
        LOG_ERROR("Failed to generate verification code");
        return QString();
    }
    
    // 验证生成的验证码格式
    if (!Validator::isValidVerificationCode(code, 6)) {
        LOG_ERROR(QString("Generated invalid verification code: %1").arg(code));
        return QString();
    }
    
    // 保存到数据库
    if (!saveToDatabase(email, code, codeType, expireMinutes)) {
        LOG_ERROR(QString("Failed to save verification code to database for email: %1").arg(email));
        return QString();
    }
    
    // 缓存到Redis
    if (!cacheToRedis(email, code, expireMinutes)) {
        LOG_WARNING(QString("Failed to cache verification code to Redis for email: %1").arg(email));
        // Redis缓存失败不影响整体流程
    }
    
    // 记录发送历史（只记录邮箱，不记录IP）
    recordSendHistory(email);
    
    LOG_INFO(QString("Verification code generated and saved for email: %1, code: %2 (internal use)").arg(email).arg(code));
    return code;
}

VerificationCodeManager::VerificationResult VerificationCodeManager::verifyCode(const QString &email, const QString &code, CodeType codeType)
{
    QMutexLocker locker(&_mutex);
    
    LOG_INFO(QString("Starting verification code validation for email: %1, code: %2").arg(email).arg(code));
    
    // 验证输入格式
    if (!Validator::isValidVerificationCode(code, 6)) {
        LOG_WARNING(QString("Invalid verification code format for email: %1, code: %2").arg(email).arg(code));
        return InvalidCode;
    }
    
    // 1. 优先从Redis验证（性能更好）
    LOG_INFO(QString("Attempting Redis verification for email: %1").arg(email));
    VerificationResult redisResult = verifyFromRedis(email, code);
    LOG_INFO(QString("Redis verification result for email %1: %2").arg(email).arg((int)redisResult));
    
    if (redisResult == Success) {
        // 验证成功后标记为已使用
        LOG_INFO(QString("Redis verification successful, marking code as used for email: %1").arg(email));
        markCodeAsUsed(email, code, codeType);
        LOG_INFO(QString("Verification code validated from Redis for email: %1, code: %2").arg(email).arg(code));
        return Success;
    }
    
    // 2. Redis验证失败，从数据库验证（备用方案）
    LOG_INFO(QString("Redis verification failed, attempting database verification for email: %1").arg(email));
    VerificationResult dbResult = verifyFromDatabase(email, code, codeType);
    LOG_INFO(QString("Database verification result for email %1: %2").arg(email).arg((int)dbResult));
    
    if (dbResult == Success) {
        // 验证成功后标记为已使用
        LOG_INFO(QString("Database verification successful, marking code as used for email: %1").arg(email));
        markCodeAsUsed(email, code, codeType);
        LOG_INFO(QString("Verification code validated from database for email: %1, code: %2").arg(email).arg(code));
        return Success;
    }
    
    // 记录验证失败的具体原因
    QString errorMsg = getVerificationResultDescription(dbResult != DatabaseError ? dbResult : redisResult);
    LOG_WARNING(QString("Verification code validation failed for email: %1, code: %2 - %3").arg(email).arg(code).arg(errorMsg));
    
    // 返回具体的错误类型
    return dbResult != DatabaseError ? dbResult : redisResult;
}

int VerificationCodeManager::cleanupExpiredCodes()
{
    QMutexLocker locker(&_mutex);
    
    QString sql = "DELETE FROM verification_codes WHERE expires_at < NOW()";
    int deleted = _databaseManager->executeUpdate(sql);
    
    if (deleted > 0) {
        LOG_INFO(QString("Cleaned up %1 expired verification codes").arg(deleted));
    }
    
    return deleted;
}

QString VerificationCodeManager::getVerificationResultDescription(VerificationResult result)
{
    switch (result) {
        case Success:
            return "验证码验证成功";
        case InvalidCode:
            return "验证码无效";
        case ExpiredCode:
            return "验证码已过期";
        case AlreadyUsed:
            return "验证码已被使用";
        case DatabaseError:
            return "数据库错误";
        case RedisError:
            return "Redis缓存错误";
        default:
            return "未知错误";
    }
}

bool VerificationCodeManager::isAllowedToSend(const QString &email, int minIntervalSeconds)
{
    if (!_lastSendTime.contains(email)) {
        return true;
    }
    
    QDateTime lastSend = _lastSendTime[email];
    QDateTime now = QDateTime::currentDateTime();
    
    return lastSend.secsTo(now) >= minIntervalSeconds;
}

int VerificationCodeManager::getRemainingWaitTime(const QString &email, int minIntervalSeconds)
{
    if (!_lastSendTime.contains(email)) {
        return 0; // 可以立即发送
    }
    
    QDateTime lastSend = _lastSendTime[email];
    QDateTime now = QDateTime::currentDateTime();
    int elapsed = lastSend.secsTo(now);
    
    // 减少频率限制时间，从60秒改为30秒
    int actualInterval = qMin(minIntervalSeconds, 30);
    
    if (elapsed >= actualInterval) {
        return 0; // 可以立即发送
    }
    
    return actualInterval - elapsed; // 返回剩余等待时间
}

bool VerificationCodeManager::isIPAllowedToSend(const QString &ipAddress, int minIntervalSeconds)
{
    if (!_lastIPSendTime.contains(ipAddress)) {
        return true;
    }
    
    QDateTime lastSend = _lastIPSendTime[ipAddress];
    QDateTime now = QDateTime::currentDateTime();
    
    return lastSend.secsTo(now) >= minIntervalSeconds;
}

int VerificationCodeManager::getIPRemainingWaitTime(const QString &ipAddress, int minIntervalSeconds)
{
    if (!_lastIPSendTime.contains(ipAddress)) {
        return 0; // 可以立即发送
    }
    
    QDateTime lastSend = _lastIPSendTime[ipAddress];
    QDateTime now = QDateTime::currentDateTime();
    int elapsed = lastSend.secsTo(now);
    
    if (elapsed >= minIntervalSeconds) {
        return 0; // 可以立即发送
    }
    
    return minIntervalSeconds - elapsed; // 返回剩余等待时间
}

QString VerificationCodeManager::generateCode()
{
    // 生成6位数字验证码
    QString code;
    for (int i = 0; i < 6; ++i) {
        code += QString::number(QRandomGenerator::global()->bounded(10));
    }
    return code;
}

bool VerificationCodeManager::saveToDatabase(const QString &email, const QString &code, CodeType codeType, int expireMinutes)
{
    QString sql = R"(
        INSERT INTO verification_codes (email, code, type, expires_at, created_at)
        VALUES (?, ?, ?, DATE_ADD(NOW(), INTERVAL ? MINUTE), NOW())
    )";
    
    QVariantList params = {email, code, codeTypeToString(codeType), expireMinutes};
    int result = _databaseManager->executeUpdate(sql, params);
    
    return result > 0;
}

bool VerificationCodeManager::cacheToRedis(const QString &email, const QString &code, int expireMinutes)
{
    if (!_redisClient || !_redisClient->isConnected()) {
        return false;
    }
    
    // 先删除旧的验证码缓存
    _redisClient->deleteVerificationCode(email);
    
    // 缓存新的验证码
    RedisClient::Result result = _redisClient->setVerificationCode(email, code, expireMinutes);
    return result == RedisClient::Success;
}

VerificationCodeManager::VerificationResult VerificationCodeManager::verifyFromRedis(const QString &email, const QString &code)
{
    if (!_redisClient || !_redisClient->isConnected()) {
        LOG_WARNING(QString("Redis not connected for verification, email: %1").arg(email));
        return RedisError;
    }
    
    QString cachedCode;
    RedisClient::Result result = _redisClient->getVerificationCode(email, cachedCode);
    
    LOG_INFO(QString("Redis getVerificationCode result for email %1: %2, cached code: %3").arg(email).arg((int)result).arg(cachedCode));
    
    if (result != RedisClient::Success) {
        LOG_WARNING(QString("Redis getVerificationCode failed for email: %1, result: %2").arg(email).arg((int)result));
        return RedisError;
    }
    
    if (cachedCode.isEmpty()) {
        LOG_WARNING(QString("Redis cached code is empty for email: %1").arg(email));
        return InvalidCode;
    }
    
    if (cachedCode == code) {
        LOG_INFO(QString("Redis code match successful for email: %1, code: %2").arg(email).arg(code));
        return Success;
    }
    
    LOG_WARNING(QString("Redis code mismatch for email: %1, expected: %2, actual: %3").arg(email).arg(code).arg(cachedCode));
    return InvalidCode;
}

VerificationCodeManager::VerificationResult VerificationCodeManager::verifyFromDatabase(const QString &email, const QString &code, CodeType codeType)
{
    QString sql = R"(
        SELECT id, expires_at, used_at FROM verification_codes
        WHERE email = ? AND code = ? AND type = ?
        ORDER BY created_at DESC LIMIT 1
    )";
    
    QVariantList params = {email, code, codeTypeToString(codeType)};
    LOG_INFO(QString("Database verification query for email: %1, code: %2, type: %3").arg(email).arg(code).arg(codeTypeToString(codeType)));
    
    QSqlQuery query = _databaseManager->executeQuery(sql, params);
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error verifying code for email %1: %2").arg(email).arg(query.lastError().text()));
        return DatabaseError;
    }
    
    if (!query.next()) {
        LOG_WARNING(QString("No verification code found in database for email: %1, code: %2").arg(email).arg(code));
        return InvalidCode;
    }
    
    // 检查是否已使用
    if (!query.value("used_at").isNull()) {
        LOG_WARNING(QString("Verification code already used for email: %1, code: %2").arg(email).arg(code));
        return AlreadyUsed;
    }
    
    // 检查是否过期
    QDateTime expiresAt = query.value("expires_at").toDateTime();
    QDateTime now = QDateTime::currentDateTime();
    if (expiresAt < now) {
        LOG_WARNING(QString("Verification code expired for email: %1, code: %2, expires_at: %3, now: %4").arg(email).arg(code).arg(expiresAt.toString()).arg(now.toString()));
        return ExpiredCode;
    }
    
    LOG_INFO(QString("Database verification successful for email: %1, code: %2").arg(email).arg(code));
    return Success;
}

bool VerificationCodeManager::markCodeAsUsed(const QString &email, const QString &code, CodeType codeType)
{
    LOG_INFO(QString("Marking verification code as used for email: %1, code: %2").arg(email).arg(code));
    
    // 标记数据库中的验证码为已使用
    QString sql = "UPDATE verification_codes SET used_at = NOW() WHERE email = ? AND code = ? AND type = ? AND used_at IS NULL";
    QVariantList params = {email, code, codeTypeToString(codeType)};
    int result = _databaseManager->executeUpdate(sql, params);
    
    LOG_INFO(QString("Database markCodeAsUsed result for email %1: %2 rows affected").arg(email).arg(result));
    
    // 从Redis缓存中删除验证码
    if (_redisClient && _redisClient->isConnected()) {
        RedisClient::Result redisResult = _redisClient->deleteVerificationCode(email);
        LOG_INFO(QString("Redis deleteVerificationCode result for email %1: %2").arg(email).arg((int)redisResult));
    } else {
        LOG_WARNING(QString("Redis not connected, cannot delete verification code for email: %1").arg(email));
    }
    
    return result > 0;
}

QString VerificationCodeManager::codeTypeToString(CodeType codeType)
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

void VerificationCodeManager::recordSendHistory(const QString &email)
{
    _lastSendTime[email] = QDateTime::currentDateTime();
}

void VerificationCodeManager::recordIPSendHistory(const QString &ipAddress)
{
    _lastIPSendTime[ipAddress] = QDateTime::currentDateTime();
}

void VerificationCodeManager::invalidateOldCodes(const QString &email, CodeType codeType)
{
    // 标记数据库中未使用的旧验证码为已使用
    QString sql = "UPDATE verification_codes SET used_at = NOW() WHERE email = ? AND type = ? AND used_at IS NULL";
    int updated = _databaseManager->executeUpdate(sql, {email, codeTypeToString(codeType)});
    
    if (updated > 0) {
        LOG_INFO(QString("Invalidated %1 old verification codes for email: %2").arg(updated).arg(email));
    }
    
    // 从Redis缓存中删除旧验证码
    if (_redisClient && _redisClient->isConnected()) {
        _redisClient->deleteVerificationCode(email);
    }
} 
