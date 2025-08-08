#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QApplication>
#include <QStyle>
#include <QScreen>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QStyleFactory>
#include <stdexcept>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isDarkTheme(false)
    , rememberPassword(false)
    , isLoggingIn(false)
    , networkManager(new QNetworkAccessManager(this))
    , settings(new QSettings("QKChat", "Server", this))
{
    ui->setupUi(this);
    setupTheme();
    setupUI();
    
    // 连接网络信号
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onNetworkReplyFinished);
    
    // 在UI创建完成后加载设置
    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::setupUI()
{
    // 设置窗口属性
    setWindowTitle("QKChat Server - 登录");
    setFixedSize(400, 500);
    setWindowFlags(Qt::Window);
    
    // 居中显示
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
    
    // 创建中央部件
    QWidget* centralWidget = new QWidget(this);
    if (!centralWidget) {
        return;
    }
    setCentralWidget(centralWidget);
    
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    if (!mainLayout) {
        return;
    }
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    
    // 创建标题
    titleLabel = new QLabel("QKChat Server", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPixelSize(32);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: " + currentTheme.textColor.name() + ";");
    
    subtitleLabel = new QLabel("管理员登录", this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subtitleFont = subtitleLabel->font();
    subtitleFont.setPixelSize(18);
    subtitleLabel->setFont(subtitleFont);
    subtitleLabel->setStyleSheet("color: " + currentTheme.secondaryTextColor.name() + ";");
    
    // 创建输入框
    usernameInput = new QLineEdit(this);
    usernameInput->setPlaceholderText("用户名");
    usernameInput->setFixedHeight(50);
    usernameInput->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid " + currentTheme.borderColor.name() + ";"
        "   border-radius: 8px;"
        "   padding: 0 15px;"
        "   font-size: 16px;"
        "   color: " + currentTheme.textColor.name() + ";"
        "   background-color: transparent;"
        "}"
    );
    
    passwordInput = new QLineEdit(this);
    passwordInput->setPlaceholderText("密码");
    passwordInput->setEchoMode(QLineEdit::Password);
    passwordInput->setFixedHeight(50);
    passwordInput->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid " + currentTheme.borderColor.name() + ";"
        "   border-radius: 8px;"
        "   padding: 0 15px;"
        "   font-size: 16px;"
        "   color: " + currentTheme.textColor.name() + ";"
        "   background-color: transparent;"
        "}"
    );
    
    // 创建记住密码复选框
    rememberPasswordCheckBox = new QCheckBox("记住密码", this);
    rememberPasswordCheckBox->setChecked(rememberPassword);
    rememberPasswordCheckBox->setStyleSheet(
        "QCheckBox {"
        "   color: " + currentTheme.textColor.name() + ";"
        "   font-size: 14px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 18px;"
        "   height: 18px;"
        "}"
    );
    
    // 创建主题切换按钮
    themeToggleButton = new QPushButton(isDarkTheme ? "浅色" : "深色", this);
    themeToggleButton->setFixedSize(80, 35);
    themeToggleButton->setStyleSheet(
        "QPushButton {"
        "   background-color: " + currentTheme.primaryColor.name() + ";"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: " + currentTheme.primaryColor.darker(110).name() + ";"
        "}"
    );
    
    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(rememberPasswordCheckBox);
    buttonLayout->addStretch();
    buttonLayout->addWidget(themeToggleButton);
    
    // 创建错误标签
    errorLabel = new QLabel(this);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet(
        "QLabel {"
        "   color: #FF3B30;"
        "   font-size: 14px;"
        "}"
    );
    errorLabel->setVisible(false);
    
    // 创建登录按钮
    loginButton = new QPushButton("登录", this);
    loginButton->setFixedHeight(50);
    loginButton->setStyleSheet(
        "QPushButton {"
        "   background-color: " + currentTheme.primaryColor.name() + ";"
        "   color: white;"
        "   border: none;"
        "   border-radius: 8px;"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: " + currentTheme.primaryColor.darker(110).name() + ";"
        "}"
        "QPushButton:disabled {"
        "   background-color: " + currentTheme.secondaryTextColor.name() + ";"
        "}"
    );
    
    // 添加组件到布局
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subtitleLabel);
    mainLayout->addStretch(1);
    mainLayout->addWidget(usernameInput);
    mainLayout->addWidget(passwordInput);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(errorLabel);
    mainLayout->addWidget(loginButton);
    mainLayout->addStretch(1);
    
    // 连接信号
    connect(loginButton, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
    connect(themeToggleButton, &QPushButton::clicked, this, &MainWindow::onThemeToggleClicked);
    connect(rememberPasswordCheckBox, &QCheckBox::toggled, this, &MainWindow::onRememberPasswordChanged);
    connect(usernameInput, &QLineEdit::returnPressed, passwordInput, [this]() { passwordInput->setFocus(); });
    connect(passwordInput, &QLineEdit::returnPressed, this, &MainWindow::onLoginClicked);
    
    // 设置焦点
    usernameInput->setFocus();
}

void MainWindow::setupTheme()
{
    // 设置浅色主题颜色
    lightTheme.backgroundColor = QColor("#FFFFFF");
    lightTheme.textColor = QColor("#000000");
    lightTheme.secondaryTextColor = QColor("#666666");
    lightTheme.borderColor = QColor("#E0E0E0");
    lightTheme.primaryColor = QColor("#007AFF");
    
    // 设置深色主题颜色
    darkTheme.backgroundColor = QColor("#1C1C1E");
    darkTheme.textColor = QColor("#FFFFFF");
    darkTheme.secondaryTextColor = QColor("#8E8E93");
    darkTheme.borderColor = QColor("#38383A");
    darkTheme.primaryColor = QColor("#007AFF");
    
    // 设置当前主题
    currentTheme = lightTheme;
}

void MainWindow::loadSettings()
{
    isDarkTheme = settings->value("theme/dark", false).toBool();
    rememberPassword = settings->value("login/remember_password", false).toBool();
    
    // 确保控件已创建后再设置值
    if (usernameInput && passwordInput && rememberPassword) {
        usernameInput->setText(settings->value("login/username", "").toString());
        passwordInput->setText(settings->value("login/password", "").toString());
    }
    
    if (isDarkTheme) {
        toggleTheme();
    }
}

void MainWindow::saveSettings()
{
    settings->setValue("theme/dark", isDarkTheme);
    settings->setValue("login/remember_password", rememberPassword);
    
    if (rememberPassword) {
        settings->setValue("login/username", usernameInput->text());
        settings->setValue("login/password", passwordInput->text());
    } else {
        settings->remove("login/username");
        settings->remove("login/password");
    }
}

bool MainWindow::validateLogin()
{
    if (usernameInput->text().trimmed().isEmpty()) {
        showError("请输入用户名");
        return false;
    }
    
    if (passwordInput->text().trimmed().isEmpty()) {
        showError("请输入密码");
        return false;
    }
    
    return true;
}

void MainWindow::performLogin()
{
    if (!validateLogin()) return;
    
    isLoggingIn = true;
    loginButton->setEnabled(false);
    loginButton->setText("登录中...");
    errorLabel->setVisible(false);
    
    // 验证管理员账号
    QString username = usernameInput->text().trimmed();
    QString password = passwordInput->text().trimmed();
    
    if (username == "admin" && password == "admin@123") {
        // 登录成功
        QTimer::singleShot(1000, [this]() {
            isLoggingIn = false;
            loginButton->setEnabled(true);
            loginButton->setText("登录");
            showSuccess("登录成功！");
            
            // TODO: 跳转到主界面
            QMessageBox::information(this, "登录成功", "欢迎使用QKChat服务器管理界面！");
        });
    } else {
        // 登录失败
        QTimer::singleShot(1000, [this]() {
            isLoggingIn = false;
            loginButton->setEnabled(true);
            loginButton->setText("登录");
            showError("用户名或密码错误");
        });
    }
}

void MainWindow::showError(const QString &message)
{
    errorLabel->setText(message);
    errorLabel->setVisible(true);
}

void MainWindow::showSuccess(const QString &message)
{
    errorLabel->setText(message);
    errorLabel->setStyleSheet(
        "QLabel {"
        "   color: #34C759;"
        "   font-size: 14px;"
        "}"
    );
    errorLabel->setVisible(true);
}

void MainWindow::toggleTheme()
{
    isDarkTheme = !isDarkTheme;
    
    if (isDarkTheme) {
        currentTheme = darkTheme;
        setStyleSheet(
            "QMainWindow {"
            "   background-color: " + currentTheme.backgroundColor.name() + ";"
            "}"
        );
        themeToggleButton->setText("浅色");
    } else {
        currentTheme = lightTheme;
        setStyleSheet(
            "QMainWindow {"
            "   background-color: " + currentTheme.backgroundColor.name() + ";"
            "}"
        );
        themeToggleButton->setText("深色");
    }
    
    // 更新组件样式
    titleLabel->setStyleSheet("color: " + currentTheme.textColor.name() + ";");
    subtitleLabel->setStyleSheet("color: " + currentTheme.secondaryTextColor.name() + ";");
    rememberPasswordCheckBox->setStyleSheet(
        "QCheckBox {"
        "   color: " + currentTheme.textColor.name() + ";"
        "   font-size: 14px;"
        "}"
    );
    
    usernameInput->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid " + currentTheme.borderColor.name() + ";"
        "   border-radius: 8px;"
        "   padding: 0 15px;"
        "   font-size: 16px;"
        "   color: " + currentTheme.textColor.name() + ";"
        "   background-color: transparent;"
        "}"
    );
    
    passwordInput->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid " + currentTheme.borderColor.name() + ";"
        "   border-radius: 8px;"
        "   padding: 0 15px;"
        "   font-size: 16px;"
        "   color: " + currentTheme.textColor.name() + ";"
        "   background-color: transparent;"
        "}"
    );
}

void MainWindow::onLoginClicked()
{
    performLogin();
}

void MainWindow::onThemeToggleClicked()
{
    toggleTheme();
}

void MainWindow::onRememberPasswordChanged(bool checked)
{
    rememberPassword = checked;
}

void MainWindow::onNetworkReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    // TODO: 处理网络响应
}
