#include "TransactionsManager.h"
#include "../dao/TransactionsDAO.h"
#include "../dao/GoodsDAO.h"
#include "../dao/UserDAO.h"
#include "../dao/SettingsDAO.h"
#include "../dao/BalanceLogsDAO.h"
#include "../dao/MessagesDAO.h"
#include "../dao/EscrowDAO.h"
#include "../db/DatabaseManager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

TransactionsManager &TransactionsManager::instance()
{
    static TransactionsManager inst;
    return inst;
}

// ==================== 手续费计算 ====================

double TransactionsManager::calculateFee(double amount)
{
    double threshold = SettingsDAO::feeThreshold();
    double rate      = SettingsDAO::feeRate();
    double taxable   = qMax(0.0, amount - threshold);
    return taxable * rate;
}

// ==================== 购买（核心！事务性支付）====================

bool TransactionsManager::purchaseGoods(int buyerId, int goodsId, const QString &remark,
                                        QString &errorMsg)
{
    // 1. 检查商品
    Goods g = GoodsDAO::getById(goodsId);
    if (g.id == 0) { errorMsg = QStringLiteral("商品不存在"); return false; }
    if (!g.isOnSale()) { errorMsg = QStringLiteral("商品已被他人购买或下架"); return false; }
    if (g.sellerId == buyerId) { errorMsg = QStringLiteral("不能购买自己的商品"); return false; }

    // 2. 检查买家余额
    User buyer = UserDAO::getById(buyerId);
    if (buyer.balance < g.currentPrice) {
        errorMsg = QStringLiteral("余额不足（需¥%1，当前¥%2）")
                       .arg(g.currentPrice, 0, 'f', 2)
                       .arg(buyer.balance, 0, 'f', 2);
        return false;
    }

    User seller = UserDAO::getById(g.sellerId);

    // 3. 计算手续费
    double fee = calculateFee(g.currentPrice);

    // 4. 开启事务，全程保证原子性
    QSqlDatabase db = DatabaseManager::instance().db();
    if (!db.transaction()) {
        errorMsg = QStringLiteral("无法开启事务");
        return false;
    }

    auto rollback = [&](const QString &e) {
        db.rollback();
        errorMsg = e;
        qWarning() << "purchaseGoods 失败:" << e;
    };

    // 4a. 乐观锁：锁定商品状态
    QString lockErr;
    if (!GoodsDAO::tryReserve(goodsId, lockErr)) {
        rollback(lockErr);
        return false;
    }

    // 4b. 创建交易记录 (pending 状态)
    Transaction t;
    t.buyerId  = buyerId;
    t.sellerId = seller.id;
    t.goodsId  = goodsId;
    t.status   = "pending";
    t.remark   = remark;
    t.feeAmount = fee;
    QString tErr;
    if (!TransactionsDAO::insert(t, tErr)) { rollback(tErr); return false; }

    // 4c. 买家扣余额（外层已开事务，传 false 避免嵌套）
    double buyerNewBal;
    QString balErr;
    if (!UserDAO::changeBalance(buyerId, -g.currentPrice, buyerNewBal, balErr, false)) {
        rollback(QStringLiteral("买家扣款失败: ") + balErr);
        return false;
    }

    // 4d. 货款进平台托管（担保交易）：卖家此时拿不到钱，确认收货/超时自动结算后才入账
    if (!EscrowDAO::create(t.id, g.currentPrice, fee, balErr)) {
        rollback(QStringLiteral("托管资金冻结失败: ") + balErr);
        return false;
    }

    // 4e. 更新交易状态为 paid
    QString updateErr;
    if (!TransactionsDAO::updateStatus(t.id, "paid", updateErr)) {
        rollback(updateErr);
        return false;
    }

    // 4f. 写余额流水（托管模式下仅买家付款流水；卖家结算流水在确认收货时产生）
    auto writeLog = [&](int uid, const QString &type, double amount,
                        double balAfter, const QString &ref, const QString &rmk) {
        BalanceLog l;
        l.userId       = uid;
        l.type         = type;
        l.amount       = amount;
        l.balanceAfter = balAfter;
        l.refId        = ref;
        l.remark       = rmk;
        QString e;
        BalanceLogsDAO::insert(l, e);
    };
    writeLog(buyerId, "consume", -g.currentPrice, buyerNewBal,
             QString::number(t.id), QStringLiteral("购买商品（货款平台托管）：%1").arg(g.title));

    // 5. 提交事务
    if (!db.commit()) {
        errorMsg = db.lastError().text();
        return false;
    }

    // 6. 发消息（事务提交后发，避免消息发了但事务回滚）
    MessagesDAO::insert(seller.id,
        QStringLiteral("🛒 有人买了你的商品"),
        QStringLiteral("买家 @%1 购买了「%2」，已付款¥%3。\n货款由平台托管，买家确认收货后将自动结算到你账户。")
            .arg(buyer.username, g.title)
            .arg(g.currentPrice, 0, 'f', 2),
        "trade");

    MessagesDAO::insert(buyerId,
        QStringLiteral("✅ 支付成功"),
        QStringLiteral("你购买的「%1」已付款¥%2，货款已由平台托管。\n请尽快与卖家 @%3 联系线下取货，确认收货后托管款才会打给卖家。")
            .arg(g.title)
            .arg(g.currentPrice, 0, 'f', 2)
            .arg(seller.username),
        "trade");

    qDebug() << "支付成功：交易ID=" << t.id
             << " 金额=" << g.currentPrice
             << " 手续费=" << fee
             << "（货款已托管）";
    return true;
}

// ==================== 取消交易（全额退款）====================



// ==================== 管理员强制退款 ====================



// ==================== 内部：退款事务（托管模式）====================



// ==================== 内部：托管结算事务 ====================
// paid→done + 托管 released + 卖家入账(价-费) + 商品 sold + 双方消息



// ==================== 超时自动结算 ====================



// ==================== 确认收货 ====================



// ==================== 查询 ====================

QList<Transaction> TransactionsManager::myBuys(int userId)
{
    return TransactionsDAO::getByBuyer(userId);
}

QList<Transaction> TransactionsManager::mySales(int userId)
{
    return TransactionsDAO::getBySeller(userId);
}

Transaction TransactionsManager::getTransaction(int id)
{
    return TransactionsDAO::getById(id);
}
