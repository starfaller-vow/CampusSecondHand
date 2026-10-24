#include "PasswordUtil.h"
#include <QCryptographicHash>

namespace passutil {

QString sha256Hex(const QString &plain)
{
    QByteArray hash = QCryptographicHash::hash(
        plain.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

} // namespace passutil