#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QPair>
#include <QMap>
#include <QJsonObject>
#include <QMutex>
#include "NetworkClient.h"
#include "SessionManager.h"
#include "../models/User.h"
#include "../models/AuthResponse.h"

/**
 * @brief 认证管理器类
 * 
 * 客户端认证系统的核心类，负责协调网络通信、会话管理、数据验证等功能。
 * 提供登录、注册、验证码发送等认证相关的高级接口。
 */
class AuthManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginStateChanged)
    Q_PROPERTY(User* currentUser READ currentUser NOTIFY currentUserChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingStateChanged)

public:
    /**
     * @brief 认证状态枚举
     */
    enum AuthState {
        Idle,
        Connecting,
        LoggingIn,
        Registering,
        SendingVerificationCode,
        LoggingOut
    };
    Q_ENUM(AuthState)

    explicit AuthManager(QObject *parent = nullptr);
    ~AuthManager();
    
    /**
     * @brief 获取单例实例
     * @return 认证管理器实例
     */
    static AuthManager* instance();
    
    /**
     * @brief 初始化认证管理器
     * @param serverHost 服务器地址
     * @param serverPort 服务器端口
     * @param useTLS 是否使用TLS
     * @return 初始化是否成功
     */
    bool initialize(const QString &serverHost, quint16 serverPort, bool useTLS = true);
    
    /**
     * @brief 连接到服务器
     * @return 连接是否成功启动
     */
    bool connectToServer();
    
    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromServer();
    
    /**
     * @brief 用户登录
     * @param username 用户名或邮箱
     * @param password 密码
     * @param rememberMe 是否记住登录
     * @return 登录请求是否成功发送
     */
    bool login(const QString &username, const QString &password, bool rememberMe = false);
    
    /**
     * @brief 用户注册
     * @param username 用户名
     * @param email 邮箱
     * @param password 密码
     * @param verificationCode 验证码
     * @return 注册请求是否成功发送
     */
    bool registerUser(const QString &username, const QString &email, 
                     const QString &password, const QString &verificationCode);
    
    /**
     * @brief 发送邮箱验证码
     * @param email 邮箱地址
     * @return 请求是否成功发送
     */
    bool sendVerificationCode(const QString &email);
    
    /**
     * @brief 用户登出
     */
    void logout();
    
    /**
     * @brief 尝试自动登录
     * @return 是否有保存的登录信息
     */
    bool tryAutoLogin();
    
    /**
     * @brief 检查是否已连接到服务器
     * @return 是否已连接
     */
    bool isConnected() const;
    
    /**
     * @brief 检查是否已登录
     * @return 是否已登录
     */
    bool isLoggedIn() const;
    
    /**
     * @brief 获取当前用户
     * @return 当前用户对象
     */
    User* currentUser() const;
    
    /**
     * @brief 检查是否正在加载
     * @return 是否正在加载
     */
    bool isLoading() const { return _authState != Idle; }
    
    /**
     * @brief 获取当前认证状态
     * @return 认证状态
     */
    AuthState authState() const { return _authState; }
    
    /**
     * @brief 验证输入数据
     * @param username 用户名
     * @param email 邮箱
     * @param password 密码
     * @param confirmPassword 确认密码
     * @param verificationCode 验证码
     * @return 验证结果和错误信息
     */
    QPair<bool, QString> validateRegistrationData(const QString &username, const QString &email,
                                                  const QString &password, const QString &confirmPassword,
                                                  const QString &verificationCode = "");
    
    /**
     * @brief 验证登录数据
     * @param username 用户名或邮箱
     * @param password 密码
     * @return 验证结果和错误信息
     */
    QPair<bool, QString> validateLoginData(const QString &username, const QString &password);

signals:
    /**
     * @brief 连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);
    
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
     * @brief 加载状态改变信号
     * @param loading 是否正在加载
     */
    void loadingStateChanged(bool loading);
    
    /**
     * @brief 登录成功信号
     * @param user 用户信息
     */
    void loginSucceeded(User* user);
    
    /**
     * @brief 登录失败信号
     * @param error 错误信息
     */
    void loginFailed(const QString &error);
    
    /**
     * @brief 注册成功信号
     * @param user 用户信息
     */
    void registerSucceeded(User* user);
    
    /**
     * @brief 注册失败信号
     * @param error 错误信息
     */
    void registerFailed(const QString &error);
    
    /**
     * @brief 验证码发送成功信号
     */
    void verificationCodeSent();
    
    /**
     * @brief 验证码发送失败信号
     * @param error 错误信息
     */
    void verificationCodeFailed(const QString &error);
    
    /**
     * @brief 网络错误信号
     * @param error 错误信息
     */
    void networkError(const QString &error);

private slots:
    void onNetworkConnectionStateChanged(NetworkClient::ConnectionState state);
    void onLoginResponse(const QString &requestId, const QJsonObject &response);
    void onRegisterResponse(const QString &requestId, const QJsonObject &response);
    void onVerificationCodeResponse(const QString &requestId, const QJsonObject &response);
    void onNetworkError(const QString &error);
    void onSessionExpired();
    void onAutoLoginRequested(const QString &username, const QString &passwordHash);

private:
    /**
     * @brief 设置认证状态
     * @param state 新状态
     */
    void setAuthState(AuthState state);
    
    /**
     * @brief 处理认证响应
     * @param response 响应数据
     * @return 认证响应对象
     */
    AuthResponse* processAuthResponse(const QJsonObject &response);

    /**
     * @brief 执行自动登录
     * @param username 用户名
     * @param passwordHash 密码哈希
     */
    void performAutoLogin(const QString &username, const QString &passwordHash);

private:
    static AuthManager* s_instance;
    static QMutex s_mutex; // 添加静态互斥锁
    
    NetworkClient* _networkClient;
    SessionManager* _sessionManager;
    
    QString _serverHost;
    quint16 _serverPort;
    bool _useTLS;
    
    AuthState _authState;
    QMap<QString, QString> _pendingRequests; // requestId -> requestType
};

#endif // AUTHMANAGER_H
