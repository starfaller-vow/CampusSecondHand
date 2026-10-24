#include "DbLock.h"

QReadWriteLock &serverDbLock()
{
    static QReadWriteLock m(QReadWriteLock::Recursive);
    return m;
}
