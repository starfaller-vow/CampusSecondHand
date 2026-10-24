#ifndef PUSHREGISTRY_H
#define PUSHREGISTRY_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>

// ============================================================
// 服务端推送注册表（用户ID → 在线连接列表）
// · 业务层（MessagesDAO 等）写完消息后调用 pushToUser 推送
// · 跨线程安全：向连接发送使用 QueuedConnection，落到连接所在线程执行
// · 编译在客户端也无副作用（客户端不使用本类）
// ============================================================
class PushRegistry
{
public:
    static PushRegistry &instance();

    // 连接注册/注销（同一连接重复注册时自动先注销旧归属）
    void registerConn(int userId, QObject *conn);
    void unregisterConn(QObject *conn);

    // 向某用户的全部在线连接推送一帧 { "push":type, "data":data }
    void pushToUser(int userId, const QString &type, const QJsonObject &data);

private:
    PushRegistry() = default;

    QMutex m_mutex;
    QHash<int, QList<QPointer<QObject>>> m_conns;
};

#endif // PUSHREGISTRY_H