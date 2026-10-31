#ifndef TRANSACTIONSDAO_H
#define TRANSACTIONSDAO_H

#include <QList>
#include "../models/Models.h"

class TransactionsDAO
{
public:
    static Transaction getById(int id);
    static QList<Transaction> getByBuyer(int buyerId);
    static QList<Transaction> getBySeller(int sellerId);
    static QList<Transaction> getPaidByGoods(int goodsId);     // 按商品查"已付款"交易（举报强制退款用）
    // 查"已付款且超过 N 天"的交易（超时自动结算用）
    static bool insert(Transaction &t, QString &errorMsg); // 非const引用：成功后回填自增id到 t.id
    static bool updateStatus(int id, const QString &status, QString &errorMsg);
    // 乐观锁式状态流转：仅当当前状态==expectedStatus 时才更新（返回1行才算成功），防止并发竞态
};

#endif // TRANSACTIONSDAO_H
