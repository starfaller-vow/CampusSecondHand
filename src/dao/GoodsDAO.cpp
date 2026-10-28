#include "GoodsDAO.h"
#include "../db/DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>

// ==================== 辅助：从 QSqlQuery 读 Goods ====================
static Goods goodsFromQuery(QSqlQuery &q)
{
    Goods g;
    g.id            = q.value("id").toInt();
    g.sellerId      = q.value("seller_id").toInt();
    g.title         = q.value("title").toString();
    g.description   = q.value("description").toString();
    g.originalPrice = q.value("original_price").toDouble();
    g.condition     = q.value("condition").toString();
    g.currentPrice  = q.value("current_price").toDouble();
    g.categoryId    = q.value("category_id").toInt();
    g.imagesJson    = q.value("images").toString();
    g.status        = q.value("status").toString();
    g.offReason     = q.value("off_reason").toString();
    g.createTime    = q.value("create_time").toString();
    g.updateTime    = q.value("update_time").toString();
    return g;
}

// ==================== 查询 ====================

Goods GoodsDAO::getById(int id)//传入商品 id，**返回一件商品的实体对象**；查不到就返回空商品（id=0）
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM goods WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        return goodsFromQuery(q);
    }
    return Goods();//默认构造出来的 Goods，id 一般是 0。
}

QList<Goods> GoodsDAO::getBySeller(int sellerId)
{
    QList<Goods> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM goods WHERE seller_id = ? ORDER BY id DESC");
    q.addBindValue(sellerId);
    if (q.exec()) {
        while (q.next()) list.append(goodsFromQuery(q));
    }
    return list;
}

QList<Goods> GoodsDAO::getAllOnSale()
{
    QList<Goods> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM goods WHERE status = 'on_sale' ORDER BY create_time DESC");
    if (q.exec()) {
        while (q.next()) list.append(goodsFromQuery(q));
    }
    return list;
}

QList<Goods> GoodsDAO::query(const QueryOptions &opt)
{
    QList<Goods> list;

    QString sql = "SELECT * FROM goods WHERE 1=1";
    QList<QVariant> binds;

    // 状态
    if (!opt.status.isEmpty()) {
        sql += " AND status = ?";
        binds.append(opt.status);
    }
    // 关键词
    if (!opt.keyword.isEmpty()) {
        sql += " AND title LIKE ?";
        binds.append("%" + opt.keyword + "%");
    }
    // 分类
    if (opt.categoryId > 0) {
        sql += " AND category_id = ?";
        binds.append(opt.categoryId);
    }
    // 价格区间
    if (opt.minPrice > 0) {
        sql += " AND current_price >= ?";
        binds.append(opt.minPrice);
    }
    if (opt.maxPrice > 0) {
        sql += " AND current_price <= ?";
        binds.append(opt.maxPrice);
    }

    // 排序
    QString field = opt.sortField == "current_price" ? "current_price" : "create_time";
    QString order = opt.sortOrder == "ASC" ? "ASC" : "DESC";
    sql += QString(" ORDER BY %1 %2").arg(field, order);

    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare(sql);
    for (const QVariant &v : binds) q.addBindValue(v);

    if (q.exec()) {
        while (q.next()) list.append(goodsFromQuery(q));
    }
    return list;
}

// ==================== 增删改 ====================

bool GoodsDAO::insert(const Goods &g, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare(
        "INSERT INTO goods (seller_id, title, description, original_price, condition, "
        "current_price, category_id, images, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    q.addBindValue(g.sellerId);
    q.addBindValue(g.title);
    q.addBindValue(g.description);
    q.addBindValue(g.originalPrice);
    q.addBindValue(g.condition);
    q.addBindValue(g.currentPrice);
    q.addBindValue(g.categoryId);
    q.addBindValue(g.imagesJson);
    q.addBindValue(g.status.isEmpty() ? QString("on_sale") : g.status);

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}





// ==================== 状态流转 ====================










