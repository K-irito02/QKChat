#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoginClicked();
    void onThemeToggleClicked();
    void onRememberPasswordChanged(bool checked);
    void onNetworkReplyFinished(QNetworkReply *reply);

private:
    void setupUI();
    void setupTheme();
    void loadSettings();
    void saveSettings();
    bool validateLogin();
    void performLogin();
    void showError(const QString &message);
    void showSuccess(const QString &message);
    void toggleTheme();

private:
    Ui::MainWindow *ui;
    
    // UI组件
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QLineEdit *usernameInput;
    QLineEdit *passwordInput;
    QCheckBox *rememberPasswordCheckBox;
    QPushButton *themeToggleButton;
    QPushButton *loginButton;
    QLabel *errorLabel;
    
    // 状态
    bool isDarkTheme;
    bool rememberPassword;
    bool isLoggingIn;
    
    // 网络管理器
    QNetworkAccessManager *networkManager;
    
    // 设置
    QSettings *settings;
    
    // 主题颜色
    struct ThemeColors {
        QColor backgroundColor;
        QColor textColor;
        QColor secondaryTextColor;
        QColor borderColor;
        QColor primaryColor;
    };
    
    ThemeColors lightTheme;
    ThemeColors darkTheme;
    ThemeColors currentTheme;
};
#endif // MAINWINDOW_H
