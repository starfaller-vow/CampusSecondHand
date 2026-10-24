#include "RegisterDialog.h"
#include "ClientConnector.h"
#include "../shared/Protocol.h"
#include "../shared/PasswordUtil.h"
#include "../shared/Validators.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QLabel>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
}

void RegisterDialog::setupUI()
{
    setWindowTitle(QStringLiteral("注册新账号"));
    resize(450, 480);

    QLabel *title = new QLabel(QStringLiteral("📝 创建新账号"), this);
    title->setAlignment(Qt::AlignCenter);
    QFont f = title->font();
    f.setPointSize(16);
    f.setBold(true);
    title->setFont(f);

    m_usernameEdit  = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("3-20位字母、数字或下划线"));

    m_passwordEdit  = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("至少8位，需含字母和数字"));

    m_confirmPwdEdit = new QLineEdit(this);
    m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
    m_confirmPwdEdit->setPlaceholderText(QStringLiteral("再次输入密码"));

    m_studentNoEdit = new QLineEdit(this);
    m_studentNoEdit->setPlaceholderText(QStringLiteral("10位学号"));

    m_collegeEdit   = new QLineEdit(this);
    m_collegeEdit->setPlaceholderText(QStringLiteral("（可选）"));

    m_contactEdit   = new QLineEdit(this);
    m_contactEdit->setPlaceholderText(QStringLiteral("微信/QQ/手机号"));

    QFormLayout *form = new QFormLayout();
    form->addRow(QStringLiteral("用户名*："),   m_usernameEdit);
    form->addRow(QStringLiteral("密  码*："),   m_passwordEdit);
    form->addRow(QStringLiteral("确认密码*："), m_confirmPwdEdit);
    form->addRow(QStringLiteral("学  号*："),   m_studentNoEdit);
    form->addRow(QStringLiteral("学  院："),    m_collegeEdit);
    form->addRow(QStringLiteral("联系方式*："), m_contactEdit);

    QPushButton *submitBtn = new QPushButton(QStringLiteral("✅ 提交注册"), this);
    submitBtn->setMinimumHeight(40);
    submitBtn->setStyleSheet(
        "QPushButton { background-color: #2E7D32; color: white; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background-color: #1B5E20; }"
    );
    connect(submitBtn, &QPushButton::clicked, this, &RegisterDialog::onSubmit);

    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(submitBtn);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(40, 25, 40, 25);
    main->addSpacing(5);
    main->addWidget(title);
    main->addSpacing(15);
    main->addLayout(form);
    main->addSpacing(15);
    main->addLayout(btnLayout);
}

void RegisterDialog::onSubmit()
{
    QString username  = m_usernameEdit->text().trimmed();
    QString password  = m_passwordEdit->text();
    QString confirm   = m_confirmPwdEdit->text();
    QString studentNo = m_studentNoEdit->text().trimmed();
    QString college   = m_collegeEdit->text().trimmed();
    QString contact   = m_contactEdit->text().trimmed();

    // 基础检查
    if (username.isEmpty() || password.isEmpty() || confirm.isEmpty()
        || studentNo.isEmpty() || contact.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请填写所有带 * 号的必填项"));
        return;
    }

    if (password != confirm) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("两次输入的密码不一致"));
        return;
    }

    // 本地格式校验（密码强度规则在明文阶段校验，服务端不再校验）
    QString err;
    if (!uservalid::isValidUsername(username, err) ||
        !uservalid::isValidPassword(password, err) ||
        !uservalid::isValidStudentNo(studentNo, err)) {
        QMessageBox::critical(this, QStringLiteral("注册失败"), err);
        return;
    }

    // C/S：确保已连接（登录窗已连过则直接复用）
    if (!ClientConnector::instance().isConnected()) {
        QString cErr;
        if (!ClientConnector::instance().connectToServer(
                QStringLiteral("localhost"), 9527, cErr)) {
            QMessageBox::critical(this, QStringLiteral("连接失败"), cErr);
            return;
        }
    }

    // 客户端哈希密码后提交服务端注册
    QJsonObject params{
        {"username", username},
        {"passwordHash", passutil::sha256Hex(password)},
        {"studentNo", studentNo},
        {"college", college},
        {"contact", contact}
    };
    QJsonObject data;
    if (!ClientConnector::instance().request(cs::cmd::Register, params, data, err)) {
        QMessageBox::critical(this, QStringLiteral("注册失败"), err);
        return;
    }

    m_registeredUsername = username;
    QMessageBox::information(this, QStringLiteral("注册成功"),
                             QStringLiteral("账号 %1 注册成功！\n请返回登录。").arg(username));
    accept();
}
