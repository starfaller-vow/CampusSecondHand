#ifndef ESCROWDAO_H
#define ESCROWDAO_H

#include <QString>

/**
 * @brief 平台资金托管表 DAO（担保交易）
 * 买家付款后货款存入 escrow(status='held')；
 * 确认收货/超时自动结算 → released（打给卖家）；
 * 取消/管理员退款 → refunded（退回买家）。
 */
class EscrowDAO
{
public:
    // 创建托管记录（status='held'）
    static bool create(int txId, double amount, double fee, QString &errorMsg);

    /**
     * @brief 乐观锁流转托管状态：仅当 status='held' 时改为 toStatus
     * @param amountOut 成功时回填托管金额
     * @param feeOut    成功时回填手续费
     * @return 失败原因含"旧版数据"提示（老库 paid 交易无托管记录）
     */

    // 托管中资金总额（管理员统计用）
};

#endif // ESCROWDAO_H