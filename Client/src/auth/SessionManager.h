#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QSettings>
#include <QPair>
#include <QMutex>
#include "../models/User.h"

/**
 * @brief 会话管理器类
 * 
 * 负责管理用户会话状态，包括登录状态、会话令牌、自动登录等功能。
 * 提供会话的创建、验证、刷新和销毁操作。
 */
class SessionManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginStateChanged)
    Q_PROPERTY(User* currentUser READ currentUser NOTIFY currentUserChanged)
    Q_PROPERTY(QString sessionToken READ sessionToken NOTIFY sessionTokenChanged)

public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager();
    
    /**
     * @brief 获取单例实例
     * @return 会话管理器实例
     */
    static SessionManager* instance();

    /**
     * @brief 清理单例实例
     * 在应用程序退出前调用，确保安全清理
     */
    static void cleanup();
    
    /**
     * @brief 创建用户会话
     * @param user 用户对象
     * @param sessionToken 会话令牌
     * @param rememberMe 是否记住登录
     * @return 创建是否成功
     */
    Q_INVOKABLE bool createSession(User* user, const QString &sessionToken, bool rememberMe = false);
    
    /**
     * @brief 销毁当前会话
     */
    Q_INVOKABLE void destroySession();
    
    /**
     * @brief 检查是否已登录
     * @return 是否已登录
     */
    bool isLoggedIn() const { return _isLoggedIn; }
    
    /**
     * @brief 获取当前用户
     * @return 当前用户对象
     */
    User* currentUser() const { return _currentUser; }
    
    /**
     * @brief 获取会话令牌
     * @return 会话令牌
     */
    QString sessionToken() const { return _sessionToken; }
    
    /**
     * @brief 获取登录时间
     * @return 登录时间
     */
    QDateTime loginTime() const { return _loginTime; }
    
    /**
     * @brief 检查会话是否有效
     * @return 会话是否有效
     */
    Q_INVOKABLE bool isSessionValid() const;
    
    /**
     * @brief 刷新会话令牌
     * @param newToken 新的会话令牌
     */
    Q_INVOKABLE void refreshSessionToken(const QString &newToken);
    
    /**
     * @brief 更新用户信息
     * @param user 更新的用户信息
     */
    Q_INVOKABLE void updateUserInfo(User* user);
    
    /**
     * @brief 设置会话超时时间
     * @param timeout 超时时间（秒）
     */
    Q_INVOKABLE void setSessionTimeout(int timeout);
    
    /**
     * @brief 检查是否启用了记住登录
     * @return 是否记住登录
     */
    Q_INVOKABLE bool isRememberMeEnabled() const { return _rememberMe; }
    
    /**
     * @brief 获取保存的登录信息
     * @return 用户名和密码哈希的键值对
     */
    Q_INVOKABLE QPair<QString, QString> getSavedLoginInfo() const;
    
    /**
     * @brief 保存登录信息
     * @param username 用户名
     * @param passwordHash 密码哈希
     */
    Q_INVOKABLE void saveLoginInfo(const QString &username, const QString &passwordHash);
    
    /**
     * @brief 清除保存的登录信息
     */
    Q_INVOKABLE void clearSavedLoginInfo();
    
    /**
     * @brief 尝试自动登录
     * @return 是否有保存的登录信息
     */
    Q_INVOKABLE bool tryAutoLogin();

signals:
    /**
     * @brief 登录状态改变信号
     * @param loggedIn 是否已登录
     */
    void loginStateChanged(bool loggedIn);
    
    /**
     * @brief 当前用户改变信号
     */
    void currentUserChanged();
    
    /**
     * @brief 会话令牌改变信号
     */
    void sessionTokenChanged();
    
    /**
     * @brief 会话即将过期信号
     * @param remainingSeconds 剩余秒数
     */
    void sessionExpiring(int remainingSeconds);
    
    /**
     * @brief 会话已过期信号
     */
    void sessionExpired();
    
    /**
     * @brief 需要自动登录信号
     * @param username 用户名
     * @param passwordHash 密码哈希
     */
    void autoLoginRequested(const QString &username, const QString &passwordHash);

private slots:
    void onSessionTimeout();
    void onSessionExpiringWarning();

private:
    /**
     * @brief 设置登录状态
     * @param loggedIn 是否已登录
     */
    void setLoggedIn(bool loggedIn);
    
    /**
     * @brief 启动会话定时器
     */
    void startSessionTimer();
    
    /**
     * @brief 停止会话定时器
     */
    void stopSessionTimer();
    
    /**
     * @brief 加载设置
     */
    void loadSettings();
    
    /**
     * @brief 保存设置
     */
    void saveSettings();

private:
    static SessionManager* s_instance;
    static QMutex s_mutex; // 添加静态互斥锁
    
    bool _isLoggedIn;
    User* _currentUser;
    QString _sessionToken;
    QDateTime _loginTime;
    bool _rememberMe;
    int _sessionTimeout; // 会话超时时间（秒）
    
    QTimer* _sessionTimer;
    QTimer* _expiringWarningTimer;
    QSettings* _settings;
};

#endif // SESSIONMANAGER_H
