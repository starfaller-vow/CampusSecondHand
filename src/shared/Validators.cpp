#include "Validators.h"

#include <QRegularExpression>

namespace uservalid {

bool isValidUsername(const QString &username, QString &err)
{
    if (username.length() < 3 || username.length() > 20) {
        err = QStringLiteral("用户名长度必须在3-20个字符之间");
        return false;
    }
    QRegularExpression re("^[a-zA-Z0-9_]+$");
    if (!re.match(username).hasMatch()) {
        err = QStringLiteral("用户名只能包含字母、数字和下划线");
        return false;
    }
    return true;
}

bool isValidPassword(const QString &password, QString &err)
{
    if (password.length() < 8) {
        err = QStringLiteral("密码至少8位");
        return false;
    }
    // 简单检查：至少含一个字母和一个数字
    const bool hasLetter = password.contains(QRegularExpression("[a-zA-Z]"));
    const bool hasDigit  = password.contains(QRegularExpression("[0-9]"));
    if (!hasLetter || !hasDigit) {
        err = QStringLiteral("密码必须同时包含字母和数字");
        return false;
    }
    return true;
}

bool isValidStudentNo(const QString &studentNo, QString &err)
{
    QRegularExpression re("^\\d{10}$");
    if (!re.match(studentNo).hasMatch()) {
        err = QStringLiteral("学号必须是10位数字");
        return false;
    }
    return true;
}

} // namespace uservalid