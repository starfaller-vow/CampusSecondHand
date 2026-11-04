#include "CategoriesDAO.h"
#include "../db/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>

QList<Category> CategoriesDAO::getAll()
{
    QList<Category> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.exec("SELECT * FROM categories ORDER BY sort_order ASC, id ASC");
    while (q.next()) {
        Category c;
        c.id = q.value("id").toInt();
        c.name = q.value("name").toString();
        c.sortOrder = q.value("sort_order").toInt();
        list.append(c);
    }
    return list;
}

Category CategoriesDAO::getById(int id)
{
    Category c;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM categories WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        c.id = q.value("id").toInt();
        c.name = q.value("name").toString();
        c.sortOrder = q.value("sort_order").toInt();
    }
    return c;
}

bool CategoriesDAO::insert(const Category &c, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT INTO categories (name, sort_order) VALUES (?, ?)");
    q.addBindValue(c.name);
    q.addBindValue(c.sortOrder);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool CategoriesDAO::update(const Category &c, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE categories SET name = ?, sort_order = ? WHERE id = ?");
    q.addBindValue(c.name);
    q.addBindValue(c.sortOrder);
    q.addBindValue(c.id);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

bool CategoriesDAO::remove(int id, QString &errorMsg)
{
    QSqlDatabase db = DatabaseManager::instance().db();

    // 引用保护：该分类下仍有商品时禁止删除，避免商品的外键分类悬空
    QSqlQuery check(db);
    check.prepare("SELECT COUNT(*) FROM goods WHERE category_id = ?");
    check.addBindValue(id);
    if (!check.exec() || !check.next()) {
        errorMsg = check.lastError().text();
        return false;
    }
    const int goodsCount = check.value(0).toInt();
    if (goodsCount > 0) {
        errorMsg = QStringLiteral("该分类下还有 %1 件商品，请先处理这些商品后再删除").arg(goodsCount);
        return false;
    }

    QSqlQuery q(db);
    q.prepare("DELETE FROM categories WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}
