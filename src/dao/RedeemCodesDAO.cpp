#include "RedeemCodesDAO.h"
#include "../db/DatabaseManager.h"
#include "UserDAO.h"
#include <QSqlQuery>
#include <QRandomGenerator>
#include <QStringList>
#include <QCoreApplication>

bool RedeemCodesDAO::insert(const RedeemCode &c, QString &err)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT INTO redeem_codes (code, amount) VALUES (?, ?)");
    q.addBindValue(c.code); q.addBindValue(c.amount);
    if (!q.exec()) { err = q.lastError().text(); return false; }
    return true;
}

bool RedeemCodesDAO::generateBatch(double amount, int count, QStringList &outCodes, QString &err)
{
    // 清空输出列表，准备存放新生成的充值码
    outCodes.clear();
    // 循环count次，生成count个充值码
    for (int i = 0; i < count; ++i) {
        QString suffix;
        // 生成8位随机后缀：0~9、A~Z，共36个字符（36进制）
        for (int j = 0; j < 8; ++j) {
            // 随机取 [0,36) 的整数，0到35
            int r = QRandomGenerator::global()->bounded(0, 36);
            if (r < 10) suffix.append(QChar('0' + r));
            else suffix.append(QChar('A' + r - 10));
        }
        QString code = QString("CAMPUS%1-%2").arg((int)amount).arg(suffix);

        RedeemCode c;
        c.code = code; c.amount = amount;
        if (!insert(c, err)) return false;
        outCodes.append(code);
    }
    return true;
}


