#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();
    void onRegisterClicked();

signals:
    void loginSuccess(const class User &user);

private:
    void setupUI();

    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_serverEdit;   // 服务器地址（host:port，见 setupUI）
    QPushButton *m_loginBtn;
    QPushButton *m_registerBtn;
    QLabel *m_titleLabel;
};

#endif // LOGINWINDOW_H
