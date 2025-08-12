#ifndef USERIDGENERATOR_H
#define USERIDGENERATOR_H

#include <QObject>
#include <QMutex>
#include <QString>
#include <QAtomicInt>
#include "../utils/ThreadSafeSingleton.h"

class DatabaseManager;

/**
 * @brief 用户ID生成器
 * 
 * 负责生成固定9位数字格式的用户ID，确保线程安全和唯一性
 * ID格式：000000000 - 999999999
 * 初始ID：000000000，后续递增
 */
class UserIdGenerator : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief ID生成结果枚举
     */
    enum GenerateResult {
        Success = 0,        // 成功生成
        DatabaseError,      // 数据库错误
        SequenceExhausted,  // ID序列已耗尽
        ConcurrencyError    // 并发冲突错误
    };

    /**
     * @brief 获取单例实例
     */
    static UserIdGenerator* instance();

    /**
     * @brief 生成下一个用户ID
     * @param userId 输出参数，生成的9位数字用户ID
     * @return 生成结果
     */
    GenerateResult generateNextUserId(QString& userId);

    /**
     * @brief 检查用户ID是否存在
     * @param userId 要检查的用户ID
     * @return true表示存在，false表示不存在
     */
    bool isUserIdExists(const QString& userId);

    /**
     * @brief 获取当前ID序列状态
     * @param currentId 当前ID序号
     * @param maxId 最大ID序号
     * @param remainingCount 剩余可用ID数量
     * @return true表示获取成功
     */
    bool getSequenceStatus(int& currentId, int& maxId, int& remainingCount);

    /**
     * @brief 重置ID序列（仅用于测试，生产环境慎用）
     * @param startId 起始ID序号
     * @return true表示重置成功
     */
    bool resetSequence(int startId = 0);

    /**
     * @brief 格式化ID序号为9位数字字符串
     * @param idNumber ID序号
     * @return 格式化后的9位数字字符串
     */
    static QString formatUserId(int idNumber);

    /**
     * @brief 验证用户ID格式是否正确
     * @param userId 要验证的用户ID
     * @return true表示格式正确
     */
    static bool isValidUserIdFormat(const QString& userId);

    /**
     * @brief 获取生成结果的描述信息
     * @param result 生成结果
     * @return 描述信息
     */
    static QString getResultDescription(GenerateResult result);

signals:
    /**
     * @brief ID序列即将耗尽警告信号
     * @param remainingCount 剩余可用ID数量
     */
    void sequenceNearExhaustion(int remainingCount);

    /**
     * @brief ID序列已耗尽信号
     */
    void sequenceExhausted();

private:
    explicit UserIdGenerator(QObject* parent = nullptr);
    ~UserIdGenerator();

    /**
     * @brief 从数据库获取下一个ID序号
     * @param nextId 输出参数，下一个ID序号
     * @return 生成结果
     */
    GenerateResult getNextIdFromDatabase(int& nextId);

    /**
     * @brief 更新数据库中的ID序列
     * @param newCurrentId 新的当前ID序号
     * @return true表示更新成功
     */
    bool updateSequenceInDatabase(int newCurrentId);

    /**
     * @brief 初始化ID序列表
     * @return true表示初始化成功
     */
    bool initializeSequenceTable();

    /**
     * @brief 检查并发出警告信号
     * @param currentId 当前ID序号
     */
    void checkAndEmitWarnings(int currentId);

private:
    DatabaseManager* _databaseManager;
    mutable QMutex _mutex;
    
    // 缓存配置
    static const int MAX_ID = 999999999;        // 最大ID序号
    static const int WARNING_THRESHOLD = 1000;  // 警告阈值
    static const int CRITICAL_THRESHOLD = 100;  // 严重警告阈值
    
    // 状态标志
    QAtomicInt _warningEmitted;     // 是否已发出警告
    QAtomicInt _criticalEmitted;    // 是否已发出严重警告
    
    friend class ThreadSafeSingleton<UserIdGenerator>;
};

#endif // USERIDGENERATOR_H