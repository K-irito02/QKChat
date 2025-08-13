#include "UserIdGenerator.h"
#include "../database/DatabaseManager.h"
#include "../database/DatabaseConnectionPool.h"
#include "../utils/Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMutexLocker>
#include <QVariant>

UserIdGenerator::UserIdGenerator(QObject* parent)
    : QObject(parent)
    , _databaseManager(DatabaseManager::instance())
    , _warningEmitted(0)
    , _criticalEmitted(0)
{
    // 初始化ID序列表
    if (!initializeSequenceTable()) {
        LOG_ERROR("Failed to initialize user ID sequence table");
    }
}

UserIdGenerator::~UserIdGenerator()
{
}

UserIdGenerator* UserIdGenerator::instance()
{
    return ThreadSafeSingleton<UserIdGenerator>::instance();
}

UserIdGenerator::GenerateResult UserIdGenerator::generateNextUserId(QString& userId)
{
    QMutexLocker locker(&_mutex);
    
    // LOG_INFO removed
    
    int nextId = 0;
    GenerateResult result = getNextIdFromDatabase(nextId);
    
    if (result != Success) {
        LOG_ERROR(QString("Failed to get next ID from database: %1").arg(getResultDescription(result)));
        return result;
    }
    
    // 检查是否超出最大值
    if (nextId > MAX_ID) {
        LOG_CRITICAL("User ID sequence exhausted! Maximum ID reached.");
        emit sequenceExhausted();
        return SequenceExhausted;
    }
    
    // 格式化为9位数字字符串
    userId = formatUserId(nextId);
    
    // 检查并发出警告
    checkAndEmitWarnings(nextId);
    
    LOG_INFO(QString("Generated user ID: %1 (sequence: %2)").arg(userId).arg(nextId));
    return Success;
}

bool UserIdGenerator::isUserIdExists(const QString& userId)
{
    if (!isValidUserIdFormat(userId)) {
        return false;
    }
    
    QString sql = "SELECT COUNT(*) FROM users WHERE user_id = ?";
    QSqlQuery query = _databaseManager->executeQuery(sql, {userId});
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error checking user ID existence: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }
    
    return false;
}

bool UserIdGenerator::getSequenceStatus(int& currentId, int& maxId, int& remainingCount)
{
    QMutexLocker locker(&_mutex);
    
    QString sql = "SELECT current_id, max_id FROM user_id_sequence WHERE id = 1";
    QSqlQuery query = _databaseManager->executeQuery(sql);
    
    if (query.lastError().isValid()) {
        LOG_ERROR(QString("Database error getting sequence status: %1").arg(query.lastError().text()));
        return false;
    }
    
    if (query.next()) {
        currentId = query.value("current_id").toInt();
        maxId = query.value("max_id").toInt();
        remainingCount = maxId - currentId;
        return true;
    }
    
    return false;
}

bool UserIdGenerator::resetSequence(int startId)
{
    QMutexLocker locker(&_mutex);
    
    if (startId < 0 || startId > MAX_ID) {
        LOG_ERROR(QString("Invalid start ID for sequence reset: %1").arg(startId));
        return false;
    }
    
    QString sql = "UPDATE user_id_sequence SET current_id = ?, updated_at = NOW() WHERE id = 1";
    int result = _databaseManager->executeUpdate(sql, {startId});
    
    if (result > 0) {
        LOG_WARNING(QString("User ID sequence reset to: %1").arg(startId));
        _warningEmitted.storeRelease(0);
        _criticalEmitted.storeRelease(0);
        return true;
    }
    
    return false;
}

QString UserIdGenerator::formatUserId(int idNumber)
{
    return QString("%1").arg(idNumber, 9, 10, QChar('0'));
}

bool UserIdGenerator::isValidUserIdFormat(const QString& userId)
{
    if (userId.length() != 9) {
        return false;
    }
    
    // 检查是否全为数字
    for (const QChar& ch : userId) {
        if (!ch.isDigit()) {
            return false;
        }
    }
    
    // 检查数值范围
    bool ok = false;
    int value = userId.toInt(&ok);
    return ok && value >= 0 && value <= MAX_ID;
}

QString UserIdGenerator::getResultDescription(GenerateResult result)
{
    switch (result) {
        case Success:
            return "成功生成用户ID";
        case DatabaseError:
            return "数据库操作错误";
        case SequenceExhausted:
            return "用户ID序列已耗尽";
        case ConcurrencyError:
            return "并发操作冲突";
        default:
            return "未知错误";
    }
}

UserIdGenerator::GenerateResult UserIdGenerator::getNextIdFromDatabase(int& nextId)
{
    // 使用数据库事务确保原子性
    bool success = _databaseManager->executeTransaction([this, &nextId](DatabaseConnection& connection) -> bool {
        // 获取当前ID并加锁（FOR UPDATE确保行级锁）
        QString selectSql = "SELECT current_id, max_id FROM user_id_sequence WHERE id = 1 FOR UPDATE";
        QSqlQuery selectQuery(connection.database());
        selectQuery.prepare(selectSql);
        
        if (!selectQuery.exec()) {
            LOG_ERROR(QString("Database error selecting current ID: %1").arg(selectQuery.lastError().text()));
            return false;
        }
        
        if (!selectQuery.next()) {
            LOG_ERROR("User ID sequence record not found");
            return false;
        }
        
        int currentId = selectQuery.value("current_id").toInt();
        int maxId = selectQuery.value("max_id").toInt();
        
        // 计算下一个ID
        nextId = currentId + 1;
        
        // 检查是否超出最大值
        if (nextId > maxId) {
            return false; // 序列耗尽
        }
        
        // 更新序列
        QString updateSql = "UPDATE user_id_sequence SET current_id = ?, updated_at = NOW() WHERE id = 1";
        QSqlQuery updateQuery(connection.database());
        updateQuery.prepare(updateSql);
        updateQuery.addBindValue(nextId);
        
        if (!updateQuery.exec()) {
            LOG_ERROR("Failed to update user ID sequence");
            return false;
        }
        
        return true;
    });
    
    if (!success) {
        LOG_ERROR("Failed to execute transaction for ID generation");
        return DatabaseError;
    }
    
    return Success;
}

bool UserIdGenerator::updateSequenceInDatabase(int newCurrentId)
{
    QString sql = "UPDATE user_id_sequence SET current_id = ?, updated_at = NOW() WHERE id = 1";
    int result = _databaseManager->executeUpdate(sql, {newCurrentId});
    return result > 0;
}

bool UserIdGenerator::initializeSequenceTable()
{
    // 检查序列表是否存在记录
    QString checkSql = "SELECT COUNT(*) FROM user_id_sequence WHERE id = 1";
    QSqlQuery checkQuery = _databaseManager->executeQuery(checkSql);
    
    if (checkQuery.lastError().isValid()) {
        LOG_ERROR(QString("Database error checking sequence table: %1").arg(checkQuery.lastError().text()));
        return false;
    }
    
    if (checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        // LOG_INFO removed
        return true;
    }
    
    // 初始化序列记录
    QString initSql = "INSERT INTO user_id_sequence (id, current_id, max_id) VALUES (1, 0, ?) ON DUPLICATE KEY UPDATE current_id=current_id";
    int result = _databaseManager->executeUpdate(initSql, {MAX_ID});
    
    if (result >= 0) {
    
        return true;
    }
    
    LOG_ERROR("Failed to initialize user ID sequence table");
    return false;
}

void UserIdGenerator::checkAndEmitWarnings(int currentId)
{
    int remaining = MAX_ID - currentId;
    
    // 发出严重警告（剩余100个以下）
    if (remaining <= CRITICAL_THRESHOLD && _criticalEmitted.loadAcquire() == 0) {
        if (_criticalEmitted.testAndSetAcquire(0, 1)) {
            LOG_CRITICAL(QString("CRITICAL: User ID sequence nearly exhausted! Only %1 IDs remaining.").arg(remaining));
            emit sequenceNearExhaustion(remaining);
        }
    }
    // 发出一般警告（剩余1000个以下）
    else if (remaining <= WARNING_THRESHOLD && _warningEmitted.loadAcquire() == 0) {
        if (_warningEmitted.testAndSetAcquire(0, 1)) {
            LOG_WARNING(QString("WARNING: User ID sequence running low! %1 IDs remaining.").arg(remaining));
            emit sequenceNearExhaustion(remaining);
        }
    }
}
