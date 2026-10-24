#ifndef MESSAGESDAO_H
#define MESSAGESDAO_H

#include "../models/Models.h"
#include <QString>

class MessagesDAO
{
public:
    /**
     * @brief 发送消息
     */
    static bool insert(int userId, const QString &title, const QString &content,
                       const QString &type = "system");

    static QList<Message> getByUser(int userId);
    static bool markRead(int msgId);//把指定单条消息标记为**已读**
    static int  unreadCount(int userId);//统计未读数
};

#endif
