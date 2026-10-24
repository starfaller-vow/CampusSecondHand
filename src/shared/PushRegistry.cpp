#include "PushRegistry.h"

#include <QMetaObject>

PushRegistry &PushRegistry::instance()
{
    static PushRegistry r;
    return r;
}

void PushRegistry::registerConn(int userId, QObject *conn)
{
    if (!conn) return;
    QMutexLocker locker(&m_mutex);
    // 重登/换账号时先移除旧归属（不能调 unregisterConn，避免嵌套加锁）
    for (auto it = m_conns.begin(); it != m_conns.end(); ++it) {
        it.value().removeAll(QPointer<QObject>(conn));
    }
    m_conns[userId].append(QPointer<QObject>(conn));
}

void PushRegistry::unregisterConn(QObject *conn)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_conns.begin(); it != m_conns.end(); ++it) {
        it.value().removeAll(QPointer<QObject>(conn));
    }
}

void PushRegistry::pushToUser(int userId, const QString &type, const QJsonObject &data)
{
    // 先复制连接列表，避免持锁期间跨线程调用
    QList<QPointer<QObject>> targets;
    {
        QMutexLocker locker(&m_mutex);
        const auto &list = m_conns.value(userId);
        for (const QPointer<QObject> &c : list) {
            if (!c.isNull()) targets.append(c);
        }
    }

    QJsonObject frame;
    frame.insert("push", type);
    frame.insert("data", data);

    for (const QPointer<QObject> &c : targets) {
        if (c.isNull()) continue;
        // QueuedConnection：帧真正写入 socket 的动作发生在连接所在线程
        QMetaObject::invokeMethod(c.data(), "pushJson",
                                  Qt::QueuedConnection, Q_ARG(QJsonObject, frame));
    }
}