#include "PasswordUtil.h"
#include <QCryptographicHash>

namespace passutil {
    //接收用户明文密码，算出 SHA256 哈希串，用来存数据库，**不保存原始明文密码**

QString sha256Hex(const QString &plain)
{
    QByteArray hash = QCryptographicHash::hash(
        plain.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

} // namespace passutil