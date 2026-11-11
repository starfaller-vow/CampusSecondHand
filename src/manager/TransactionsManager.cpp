#include "TransactionsManager.h"
#include "ReportReview.h"
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

double TransactionsManager::calculateFee(double amount)//`amount`：交易金额
{
    double threshold = SettingsDAO::feeThreshold();
    double rate      = SettingsDAO::feeRate();
    double taxable   = qMax(0.0, amount - threshold);//如果交易金额 ≤ 门槛，`amount - threshold` 是负数，`qMax` 会把 taxable 置为 0，也就是不用扣手续费
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
    BalanceLog log;
    log.userId = buyerId;
    log.type = "consume";
    log.amount = -g.currentPrice;
    log.balanceAfter = buyerNewBal;
    log.refId = QString::number(t.id);
    log.remark = QStringLiteral("购买商品（货款平台托管）：%1").arg(g.title);
    if (!BalanceLogsDAO::insert(log, balErr)) {
        rollback(QStringLiteral("余额流水写入失败：%1").arg(balErr));
        return false;
    }

    // 5. 提交事务
    if (!db.commit()) {
        rollback(db.lastError().text());
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

bool TransactionsManager::cancelTransaction(int transactionId, int operatorId,//订单编号，当前登录用户以及错误信息
                                             QString &errorMsg)
{
    Transaction t = TransactionsDAO::getById(transactionId);
    if (t.id == 0) { errorMsg = QStringLiteral("交易不存在"); return false; }
    if (t.status != "paid") {
        errorMsg = QStringLiteral("当前交易状态不可取消（仅已付款状态可取消）");
        return false;
    }
    // 操作人必须是买家或卖家
    if (operatorId != t.buyerId && operatorId != t.sellerId) {
        errorMsg = QStringLiteral("无权操作此交易");
        return false;
    }

    QString opName = (operatorId == t.buyerId) ? "买家" : "卖家";//判断是谁发起的取消订单，记录操作人的身份。
    return doRefund(transactionId, "cancelled",
                    QStringLiteral("%1主动取消交易").arg(opName), 0, errorMsg);
}

// ==================== 管理员强制退款 ====================

bool TransactionsManager::refundByAdmin(int transactionId, int adminId, QString &errorMsg)
{
    Transaction t = TransactionsDAO::getById(transactionId);
    if (t.id == 0) { errorMsg = QStringLiteral("交易不存在"); return false; }
    if (t.status != "paid") {
        errorMsg = QStringLiteral("当前交易状态不可退款");
        return false;
    }
    return doRefund(transactionId, "refunded",
                    QStringLiteral("管理员(id=%1)强制退款（举报/争议处理）").arg(adminId),
                    adminId, errorMsg);
}

// ==================== 内部：退款事务（托管模式）====================

bool TransactionsManager::doRefund(int transactionId, const QString &newStatus,
                                  const QString &reason, int refundAdminId,
                                  QString &errorMsg)
{
    Transaction t = TransactionsDAO::getById(transactionId);
    Goods g       = GoodsDAO::getById(t.goodsId);

    QSqlDatabase db = DatabaseManager::instance().db();
    if (!db.transaction()) {
        errorMsg = QStringLiteral("无法开启事务");
        return false;
    }

    auto rollback = [&](const QString &msg) { db.rollback(); errorMsg = msg; };
    QString e;

    // 0. 乐观锁抢占：仅当交易仍为 paid 时推进（防止与“确认收货/自动结算”等并发操作竞态）
    if (!TransactionsDAO::updateStatusIf(t.id, newStatus, "paid", e)) {
        rollback(e);
        return false;
    }

    // 1. 托管资金直接退回买家（不依赖卖家余额；无托管记录的旧版数据在此被保险拦截）
    double amount = 0, fee = 0;
    if (!EscrowDAO::transitionIfHeld(t.id, "refunded", amount, fee, e)) {
        rollback(e);
        return false;
    }
    Q_UNUSED(fee); // 托管模式：手续费从未实扣，无需退回

    // 2. 买家加回全部货款
    double buyerAfter;
    if (!UserDAO::changeBalance(t.buyerId, amount, buyerAfter, e, false)) {
        rollback(QStringLiteral("买家退款失败：") + e);
        return false;
    }

    if (refundAdminId > 0) {
        if (!GoodsDAO::forceOffShelfReport(t.goodsId, e)
            || !reportreview::finishRefund(t, refundAdminId, e)) {
            rollback(e);
            return false;
        }
    } else if (!reportreview::returnCancelledGoods(t, e)) {
        rollback(e);
        return false;
    }

    // 4. 写退款流水（托管模式：钱未经过卖家，仅买家有退款流水）
    BalanceLog l;
    l.userId       = t.buyerId;
    l.type         = "refund";
    l.amount       = amount;
    l.balanceAfter = buyerAfter;
    l.refId        = QString::number(t.id);
    l.remark       = QStringLiteral("退款（托管退回）：%1").arg(reason);
    if (!BalanceLogsDAO::insert(l, e)) {
        rollback(QStringLiteral("余额流水写入失败：%1").arg(e));
        return false;
    }

    if (!db.commit()) {
        rollback(db.lastError().text());
        return false;
    }

    // 5. 发消息（事务提交后）
    const QString title = (newStatus == "cancelled")
        ? QStringLiteral("❌ 交易已取消（全额退款）")
        : QStringLiteral("⚠️ 管理员强制退款");
    const QString content = QStringLiteral("交易「%1」因%2已全额退款。平台托管货款¥%3已原路退回买家账户。")
                                .arg(g.title, reason)
                                .arg(amount, 0, 'f', 2);
    MessagesDAO::insert(t.buyerId, title, content, "trade");
    MessagesDAO::insert(t.sellerId, title, content, "trade");

    return true;
}

// ==================== 内部：托管结算事务 ====================
// paid→done + 托管 released + 卖家入账(价-费) + 商品 sold + 双方消息

bool TransactionsManager::settleToDone(int transactionId, const QString &reason,
                                       QString &errorMsg)
{
    Transaction t = TransactionsDAO::getById(transactionId);
    if (t.id == 0) { errorMsg = QStringLiteral("交易不存在"); return false; }
    Goods g = GoodsDAO::getById(t.goodsId);

    QSqlDatabase db = DatabaseManager::instance().db();
    if (!db.transaction()) {
        errorMsg = QStringLiteral("无法开启事务");
        return false;
    }

    auto rollback = [&](const QString &msg) { db.rollback(); errorMsg = msg; };
    QString e;

    // 1. 乐观锁：paid → done（与退款操作互斥）
    if (!TransactionsDAO::updateStatusIf(t.id, "done", "paid", e)) {
        rollback(e);
        return false;
    }

    // 2. 托管资金 released（无托管记录的旧版数据在此被保险拦截，杜绝双付）
    double amount = 0, fee = 0;
    if (!EscrowDAO::transitionIfHeld(t.id, "released", amount, fee, e)) {
        rollback(e);
        return false;
    }

    // 3. 卖家入账：货款（手续费在入账时从卖家所得中扣）
    double sellerAfter;
    if (!UserDAO::changeBalance(t.sellerId, amount, sellerAfter, e, false)) {
        rollback(QStringLiteral("卖家收款失败：") + e);
        return false;
    }

    double sellerFinal = sellerAfter;
    if (fee > 0) {
        if (!UserDAO::changeBalance(t.sellerId, -fee, sellerFinal, e, false)) {
            rollback(QStringLiteral("手续费扣除失败：") + e);
            return false;
        }
    }

    // 4. 商品 sold + 完成时间
    if (!GoodsDAO::setStatus(t.goodsId, "sold", e) || !reportreview::finishSale(t.id, e)) {
        rollback(e);
        return false;
    }
    if (!TransactionsDAO::updateFinishTime(t.id,
            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"), e)) {
        rollback(e);
        return false;
    }

    // 5. 写结算流水
    BalanceLog log1;
    log1.userId       = t.sellerId;
    log1.type         = "sale";  // 卖家售出货款入账（区别于买家的 consume）
    log1.amount       = amount;
    log1.balanceAfter = sellerAfter;
    log1.refId        = QString::number(t.id);
    log1.remark       = QStringLiteral("货款结算到账（%1）：%2").arg(reason, g.title);
    if (!BalanceLogsDAO::insert(log1, e)) {
        rollback(QStringLiteral("余额流水写入失败：%1").arg(e));
        return false;
    }

    if (fee > 0) {
        BalanceLog log2;
        log2.userId       = t.sellerId;
        log2.type         = "fee_deduct";
        log2.amount       = -fee;
        log2.balanceAfter = sellerFinal;
        log2.refId        = QString::number(t.id);
        log2.remark       = QStringLiteral("平台手续费");
        if (!BalanceLogsDAO::insert(log2, e)) {
            rollback(QStringLiteral("余额流水写入失败：%1").arg(e));
            return false;
        }
    }

    if (!db.commit()) {
        rollback(db.lastError().text());
        return false;
    }

    // 6. 发消息（事务提交后）
    MessagesDAO::insert(t.sellerId,
        QStringLiteral("🎉 货款已结算到账"),
        QStringLiteral("交易「%1」%2，托管货款¥%3（手续费¥%4）已结算到你的账户。")
            .arg(g.title, reason)
            .arg(amount, 0, 'f', 2)
            .arg(fee, 0, 'f', 2),
        "trade");

    MessagesDAO::insert(t.buyerId,
        QStringLiteral("✅ 交易完成"),
        QStringLiteral("你购买的「%1」已完成结算（%2）。").arg(g.title, reason),
        "trade");

    return true;
}

// ==================== 超时自动结算 ====================

int TransactionsManager::autoSettleExpired(int maxCount)
//`maxCount`：单次最多处理多少条，防止一次性处理上万条把数据库压垮，定时器分批跑
{
    const int days = SettingsDAO::transactionTimeoutDays();
    int settled = 0;
    const QList<Transaction> expired = TransactionsDAO::getPaidExpired(days);
    for (const Transaction &t : expired) {
        if (maxCount > 0 && settled >= maxCount) break; // 本批限额：让锁后由定时器调度下一批
        QString e;
        const QString reason = QStringLiteral("买家超过%1天未确认收货，系统自动结算").arg(days);
        if (settleToDone(t.id, reason, e)) {
            settled++;
            qInfo() << "超时自动结算：交易" << t.id << "已打款给卖家" << t.sellerId;
        } else {
            qWarning() << "超时自动结算失败：交易" << t.id << "原因:" << e;
        }
    }
    return settled;
}

// ==================== 确认收货 ====================

bool TransactionsManager::confirmReceived(int transactionId, int buyerId,
                                          QString &errorMsg)
{
    Transaction t = TransactionsDAO::getById(transactionId);
    if (t.id == 0) { errorMsg = QStringLiteral("交易不存在"); return false; }
    if (t.buyerId != buyerId) { errorMsg = QStringLiteral("仅买家可确认收货"); return false; }
    if (t.status != "paid") {
        errorMsg = QStringLiteral("当前交易状态不可确认收货");
        return false;
    }

    // 托管结算：paid→done + 托管 released + 卖家入账 + 商品 sold（事务内原子完成）
    return settleToDone(transactionId, QStringLiteral("买家确认收货"), errorMsg);
}

// ==================== 查询 ====================

QList<Transaction> TransactionsManager::myBuys(int userId)
//myBuys：**我买到的订单**，根据用户 ID，查询所有该用户作为买家的交易记录
{
    return TransactionsDAO::getByBuyer(userId);
}

QList<Transaction> TransactionsManager::mySales(int userId)
//mySales：**我卖出的订单**，查询该用户作为卖家的所有交易记录
{
    return TransactionsDAO::getBySeller(userId);
}

Transaction TransactionsManager::getTransaction(int id)
//mySales：**我卖出的订单**，查询该用户作为卖家的所有交易记录
{
    return TransactionsDAO::getById(id);
}
