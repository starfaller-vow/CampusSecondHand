#include "Dispatcher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QRandomGenerator>

#include "Models.h"
#include "Protocol.h"
#include "JsonConv.h"
#include "DatabaseManager.h"
#include "UserDAO.h"
#include "UserManager.h"
#include "MessagesDAO.h"
#include "../shared/PushRegistry.h"

Dispatcher &Dispatcher::instance()
{
    static Dispatcher d;
    return d;
}

// ==================== token 会话 ====================

QString Dispatcher::issueToken(int userId)
{
    // 64位随机数转16位十六进制串，课程级强度足够
    const QString token = QString::number(
        QRandomGenerator::global()->generate64(), 16);
    QMutexLocker locker(&m_tokenMutex);
    m_tokens.insert(token, userId);
    return token;
}

bool Dispatcher::tokenUserId(const QString &token, int &userId)
{
    QMutexLocker locker(&m_tokenMutex);
    auto it = m_tokens.constFind(token);
    if (it == m_tokens.constEnd()) return false;
    userId = it.value();
    return true;
}

void Dispatcher::revokeTokensOfUser(int userId)
{
    QMutexLocker locker(&m_tokenMutex);
    for (auto it = m_tokens.begin(); it != m_tokens.end(); ) {
        if (it.value() == userId) it = m_tokens.erase(it);
        else ++it;
    }
}

// ==================== 内部工具 ====================

namespace {

// 校验 token 并把 userId 填入；失败返回 false + 错误信息
bool requireUser(Dispatcher &d, const QString &token, int &userId, QString &err)
{
    if (!d.tokenUserId(token, userId) || userId <= 0) {
        err = QStringLiteral("登录已失效，请重新登录");
        return false;
    }
    return true;
}

// 校验 token 且必须是管理员
bool requireAdmin(Dispatcher &d, const QString &token, int &adminId, QString &err)
{
    if (!requireUser(d, token, adminId, err)) return false;
    const User u = UserDAO::getById(adminId);
    if (!u.isAdmin()) {
        err = QStringLiteral("需要管理员权限");
        return false;
    }
    return true;
}

// 商品 JSON 附带卡片展示所需的分类名与卖家用户名（客户端无需再额外请求）


// 交易 JSON（用户视角）：一次性附带商品信息与对端用户信息，消灭客户端列表的逐行 N+1 请求


// 交易 JSON（管理员视角）：附带买卖双方与商品信息


// 举报 JSON：附带被举报商品信息（举报中心列表直接显示，无需逐行请求商品详情）


} // namespace

// ==================== 命令分发 ====================

QJsonObject Dispatcher::handle(const QString &cmd, const QJsonObject &p,
                               const QString &token, QString &err)
{
    int uid = 0;

    // ==================== 认证 ====================

    // ---- 登录（客户端已哈希）----
    if (cmd == cs::cmd::Login) {
        const QString username = p.value("username").toString().trimmed();
        const QString passwordHash = p.value("passwordHash").toString();
        User u;
        QString e;
        if (!UserDAO::loginByHash(username, passwordHash, u, e)) { err = e; return {}; }
        QJsonObject data;
        data.insert("user", jsoncv::userToJson(u));
        data.insert("token", issueToken(u.id));
        return data;
    }

    // ---- 注册（密码已哈希）----
    if (cmd == cs::cmd::Register) {
        const QString username   = p.value("username").toString().trimmed();
        const QString passwordHash = p.value("passwordHash").toString();
        const QString studentNo  = p.value("studentNo").toString().trimmed();
        const QString college    = p.value("college").toString().trimmed();
        const QString contact    = p.value("contact").toString().trimmed();
        if (!UserManager::instance().registerUserWithHash(
                username, passwordHash, studentNo, college, contact, err)) {
            return {};
        }
        return QJsonObject{ {"username", username} };
    }

    // ==================== 公开数据 ====================

    // ---- 商品分类列表 ----


    // ---- 商品浏览/搜索（无需登录，与单机版一致）----


    // ---- 商品详情（任何人可看，购买需要登录）----


    // ---- 用户公开信息（交易记录对端显示等）----
    if (cmd == cs::cmd::UserGet) {
        const User u = UserDAO::getById(p.value("userId").toInt());
        if (u.id == 0) { err = QStringLiteral("用户不存在"); return {}; }
        return QJsonObject{ {"user", jsoncv::userToJson(u)} };
    }

    // ==================== 需登录：用户自身 ====================
    if (!requireUser(*this, token, uid, err)) return {};

    // ---- 我自己的信息（刷新余额用）----
    if (cmd == cs::cmd::Self) {
        return QJsonObject{ {"user", jsoncv::userToJson(UserDAO::getById(uid))} };
    }

    if (cmd == cs::cmd::UpdateContact) {
        const QString contact = p.value("contact").toString().trimmed();
        if (!UserManager::instance().changeContact(uid, contact, err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::ChangePassword) {
        // 新旧密码都已由客户端哈希
        const QString oldHash = p.value("oldPasswordHash").toString();
        const QString newHash = p.value("newPasswordHash").toString();
        const User u = UserDAO::getById(uid);
        if (u.id == 0) { err = QStringLiteral("用户不存在"); return {}; }
        if (oldHash != u.passwordHash) { err = QStringLiteral("旧密码错误"); return {}; }
        if (newHash.size() != 64) { err = QStringLiteral("新密码格式错误"); return {}; }
        if (!UserDAO::updatePassword(uid, newHash, err)) return {};
        return QJsonObject{};
    }

    // ---- 推荐价（发布时参考）----


    // ==================== 商品：我的/发布/编辑/上下架 ====================













    // ==================== 收藏 ====================







    // ==================== 消息 ====================

    if (cmd == cs::cmd::MessageList) {
        QJsonArray arr;
        for (const Message &m : MessagesDAO::getByUser(uid)) {
            arr.append(jsoncv::messageToJson(m));
        }
        return QJsonObject{ {"messages", arr} };
    }

    if (cmd == cs::cmd::MessageMarkRead) {
        MessagesDAO::markRead(p.value("msgId").toInt());
        return QJsonObject{};
    }

    if (cmd == cs::cmd::MessageReadAll) {
        for (const Message &m : MessagesDAO::getByUser(uid)) {
            if (!m.read()) MessagesDAO::markRead(m.id);
        }
        return QJsonObject{};
    }

    if (cmd == cs::cmd::MessageUnread) {
        return QJsonObject{ {"unread", MessagesDAO::unreadCount(uid)} };
    }

    // ---- 联系管理员：反馈直达全部管理员消息箱（单点“联系管理员”入口）----
    if (cmd == cs::cmd::ContactAdmin) {
        const QString content = p.value("content").toString().trimmed();
        if (content.isEmpty()) { err = QStringLiteral("内容不能为空"); return {}; }
        if (content.size() > 500) { err = QStringLiteral("内容过长（最多500字）"); return {}; }
        const User me = UserDAO::getById(uid);
        int sent = 0;
        for (const User &u : UserManager::instance().listAllUsers()) {
            if (!u.isAdmin()) continue;
            MessagesDAO::insert(u.id,
                QStringLiteral("📮 用户反馈：@%1").arg(me.username),
                QStringLiteral("用户【%1 (id=%2)】反馈：\n%3")
                    .arg(me.username).arg(me.id).arg(content),
                "feedback");
            sent++;
        }
        return QJsonObject{ {"sent", sent} };
    }

    // ==================== 交易 ====================











    // ==================== 钱包 ====================





    // ==================== 举报（提交人人可为）====================



    // ==================== 管理员专区 ====================
    if (!requireAdmin(*this, token, uid, err)) return {};









    // ---- 分类管理 ----




    // ---- 手续费/系数设置 ----



    // ---- 充值码生成 ----


    // ---- 举报处理 ----








    // ---- 交易管理 ----






    // ---- 余额统计 ----


    err = QStringLiteral("未实现命令: %1").arg(cmd);
    return {};
}