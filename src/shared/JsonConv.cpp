#include "JsonConv.h"

namespace jsoncv {

// ---------------- User ----------------

QJsonObject userToJson(const User &u)
{
    QJsonObject o;
    o.insert("id", u.id);
    o.insert("username", u.username);
    o.insert("role", u.role);
    o.insert("studentNo", u.studentNo);
    o.insert("college", u.college);
    o.insert("contact", u.contact);
    o.insert("balance", u.balance);
    o.insert("status", u.status);
    o.insert("createTime", u.createTime);
    return o;
    // 注意：passwordHash 永不随网络传输
}

User userFromJson(const QJsonObject &o)
{
    User u;
    u.id         = o.value("id").toInt();
    u.username   = o.value("username").toString();
    u.role       = o.value("role").toString();
    u.studentNo  = o.value("studentNo").toString();
    u.college    = o.value("college").toString();
    u.contact    = o.value("contact").toString();
    u.balance    = o.value("balance").toDouble();
    u.status     = o.value("status").toString();
    u.createTime = o.value("createTime").toString();
    return u;
}

// ---------------- Goods ----------------

QJsonObject goodsToJson(const Goods &g)
{
    QJsonObject o;
    o.insert("id", g.id);
    o.insert("sellerId", g.sellerId);
    o.insert("title", g.title);
    o.insert("description", g.description);
    o.insert("originalPrice", g.originalPrice);
    o.insert("condition", g.condition);
    o.insert("currentPrice", g.currentPrice);
    o.insert("categoryId", g.categoryId);
    o.insert("imagesJson", g.imagesJson);
    o.insert("status", g.status);
    o.insert("offReason", g.offReason);
    o.insert("createTime", g.createTime);
    o.insert("updateTime", g.updateTime);
    return o;
}

Goods goodsFromJson(const QJsonObject &o)
{
    Goods g;
    g.id            = o.value("id").toInt();
    g.sellerId      = o.value("sellerId").toInt();
    g.title         = o.value("title").toString();
    g.description   = o.value("description").toString();
    g.originalPrice = o.value("originalPrice").toDouble();
    g.condition     = o.value("condition").toString();
    g.currentPrice  = o.value("currentPrice").toDouble();
    g.categoryId    = o.value("categoryId").toInt();
    g.imagesJson    = o.value("imagesJson").toString();
    g.status        = o.value("status").toString();
    g.offReason     = o.value("offReason").toString();
    g.createTime    = o.value("createTime").toString();
    g.updateTime    = o.value("updateTime").toString();
    // 展示字段（服务端下发的冗余信息，用于卡片直接显示）
    g.categoryName  = o.value("categoryName").toString();
    g.sellerName    = o.value("sellerName").toString();
    return g;
}

// ---------------- Category ----------------

QJsonObject categoryToJson(const Category &c)
{
    return QJsonObject{
        {"id", c.id},
        {"name", c.name},
        {"sortOrder", c.sortOrder}
    };
}

Category categoryFromJson(const QJsonObject &o)
{
    Category c;
    c.id        = o.value("id").toInt();
    c.name      = o.value("name").toString();
    c.sortOrder = o.value("sortOrder").toInt();
    return c;
}

// ---------------- Transaction ----------------

QJsonObject transactionToJson(const Transaction &t)
{
    return QJsonObject{
        {"id", t.id},
        {"buyerId", t.buyerId},
        {"sellerId", t.sellerId},
        {"goodsId", t.goodsId},
        {"status", t.status},
        {"remark", t.remark},
        {"feeAmount", t.feeAmount},
        {"feeRefunded", t.feeRefunded},
        {"createTime", t.createTime},
        {"finishTime", t.finishTime}
    };
}

Transaction transactionFromJson(const QJsonObject &o)
{
    Transaction t;
    t.id          = o.value("id").toInt();
    t.buyerId     = o.value("buyerId").toInt();
    t.sellerId    = o.value("sellerId").toInt();
    t.goodsId     = o.value("goodsId").toInt();
    t.status      = o.value("status").toString();
    t.remark      = o.value("remark").toString();
    t.feeAmount   = o.value("feeAmount").toDouble();
    t.feeRefunded = o.value("feeRefunded").toInt();
    t.createTime  = o.value("createTime").toString();
    t.finishTime  = o.value("finishTime").toString();
    // 展示附带字段（列表接口才有，缺失时保持默认值，不影响详情等接口）
    t.goodsTitle    = o.value("goodsTitle").toString();
    t.goodsPrice    = o.value("goodsPrice").toDouble();
    t.goodsExists   = o.value("goodsExists").toBool();
    t.goodsStatus   = o.value("goodsStatus").toString();
    t.goodsOffReason = o.value("goodsOffReason").toString();
    t.peerName      = o.value("peerName").toString();
    t.peerStudentNo = o.value("peerStudentNo").toString();
    t.buyerName      = o.value("buyerName").toString();
    t.buyerStudentNo = o.value("buyerStudentNo").toString();
    t.sellerName      = o.value("sellerName").toString();
    t.sellerStudentNo = o.value("sellerStudentNo").toString();
    return t;
}

// ---------------- Message ----------------

QJsonObject messageToJson(const Message &m)
{
    return QJsonObject{
        {"id", m.id},
        {"userId", m.userId},
        {"title", m.title},
        {"content", m.content},
        {"isRead", m.isRead},
        {"type", m.type},
        {"createTime", m.createTime}
    };
}

Message messageFromJson(const QJsonObject &o)
{
    Message m;
    m.id         = o.value("id").toInt();
    m.userId     = o.value("userId").toInt();
    m.title      = o.value("title").toString();
    m.content    = o.value("content").toString();
    m.isRead     = o.value("isRead").toInt();
    m.type       = o.value("type").toString();
    m.createTime = o.value("createTime").toString();
    return m;
}

// ---------------- Report ----------------

QJsonObject reportToJson(const Report &r)
{
    return QJsonObject{
        {"id", r.id},
        {"reporterId", r.reporterId},
        {"targetType", r.targetType},
        {"targetId", r.targetId},
        {"reasonType", r.reasonType},
        {"description", r.description},
        {"status", r.status},
        {"handlerId", r.handlerId},
        {"handlerNote", r.handlerNote},
        {"createTime", r.createTime},
        {"handleTime", r.handleTime}
    };
}

Report reportFromJson(const QJsonObject &o)
{
    Report r;
    r.id          = o.value("id").toInt();
    r.reporterId  = o.value("reporterId").toInt();
    r.targetType  = o.value("targetType").toString();
    r.targetId    = o.value("targetId").toInt();
    r.reasonType  = o.value("reasonType").toString();
    r.description = o.value("description").toString();
    r.status      = o.value("status").toString();
    r.handlerId   = o.value("handlerId").toInt();
    r.handlerNote = o.value("handlerNote").toString();
    r.createTime  = o.value("createTime").toString();
    r.handleTime  = o.value("handleTime").toString();
    r.targetTitle = o.value("targetTitle").toString();
    r.targetExists = o.value("targetExists").toBool();
    r.targetStatus = o.value("targetStatus").toString();
    r.targetOffReason = o.value("targetOffReason").toString();
    return r;
}

// ---------------- BalanceLog ----------------

QJsonObject balanceLogToJson(const BalanceLog &l)
{
    return QJsonObject{
        {"id", l.id},
        {"userId", l.userId},
        {"type", l.type},
        {"amount", l.amount},
        {"balanceAfter", l.balanceAfter},
        {"refId", l.refId},
        {"remark", l.remark},
        {"createTime", l.createTime}
    };
}

BalanceLog balanceLogFromJson(const QJsonObject &o)
{
    BalanceLog l;
    l.id           = o.value("id").toInt();
    l.userId       = o.value("userId").toInt();
    l.type         = o.value("type").toString();
    l.amount       = o.value("amount").toDouble();
    l.balanceAfter = o.value("balanceAfter").toDouble();
    l.refId        = o.value("refId").toString();
    l.remark       = o.value("remark").toString();
    l.createTime   = o.value("createTime").toString();
    return l;
}

} // namespace jsoncv