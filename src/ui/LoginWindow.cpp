#include "LoginWindow.h"
#include "RegisterDialog.h"
#include "ClientConnector.h"
#include "../models/Models.h"
#include "../shared/Protocol.h"
#include "../shared/JsonConv.h"
#include "../shared/PasswordUtil.h"
#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFormLayout>

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void LoginWindow::setupUI()
{
    setWindowTitle(QStringLiteral("校园二手交易系统 - 登录"));
    resize(400, 320);

    // === 标题 ===
    m_titleLabel = new QLabel(QStringLiteral("🎓 校园二手交易系统"), this);// 第二个参数this：以LoginWindow自身作为父对象，标题标签挂载在登录窗口下；建立父子关系，窗口销毁时标签自动释放，无需手动delete，this不用于窗口跳转

    m_titleLabel->setAlignment(Qt::AlignCenter);//水平、垂直都居中
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(18);//字号设为 18
    titleFont.setBold(true);//加粗
    m_titleLabel->setFont(titleFont);

    // === 表单 ===
    m_usernameEdit = new QLineEdit(this);//QLineEdit 是单行文本输入框。new 在堆上创建，this 作为父对象，挂到登录窗口下自动管理生命周期。
    m_usernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));//setPlaceholderText 设置 "占位提示文字"。它显示在输入框里，但只是提示，不是输入的内容。

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);//QLineEdit::Password 模式表示：用户实际输入的是密码，但屏幕上显示成圆点（●），防止旁边人看到。默认是 Normal（正常明文显示）

    m_serverEdit = new QLineEdit(this);
    m_serverEdit->setPlaceholderText(QStringLiteral("localhost:9527"));
    // C/S：默认填上次连过的服务器地址（三机联调时重登不必重填 IP）
    m_serverEdit->setText(QStringLiteral("%1:%2")
        .arg(ClientConnector::instance().lastHost())
        .arg(ClientConnector::instance().lastPort()));

    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow(QStringLiteral("用户名："), m_usernameEdit);
    formLayout->addRow(QStringLiteral("密  码："), m_passwordEdit);
    formLayout->addRow(QStringLiteral("服务器："), m_serverEdit);

    // === 按钮 ===
    m_loginBtn = new QPushButton(QStringLiteral("🔑 登录"), this);
    m_loginBtn->setMinimumHeight(40);
    m_loginBtn->setStyleSheet(
        "QPushButton { background-color: #1976D2; color: white; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background-color: #1565C0; }"//鼠标悬停在按钮上" 时的状态：背景色变成更深的 #1565C0。所以鼠标移上去按钮会变深一点，有交互反馈。
    );
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);//一旦 m_loginBtn 发出 clicked 信号，就自动调用 LoginWindow 的 onLoginClicked 这个函数。

    m_registerBtn = new QPushButton(QStringLiteral("注册新账号"), this);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);

    // === 主布局 ===
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 30, 40, 30);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(m_titleLabel);
    mainLayout->addSpacing(20);
    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(m_loginBtn);
    mainLayout->addSpacing(8);

    QHBoxLayout *registerLayout = new QHBoxLayout();
    registerLayout->addStretch();
    registerLayout->addWidget(m_registerBtn);
    registerLayout->addStretch();
    mainLayout->addLayout(registerLayout);
    mainLayout->addStretch();
}

void LoginWindow::onLoginClicked()
{
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("用户名和密码不能为空"));
        return;
    }

    // 1. 解析并连接服务器（默认 localhost:9527）
    const QString addr = m_serverEdit->text().trimmed();
    const QStringList parts = addr.split(QLatin1Char(':'));
    QString host = parts.value(0).trimmed().isEmpty()
        ? QStringLiteral("localhost") : parts.value(0).trimmed();
    quint16 port = static_cast<quint16>(
        (parts.size() > 1 ? parts.value(1).toUInt() : 9527u) & 0xFFFFu);

    QString err;
    if (!ClientConnector::instance().connectToServer(host, port, err)) {
        QMessageBox::critical(this, QStringLiteral("连接失败"), err);
        return;
    }

    // 2. 客户端先哈希密码，再发服务端（网络不传明文）
    QJsonObject params{
        {"username", username},
        {"passwordHash", passutil::sha256Hex(password)}
    };
    QJsonObject data;
    if (!ClientConnector::instance().request(cs::cmd::Login, params, data, err)) {
        QMessageBox::critical(this, QStringLiteral("登录失败"), err);
        return;
    }

    // 3. 解析用户 + 记住会话 token
    User user = jsoncv::userFromJson(data.value("user").toObject());
    ClientConnector::instance().setToken(data.value("token").toString());
    ClientConnector::instance().setMyUserId(user.id);

    // 登录成功 → 打开主窗口
    MainWindow *mainWin = new MainWindow(user);
    mainWin->show();
    this->close();
}

void LoginWindow::onRegisterClicked()
{
    RegisterDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_usernameEdit->setText(dlg.registeredUsername());
        m_passwordEdit->setFocus();
    }
}
