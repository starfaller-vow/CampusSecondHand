#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <QJsonObject>
#include <QHash>
#include <QMutex>
#include <QString>

// 命令分发器：把 cmd 路由到具体业务（服务端专属）
// 注意：handle() 被调用前，调用方必须已持有 serverDbLock()
// （只读命令持读锁可并发，写命令持写锁独占）
class Dispatcher
{
public:
    static Dispatcher &instance();

    /**
     * @brief 处理一条命令
     * @param cmd    命令名（cs::cmd::xxx）
     * @param params 请求参数
     * @param token  会话令牌（未登录命令可为空）
     * @param err    失败原因（成功时为空）
     * @return 成功返回 data 对象；失败返回空对象
     */
    QJsonObject handle(const QString &cmd, const QJsonObject &params,
                       const QString &token, QString &err);

    // ===== 会话 token =====
    QString issueToken(int userId);
    // 校验 token：有效返回 true 并输出 userId；无效/过期返回 false
    bool  tokenUserId(const QString &token, int &userId);
    void  revokeTokensOfUser(int userId); // 封禁时踢下线用

private:
    Dispatcher() = default;

    QHash<QString, int> m_tokens; // token -> userId
    QMutex m_tokenMutex;
};

#endif // DISPATCHER_H