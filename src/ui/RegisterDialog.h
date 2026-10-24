#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);

    QString registeredUsername() const { return m_registeredUsername; }

private slots:
    void onSubmit();

private:
    void setupUI();

    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_confirmPwdEdit;
    QLineEdit *m_studentNoEdit;
    QLineEdit *m_collegeEdit;
    QLineEdit *m_contactEdit;

    QString m_registeredUsername;
};

#endif // REGISTERDIALOG_H
