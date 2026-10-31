#include "TransactionsDAO.h"
#include "../db/DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>

Transaction TransactionsDAO::getById(int id)
{
    Transaction t;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM transactions WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        t.id           = q.value("id").toInt();
        t.buyerId      = q.value("buyer_id").toInt();
        t.sellerId     = q.value("seller_id").toInt();
        t.goodsId      = q.value("goods_id").toInt();
        t.status       = q.value("status").toString();
        t.remark       = q.value("remark").toString();
        t.feeAmount    = q.value("fee_amount").toDouble();
        t.feeRefunded  = q.value("fee_refunded").toInt();
        t.createTime   = q.value("create_time").toString();
        t.finishTime   = q.value("finish_time").toString();
    }
    return t;
}

QList<Transaction> TransactionsDAO::getByBuyer(int buyerId)
{
    QList<Transaction> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM transactions WHERE buyer_id = ? ORDER BY id DESC");
    q.addBindValue(buyerId);
    if (q.exec()) {
        while (q.next()) {
            Transaction t;
            t.id           = q.value("id").toInt();
            t.buyerId      = q.value("buyer_id").toInt();
            t.sellerId     = q.value("seller_id").toInt();
            t.goodsId      = q.value("goods_id").toInt();
            t.status       = q.value("status").toString();
            t.remark       = q.value("remark").toString();
            t.feeAmount    = q.value("fee_amount").toDouble();
            t.feeRefunded  = q.value("fee_refunded").toInt();
            t.createTime   = q.value("create_time").toString();
            t.finishTime   = q.value("finish_time").toString();
            list.append(t);
        }
    }
    return list;
}

QList<Transaction> TransactionsDAO::getBySeller(int sellerId)
{
    QList<Transaction> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM transactions WHERE seller_id = ? ORDER BY id DESC");
    q.addBindValue(sellerId);
    if (q.exec()) {
        while (q.next()) {
            Transaction t;
            t.id           = q.value("id").toInt();
            t.buyerId      = q.value("buyer_id").toInt();
            t.sellerId     = q.value("seller_id").toInt();
            t.goodsId      = q.value("goods_id").toInt();
            t.status       = q.value("status").toString();
            t.remark       = q.value("remark").toString();
            t.feeAmount    = q.value("fee_amount").toDouble();
            t.feeRefunded  = q.value("fee_refunded").toInt();
            t.createTime   = q.value("create_time").toString();
            t.finishTime   = q.value("finish_time").toString();
            list.append(t);
        }
    }
    return list;
}

QList<Transaction> TransactionsDAO::getPaidByGoods(int goodsId)
{
    QList<Transaction> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM transactions WHERE goods_id = ? AND status = 'paid' ORDER BY id DESC");
    q.addBindValue(goodsId);
    if (q.exec()) {
        while (q.next()) {
            Transaction t;
            t.id           = q.value("id").toInt();
            t.buyerId      = q.value("buyer_id").toInt();
            t.sellerId     = q.value("seller_id").toInt();
            t.goodsId      = q.value("goods_id").toInt();
            t.status       = q.value("status").toString();
            t.remark       = q.value("remark").toString();
            t.feeAmount    = q.value("fee_amount").toDouble();
            t.feeRefunded  = q.value("fee_refunded").toInt();
            t.createTime   = q.value("create_time").toString();
            t.finishTime   = q.value("finish_time").toString();
            list.append(t);
        }
    }
    return list;
}





bool TransactionsDAO::insert(Transaction &t, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare(
        "INSERT INTO transactions (buyer_id, seller_id, goods_id, status, remark, fee_amount) "
        "VALUES (?, ?, ?, ?, ?, ?)"
    );
    q.addBindValue(t.buyerId);
    q.addBindValue(t.sellerId);
    q.addBindValue(t.goodsId);
    q.addBindValue(t.status.isEmpty() ? QString("pending") : t.status);
    q.addBindValue(t.remark);
    q.addBindValue(t.feeAmount);

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    // 回填自增 id，调用方（purchaseGoods）后续要用 t.id 更新状态
    t.id = q.lastInsertId().toInt();
    return true;
}

bool TransactionsDAO::updateStatus(int id, const QString &status, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE transactions SET status = ? WHERE id = ?");
    q.addBindValue(status);
    q.addBindValue(id);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    // 防止“静默0行”：id不存在时 UPDATE 成功但没改到任何行
    if (q.numRowsAffected() != 1) {
        errorMsg = QStringLiteral("更新状态失败：交易不存在（id=%1）").arg(id);
        return false;
    }
    return true;
}






