#ifndef GOODSMANAGER_H
#define GOODSMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>
#include "../models/Models.h"
#include "../dao/GoodsDAO.h"

class GoodsManager
{
public:
    static GoodsManager &instance();

    // ===== 推荐价 =====
    /**
     * @brief 根据磨损程度 + 原价算推荐价（向下取整到元）
     */
    static double suggestPrice(const QString &condition, double originalPrice);

    // ===== 发布 =====
    bool publishGoods(int sellerId,
                      const QString &title, const QString &desc,
                      double originalPrice, const QString &condition,
                      double currentPrice, int categoryId,
                      const QString &contact,
                      const QStringList &imageLocalPaths,  // 用户选的本地临时路径
                      QString &errorMsg);

    // ===== 编辑 =====
    bool editGoods(int goodsId, int operatorId,  // operatorId 必须是卖家
                   const QString &title, const QString &desc,
                   double originalPrice, const QString &condition,
                   double currentPrice, int categoryId,
                   const QString &contact,
                   const QStringList &imageLocalPaths,
                   QString &errorMsg);

    // ===== 状态操作 =====
    bool offShelf(int goodsId, int operatorId, QString &errorMsg);
    bool onShelf(int goodsId, int operatorId, QString &errorMsg);

    // ===== 删除 =====
    bool deleteGoods(int goodsId, int operatorId, QString &errorMsg);

    // ===== 查询 =====
    QList<Goods> browseOnSale(const GoodsDAO::QueryOptions &opt);
    QList<Goods> myGoods(int sellerId);

    // ===== 卖家封禁时自动下架 =====

    // ===== 敏感词检查 =====
    static bool containsSensitiveWord(const QString &text, QString &hitWord);

private:
    GoodsManager() = default;
};

#endif // GOODSMANAGER_H
