#include "FavoritesDAO.h"
#include "../db/DatabaseManager.h"
#include <QSqlQuery>

bool FavoritesDAO::add(int userId, int goodsId)
//把某件商品加入某个用户的收藏夹。如果已经收藏过，就忽略这次插入，不会产生重复记录
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT OR IGNORE INTO favorites (user_id, goods_id) VALUES (?, ?)");
    q.addBindValue(userId); q.addBindValue(goodsId);
    return q.exec();
}

bool FavoritesDAO::remove(int userId, int goodsId)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("DELETE FROM favorites WHERE user_id=? AND goods_id=?");
    q.addBindValue(userId); q.addBindValue(goodsId);
    return q.exec();
}

bool FavoritesDAO::exists(int userId, int goodsId)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT COUNT(*) FROM favorites WHERE user_id=? AND goods_id=?");
    q.addBindValue(userId); q.addBindValue(goodsId);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

QList<Favorite> FavoritesDAO::getByUser(int userId)
//	查询该用户的全部收藏记录
{
    QList<Favorite> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM favorites WHERE user_id=? ORDER BY id DESC");//按 id**倒序**（新收藏排在前面
    q.addBindValue(userId);
    if (!q.exec()) return list;
    while (q.next()) {
        Favorite f;
        f.id = q.value("id").toInt();
        f.userId = q.value("user_id").toInt();
        f.goodsId = q.value("goods_id").toInt();
        f.createTime = q.value("create_time").toString();
        list.append(f);
    }
    return list;
}
