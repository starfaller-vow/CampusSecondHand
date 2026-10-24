#include "MessagesDAO.h"
#include "../db/DatabaseManager.h"
#include "../shared/Protocol.h"
#include "../shared/PushRegistry.h"
#include <QSqlQuery>

bool MessagesDAO::insert(int userId, const QString &title, const QString &content,
                         const QString &type)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT INTO messages (user_id, title, content, type) VALUES (?, ?, ?, ?)");
    q.addBindValue(userId);
    q.addBindValue(title);
    q.addBindValue(content);
    q.addBindValue(type);
    const bool ok = q.exec();

    // C/S：写库成功后向接收方在线客户端实时推送（未登录/离线则空推）
    if (ok) {
        QJsonObject data;
        data.insert("title", title);
        PushRegistry::instance().pushToUser(userId, cs::push::Message, data);
    }
    return ok;
}

QList<Message> MessagesDAO::getByUser(int userId)
{
    QList<Message> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM messages WHERE user_id = ? ORDER BY id DESC");
    q.addBindValue(userId);
    if (q.exec()) {
        while (q.next()) {
            Message m;
            m.id = q.value("id").toInt();// 消息主键id
            m.userId = q.value("user_id").toInt();// 接收消息用户id
            m.title = q.value("title").toString();// 消息标题
            m.content = q.value("content").toString();// 消息正文
            m.isRead = q.value("is_read").toInt();// 是否已读
            m.type = q.value("type").toString();// 消息类型：system / trade
            m.createTime = q.value("create_time").toString();
            list.append(m);// 将这条消息对象加入列表
        }
    }
    return list;
}

bool MessagesDAO::markRead(int msgId)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE messages SET is_read = 1 WHERE id = ?");
    q.addBindValue(msgId);
    return q.exec();
}

int MessagesDAO::unreadCount(int userId)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT COUNT(*) FROM messages WHERE user_id = ? AND is_read = 0");
    q.addBindValue(userId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}
