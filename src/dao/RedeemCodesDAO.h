#ifndef REDEEMCODESDAO_H
#define REDEEMCODESDAO_H
#include "../models/Models.h"
#include <QString>
class RedeemCodesDAO {
public:
    static bool insert(const RedeemCode &c, QString &err);
    static bool generateBatch(double amount, int count, QStringList &outCodes, QString &err);
    /**
     * @brief 兑换充值码
     * @return true=成功兑换，outBalance=兑换后用户余额
     */
    static bool redeem(const QString &code, int userId, double &outBalance, QString &err);
};
#endif
