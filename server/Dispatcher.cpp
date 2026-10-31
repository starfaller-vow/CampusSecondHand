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
#include "GoodsDAO.h"
#include "GoodsManager.h"
#include "CategoriesDAO.h"
#include "FavoritesDAO.h"
#include "MessagesDAO.h"
#include "ReportsDAO.h"
#include "RedeemCodesDAO.h"
#include "BalanceLogsDAO.h"
#include "SettingsDAO.h"
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
QJsonObject goodsJsonWithContext(const Goods &g)
{
    QJsonObject o = jsoncv::goodsToJson(g);
    o.insert("categoryName", CategoriesDAO::getById(g.categoryId).name);
    o.insert("sellerName", UserDAO::getById(g.sellerId).username);
    return o;
}

// 交易 JSON（用户视角）：一次性附带商品信息与对端用户信息，消灭客户端列表的逐行 N+1 请求


// 交易 JSON（管理员视角）：附带买卖双方与商品信息


// 举报 JSON：附带被举报商品信息（举报中心列表直接显示，无需逐行请求商品详情）
QJsonObject reportJsonWithContext(const Report &r)
{
    QJsonObject o = jsoncv::reportToJson(r);
    if (r.targetType == "goods") {
        const Goods g = GoodsDAO::getById(r.targetId);
        o.insert("targetExists", g.id != 0);
        if (g.id != 0) {
            o.insert("targetTitle", g.title);
            o.insert("targetStatus", g.status);
            o.insert("targetOffReason", g.offReason);
        }
    }
    return o;
}

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
    if (cmd == cs::cmd::ListCategories) {
        QJsonArray arr;
        for (const Category &c : CategoriesDAO::getAll()) {
            arr.append(jsoncv::categoryToJson(c));
        }
        return QJsonObject{ {"categories", arr} };
    }

    // ---- 商品浏览/搜索（无需登录，与单机版一致）----
    if (cmd == cs::cmd::ListGoods) {
        GoodsDAO::QueryOptions opt;
        opt.keyword    = p.value("keyword").toString().trimmed();
        opt.categoryId = p.value("categoryId").toInt();
        opt.minPrice   = p.value("minPrice").toDouble();
        opt.maxPrice   = p.value("maxPrice").toDouble();
        const QString field = p.value("sortField").toString();
        const QString order = p.value("sortOrder").toString();
        if (!field.isEmpty()) opt.sortField = field;
        if (!order.isEmpty()) opt.sortOrder = order;

        QJsonArray arr;
        const QList<Goods> list = GoodsManager::instance().browseOnSale(opt);
        for (const Goods &g : list) {
            arr.append(goodsJsonWithContext(g));
        }
        return QJsonObject{ {"goods", arr} };
    }

    // ---- 商品详情（任何人可看，购买需要登录）----
    if (cmd == cs::cmd::GoodsDetail) {
        const Goods g = GoodsDAO::getById(p.value("goodsId").toInt());
        if (g.id == 0) { err = QStringLiteral("商品不存在"); return {}; }
        return QJsonObject{
            {"goods", goodsJsonWithContext(g)}
        };
    }

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
    if (cmd == cs::cmd::SuggestPrice) {
        const QString cond = p.value("condition").toString();
        const double price = GoodsManager::suggestPrice(cond, p.value("originalPrice").toDouble());
        return QJsonObject{
            {"price", price},
            {"coef", SettingsDAO::conditionCoef(cond)}
        };
    }

    // ==================== 商品：我的/发布/编辑/上下架 ====================

    if (cmd == cs::cmd::MineGoods) {
        QJsonArray arr;
        for (const Goods &g : GoodsDAO::getBySeller(uid)) {
            arr.append(goodsJsonWithContext(g));
        }
        return QJsonObject{ {"goods", arr} };
    }

    if (cmd == cs::cmd::PublishGoods) {
        // 图片已通过 image.upload 传到服务端，这里只存文件名列表
        const QString title = p.value("title").toString().trimmed();
        const QString desc  = p.value("description").toString().trimmed();
        const double originalPrice = p.value("originalPrice").toDouble();
        const QString condition    = p.value("condition").toString();
        const double currentPrice  = p.value("currentPrice").toDouble();
        const int categoryId       = p.value("categoryId").toInt();
        const QString contact      = p.value("contact").toString().trimmed();

        if (title.isEmpty())      { err = QStringLiteral("标题不能为空"); return {}; }
        if (desc.isEmpty())       { err = QStringLiteral("描述不能为空"); return {}; }
        if (originalPrice <= 0)   { err = QStringLiteral("原价必须大于0"); return {}; }
        if (currentPrice <= 0)    { err = QStringLiteral("现价必须大于0"); return {}; }
        if (categoryId <= 0)      { err = QStringLiteral("请选择分类"); return {}; }

        QString hit;
        if (GoodsManager::containsSensitiveWord(title, hit)) {
            err = QStringLiteral("标题包含敏感词：%1").arg(hit); return {};
        }
        if (GoodsManager::containsSensitiveWord(desc, hit)) {
            err = QStringLiteral("描述包含敏感词：%1").arg(hit); return {};
        }

        // 组装图片 JSON（服务端只接受已上传的合法文件名）
        QJsonArray imgArr;
        const QJsonArray names = p.value("images").toArray();
        for (const auto &v : names) {
            const QString fname = QFileInfo(v.toString()).fileName();
            imgArr.append(QStringLiteral("images/%1").arg(fname));
        }

        Goods g;
        g.sellerId      = uid;
        g.title         = title;
        g.description   = desc;
        g.originalPrice = originalPrice;
        g.condition     = condition;
        g.currentPrice  = currentPrice;
        g.categoryId    = categoryId;
        g.imagesJson    = QString::fromUtf8(QJsonDocument(imgArr).toJson(QJsonDocument::Compact));
        g.status        = "on_sale";
        if (!GoodsDAO::insert(g, err)) return {};
        return QJsonObject{ {"goodsId", 0} };
    }

    if (cmd == cs::cmd::EditGoods) {
        Goods existing = GoodsDAO::getById(p.value("goodsId").toInt());
        if (existing.id == 0) { err = QStringLiteral("商品不存在"); return {}; }
        if (existing.sellerId != uid) { err = QStringLiteral("无权编辑该商品"); return {}; }
        if (!existing.isOnSale()) { err = QStringLiteral("该商品状态下不可编辑（仅在售状态可编辑）"); return {}; }

        existing.title         = p.value("title").toString().trimmed();
        existing.description   = p.value("description").toString().trimmed();
        existing.originalPrice = p.value("originalPrice").toDouble();
        existing.condition     = p.value("condition").toString();
        existing.currentPrice  = p.value("currentPrice").toDouble();
        existing.categoryId    = p.value("categoryId").toInt();
        // 图片沿用已有；若客户端上传了新图，则替换
        const QJsonArray names = p.value("images").toArray();
        if (!names.isEmpty()) {
            QJsonArray imgArr;
            for (const auto &v : names) {
                const QString fname = QFileInfo(v.toString()).fileName();
                imgArr.append(QStringLiteral("images/%1").arg(fname));
            }
            existing.imagesJson = QString::fromUtf8(
                QJsonDocument(imgArr).toJson(QJsonDocument::Compact));
        }
        if (!GoodsDAO::update(existing, err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::DeleteGoods) {
        if (!GoodsManager::instance().deleteGoods(p.value("goodsId").toInt(), uid, err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::OffShelf) {
        if (!GoodsManager::instance().offShelf(p.value("goodsId").toInt(), uid, err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::OnShelf) {
        if (!GoodsManager::instance().onShelf(p.value("goodsId").toInt(), uid, err)) return {};
        return QJsonObject{};
    }

    // ==================== 收藏 ====================

    if (cmd == cs::cmd::FavoriteList) {
        QJsonArray arr;
        // 返回收藏的商品列表（客户端卡片直接渲染）；已删除的商品跳过
        for (const Favorite &f : FavoritesDAO::getByUser(uid)) {
            const Goods g = GoodsDAO::getById(f.goodsId);
            if (g.id != 0) arr.append(goodsJsonWithContext(g));
        }
        return QJsonObject{ {"goods", arr} };
    }

    if (cmd == cs::cmd::FavoriteAdd) {
        if (!FavoritesDAO::add(uid, p.value("goodsId").toInt())) {
            err = QStringLiteral("收藏失败"); return {};
        }
        return QJsonObject{};
    }

    if (cmd == cs::cmd::FavoriteRemove) {
        FavoritesDAO::remove(uid, p.value("goodsId").toInt());
        return QJsonObject{};
    }

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

    if (cmd == cs::cmd::Redeem) {
        double newBal = 0;
        if (!RedeemCodesDAO::redeem(p.value("code").toString().trimmed(),
                                    uid, newBal, err)) return {};
        return QJsonObject{ {"balance", newBal} };
    }

    if (cmd == cs::cmd::BalanceLogs) {
        QJsonArray arr;
        for (const BalanceLog &l : BalanceLogsDAO::getByUser(uid)) {
            arr.append(jsoncv::balanceLogToJson(l));
        }
        return QJsonObject{ {"logs", arr} };
    }

    // ==================== 举报（提交人人可为）====================

    if (cmd == cs::cmd::ReportSubmit) {
        Report r;
        r.reporterId  = uid;
        r.targetType  = p.value("targetType").toString();
        r.targetId    = p.value("targetId").toInt();
        r.reasonType  = p.value("reasonType").toString();
        r.description = p.value("description").toString().trimmed();
        if (r.description.size() < 10) {
            err = QStringLiteral("详细描述至少10字"); return {};
        }
        if (!ReportsDAO::insert(r, err)) return {};
        return QJsonObject{};
    }

    // ==================== 管理员专区 ====================
    if (!requireAdmin(*this, token, uid, err)) return {};

    if (cmd == cs::cmd::ListUsers) {
        QJsonArray arr;
        for (const User &u : UserManager::instance().listAllUsers()) {
            arr.append(jsoncv::userToJson(u));
        }
        return QJsonObject{ {"users", arr} };
    }

    if (cmd == cs::cmd::BanUser) {
        const int targetId = p.value("userId").toInt();
        if (!UserManager::instance().banUser(targetId, err)) return {};
        // 封禁联动：在售商品自动下架 + 现有会话失效
        GoodsManager::instance().autoOffShelfBySeller(targetId);
        revokeTokensOfUser(targetId);
        // M6：向被禁用户的在线连接实时推送踢下线（客户端收到后弹窗并退回登录窗）
        PushRegistry::instance().pushToUser(targetId, cs::push::Kick,
            QJsonObject{ {"reason", QStringLiteral("您的账号已被管理员封禁，请联系管理员。")} });
        return QJsonObject{};
    }

    if (cmd == cs::cmd::UnbanUser) {
        if (!UserManager::instance().unbanUser(p.value("userId").toInt(), err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::DeleteUser) {
        if (!UserManager::instance().deleteUser(p.value("userId").toInt(), err)) return {};
        return QJsonObject{};
    }

    // ---- 分类管理 ----
    if (cmd == cs::cmd::CategoryAdd) {
        Category c;
        c.name      = p.value("name").toString().trimmed();
        c.sortOrder = p.value("sortOrder").toInt();
        if (!CategoriesDAO::insert(c, err)) return {};
        return QJsonObject{};
    }
    if (cmd == cs::cmd::CategoryUpdate) {
        Category c;
        c.id        = p.value("id").toInt();
        c.name      = p.value("name").toString().trimmed();
        c.sortOrder = p.value("sortOrder").toInt();
        if (!CategoriesDAO::update(c, err)) return {};
        return QJsonObject{};
    }
    if (cmd == cs::cmd::CategoryRemove) {
        if (!CategoriesDAO::remove(p.value("id").toInt(), err)) return {};
        return QJsonObject{};
    }

    // ---- 手续费/系数设置 ----
    if (cmd == cs::cmd::SettingsGet) {
        QJsonObject coef;
        coef.insert("全新",     SettingsDAO::conditionCoef(QStringLiteral("全新")));
        coef.insert("九成新",   SettingsDAO::conditionCoef(QStringLiteral("九成新")));
        coef.insert("八成新",   SettingsDAO::conditionCoef(QStringLiteral("八成新")));
        coef.insert("七成新",   SettingsDAO::conditionCoef(QStringLiteral("七成新")));
        coef.insert("六成新及以下", SettingsDAO::conditionCoef(QStringLiteral("六成新及以下")));
        return QJsonObject{
            {"feeThreshold", SettingsDAO::feeThreshold()},
            {"feeRate", SettingsDAO::feeRate()},
            {"timeoutDays", SettingsDAO::transactionTimeoutDays()},
            {"coef", coef}
        };
    }
    if (cmd == cs::cmd::SettingsSet) {
        if (p.contains("feeThreshold")) SettingsDAO::set(
            "fee_threshold", QString::number(p.value("feeThreshold").toDouble()));
        if (p.contains("feeRate")) SettingsDAO::set(
            "fee_rate", QString::number(p.value("feeRate").toDouble()));
        if (p.contains("timeoutDays")) SettingsDAO::set(
            "transaction_timeout_days", QString::number(p.value("timeoutDays").toInt()));
        if (p.contains("coef")) {
            const QJsonObject coef = p.value("coef").toObject();
            for (auto it = coef.begin(); it != coef.end(); ++it) {
                SettingsDAO::set("coef_" + it.key(), QString::number(it.value().toDouble()));
            }
        }
        return QJsonObject{};
    }

    // ---- 充值码生成 ----
    if (cmd == cs::cmd::RedeemGenerate) {
        QStringList codes;
        if (!RedeemCodesDAO::generateBatch(p.value("amount").toDouble(),
                                           p.value("count").toInt(), codes, err)) {
            return {};
        }
        QJsonArray arr;
        for (const QString &c : codes) arr.append(c);
        return QJsonObject{ {"codes", arr} };
    }

    // ---- 举报处理 ----
    if (cmd == cs::cmd::ReportList) {
        QJsonArray arr;
        for (const Report &r : ReportsDAO::getAll(p.value("status").toString())) {
            arr.append(reportJsonWithContext(r));
        }
        return QJsonObject{ {"reports", arr} };
    }

    if (cmd == cs::cmd::ReportHandle) {
        const int reportId = p.value("reportId").toInt();
        const QString newStatus = p.value("newStatus").toString();
        const QString note = p.value("note").toString().trimmed();

        // resolved：若目标是商品且在售 → 举报下架 + 通知卖家（消息由事务外发送）
        if (newStatus == "resolved") {
            Report r = ReportsDAO::getById(reportId);
            if (r.targetType == "goods") {
                Goods g = GoodsDAO::getById(r.targetId);
                QString e2;
                if (g.id != 0 && g.isOnSale()) {
                    // 举报处理 + 下架 放一个事务里
                    QSqlDatabase db = DatabaseManager::instance().db();
                    if (db.transaction()) {
                        ReportsDAO::handle(reportId, uid, "resolved", note, e2);
                        if (!GoodsDAO::offShelfByReport(g.id, e2)) {
                            db.rollback();
                            err = e2; return {};
                        }
                        db.commit();
                        MessagesDAO::insert(g.sellerId,
                            QStringLiteral("🚩 商品因举报被下架"),
                            QStringLiteral("你的商品「%1」经管理员审核后已被下架，暂时无法自行重新上架。\n处理备注：%2")
                                .arg(g.title, note.isEmpty() ? QStringLiteral("（无）") : note),
                            "system");
                        return QJsonObject{};
                    }
                }
            }
        }
        if (!ReportsDAO::handle(reportId, uid, newStatus, note, err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::ForceOffReport) {
        if (!GoodsDAO::forceOffShelfReport(p.value("goodsId").toInt(), err)) return {};
        return QJsonObject{};
    }

    if (cmd == cs::cmd::RestoreReport) {
        if (!GoodsDAO::restoreFromReport(p.value("goodsId").toInt(), err)) return {};
        return QJsonObject{};
    }

    // ---- 交易管理 ----






    // ---- 余额统计 ----


    err = QStringLiteral("未实现命令: %1").arg(cmd);
    return {};
}