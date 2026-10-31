#ifndef GOODSDAO_H
#define GOODSDAO_H

#include <QString>
#include <QList>
#include "../models/Models.h"

/**
 * @brief 商品数据访问层
 */
class GoodsDAO
{
public:
    // ===== 查询 =====
    static Goods getById(int id);
    static QList<Goods> getBySeller(int sellerId);
    static QList<Goods> getAllOnSale();

    /**
     * @brief 条件查询（搜索/分类/价格/排序）
     */
    struct QueryOptions {
        QString keyword;    // 标题关键词（空=不限）
        int categoryId = 0; // 0=不限
        double minPrice = 0; // 0=不限
        double maxPrice = 0; // 0=不限
        QString sortField = "create_time"; // create_time / current_price
        QString sortOrder = "DESC";        // ASC / DESC
        QString status = "on_sale";        // 默认只看在售
    };
    static QList<Goods> query(const QueryOptions &opt);

    // ===== 增删改 =====
    static bool insert(const Goods &g, QString &errorMsg);
    static bool update(const Goods &g, QString &errorMsg);
    static bool deleteById(int id, QString &errorMsg);  // 硬删除

    // ===== 状态流转 =====
    static bool setStatus(int goodsId, const QString &status, QString &errorMsg);

    // ===== 在售商品被付款后锁定 =====
    static bool tryReserve(int goodsId, QString &errorMsg);

    // ===== 举报下架 / 管理员恢复 =====
    // 仅当商品仍为 on_sale 时下架成功（乐观锁：1=成功下架，0=未在售没动）
    static bool offShelfByReport(int goodsId, QString &errorMsg);
    // 无条件置为"举报下架"（不要求当前在售，供管理员强制退款/强制下架使用）
    static bool forceOffShelfReport(int goodsId, QString &errorMsg);
    // 仅当商品 off_reason='report' 时恢复为 on_sale 并清空原因（返回1行才算成功）
    static bool restoreFromReport(int goodsId, QString &errorMsg);
};

#endif // GOODSDAO_H
