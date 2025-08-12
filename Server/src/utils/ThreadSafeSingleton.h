#ifndef THREADSAFESINGLETON_H
#define THREADSAFESINGLETON_H

#include <QMutex>
#include <QMutexLocker>
#include <QAtomicPointer>
#include <QReadWriteLock>

/**
 * @brief 线程安全的单例模板类
 * 
 * 使用双重检查锁定模式确保单例的线程安全性，
 * 避免多线程环境下的竞态条件和重复创建问题。
 * 
 * 使用方法：
 * class MyClass : public ThreadSafeSingleton<MyClass> {
 *     friend class ThreadSafeSingleton<MyClass>;
 * private:
 *     MyClass() = default;
 * };
 */
template<typename T>
class ThreadSafeSingleton
{
public:
    /**
     * @brief 获取单例实例
     * @return 单例实例指针
     */
    static T* instance()
    {
        T* tmp = s_instance.loadAcquire();
        if (!tmp) {
            QMutexLocker locker(&s_mutex);
            tmp = s_instance.loadAcquire();
            if (!tmp) {
                tmp = new T();
                s_instance.storeRelease(tmp);
            }
        }
        return tmp;
    }

    /**
     * @brief 销毁单例实例
     * 注意：通常不需要手动调用，程序结束时会自动清理
     */
    static void destroyInstance()
    {
        QMutexLocker locker(&s_mutex);
        T* tmp = s_instance.loadAcquire();
        if (tmp) {
            s_instance.storeRelease(nullptr);
            delete tmp;
        }
    }

protected:
    ThreadSafeSingleton() = default;
    virtual ~ThreadSafeSingleton() = default;

    // 禁止拷贝和赋值
    ThreadSafeSingleton(const ThreadSafeSingleton&) = delete;
    ThreadSafeSingleton& operator=(const ThreadSafeSingleton&) = delete;
    ThreadSafeSingleton(ThreadSafeSingleton&&) = delete;
    ThreadSafeSingleton& operator=(ThreadSafeSingleton&&) = delete;

private:
    static QAtomicPointer<T> s_instance;
    static QMutex s_mutex;
};

// 静态成员定义
template<typename T>
QAtomicPointer<T> ThreadSafeSingleton<T>::s_instance;

template<typename T>
QMutex ThreadSafeSingleton<T>::s_mutex;

/**
 * @brief 资源管理辅助类
 * 
 * 用于自动管理资源的生命周期，确保在作用域结束时自动释放资源。
 * 特别适用于数据库连接、文件句柄等需要确保释放的资源。
 */
template<typename T>
class ScopeGuard
{
public:
    explicit ScopeGuard(T&& func) : _func(std::move(func)), _active(true) {}
    
    ~ScopeGuard()
    {
        if (_active) {
            _func();
        }
    }
    
    void dismiss() { _active = false; }
    
    // 禁止拷贝和赋值
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
    // 允许移动
    ScopeGuard(ScopeGuard&& other) noexcept 
        : _func(std::move(other._func)), _active(other._active)
    {
        other._active = false;
    }

private:
    T _func;
    bool _active;
};

/**
 * @brief 创建作用域守卫的辅助函数
 */
template<typename T>
ScopeGuard<T> makeScopeGuard(T&& func)
{
    return ScopeGuard<T>(std::forward<T>(func));
}

/**
 * @brief 线程安全的计数器
 */
class ThreadSafeCounter
{
public:
    ThreadSafeCounter(qint64 initialValue = 0) : _value(initialValue) {}
    
    qint64 increment() { return _value.fetchAndAddOrdered(1) + 1; }
    qint64 decrement() { return _value.fetchAndSubOrdered(1) - 1; }
    qint64 add(qint64 value) { return _value.fetchAndAddOrdered(value) + value; }
    qint64 subtract(qint64 value) { return _value.fetchAndSubOrdered(value) - value; }
    
    qint64 value() const { return _value.loadAcquire(); }
    void setValue(qint64 value) { _value.storeRelease(value); }
    
    bool compareAndSwap(qint64 expected, qint64 newValue) {
        return _value.testAndSetOrdered(expected, newValue);
    }

private:
    QAtomicInteger<qint64> _value;
};

/**
 * @brief 读写锁的RAII包装器
 */
class ReadLockGuard
{
public:
    explicit ReadLockGuard(QReadWriteLock& lock) : _lock(lock) {
        _lock.lockForRead();
    }
    
    ~ReadLockGuard() {
        _lock.unlock();
    }
    
    // 禁止拷贝和赋值
    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;

private:
    QReadWriteLock& _lock;
};

class WriteLockGuard
{
public:
    explicit WriteLockGuard(QReadWriteLock& lock) : _lock(lock) {
        _lock.lockForWrite();
    }
    
    ~WriteLockGuard() {
        _lock.unlock();
    }
    
    // 禁止拷贝和赋值
    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;

private:
    QReadWriteLock& _lock;
};

// 便利宏定义
#define READ_LOCK_GUARD(lock) ReadLockGuard _readGuard(lock)
#define WRITE_LOCK_GUARD(lock) WriteLockGuard _writeGuard(lock)
#define SCOPE_GUARD(func) auto _scopeGuard = makeScopeGuard(func)

#endif // THREADSAFESINGLETON_H