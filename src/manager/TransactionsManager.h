#ifndef TRANSACTIONSMANAGER_H
#define TRANSACTIONSMANAGER_H

#include <QString>
#include <QList>
#include "../models/Models.h"

class TransactionsManager
{
public:
    static TransactionsManager &instance();

    // ===== 手续费计算 =====
    static double calculateFee(double amount);

    // ===== 购买（下单+支付）=====
    /**
     * @brief 买家购买商品（完整支付流程，事务保障）
     * @param buyerId 买家
     * @param goodsId 商品
     * @param remark 可选备注
     */
    bool purchaseGoods(int buyerId, int goodsId, const QString &remark,
                       QString &errorMsg);

    // ===== 取消交易（自动退款）=====
    /**
     * @brief 取消交易并全额退款，买卖双方均可操作
     */

    // ===== 确认收货（仅买家）=====

    // ===== 管理员强制退款 =====
    /**
     * @brief 管理员对已付款交易强制退款（举报/争议处理）
     * @param adminId 操作管理员 id（写入流水备注与消息，留审计）
     * 仅支持 paid 状态；退款后商品置"举报下架"（卖家不可自行上架）
     * 托管模式：货款从平台托管直接退回买家，不依赖卖家余额
     */

    // ===== 超时自动结算（托管模式）=====
    /**
     * @brief 扫描超过 transactionTimeoutDays 天的已付款交易，自动结算给卖家（等同确认收货）
     * @param maxCount 本批最多结算笔数（>0 时限流分批，避免长时间独占写锁；0=不限制）
     * @return 本次结算笔数（由服务端定时任务在持锁状态下调用）
     */

    // ===== 查询 =====
    QList<Transaction> myBuys(int userId);
    QList<Transaction> mySales(int userId);
    Transaction getTransaction(int id);

private:
    TransactionsManager() = default;

    /**
     * @brief 内部：执行退款事务（被cancel/refundByAdmin共用）
     * @param reportOffShelf true=退款后商品置"举报下架"（管理员强制退款），false=回到在售（协商取消）
     * 托管模式：买家货款从托管直接退回；若该交易无托管记录（旧版数据）会失败
     */

    /**
     * @brief 内部：托管结算事务（被 confirmReceived/autoSettleExpired 共用）
     * 乐观锁 paid→done + 托管 held→released + 卖家入账(价-费) + 商品 sold + 双方消息
     */
};

#endif // TRANSACTIONSMANAGER_H
