#include "RedeemCodesDAO.h"
#include "../db/DatabaseManager.h"
#include "UserDAO.h"
#include "BalanceLogsDAO.h"
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

bool RedeemCodesDAO::redeem(const QString &code, int userId, double &outBalance, QString &err)
{
    QSqlDatabase db = DatabaseManager::instance().db();
    db.transaction();

    QSqlQuery q(db);
    q.prepare("SELECT id, amount, used FROM redeem_codes WHERE code = ?");
    q.addBindValue(code);
    if (!q.exec() || !q.next()) { err = QStringLiteral("充值码无效"); db.rollback(); return false; }
    int id = q.value(0).toInt();
    double amount = q.value(1).toDouble();
    int used = q.value(2).toInt();
    if (used) { err = QStringLiteral("该充值码已被使用"); db.rollback(); return false; }

    // 标记已使用
    q.prepare("UPDATE redeem_codes SET used=1, used_by=?, used_time=datetime('now','localtime') WHERE id=?");
    q.addBindValue(userId); q.addBindValue(id);
    if (!q.exec()) { err = q.lastError().text(); db.rollback(); return false; }

    // 给用户加余额（外层已开事务，传 false 避免嵌套）
    double newBal;
    if (!UserDAO::changeBalance(userId, amount, newBal, err, false)) { db.rollback(); return false; }

    // 写流水
    BalanceLog l;
    l.userId = userId; l.type = "recharge"; l.amount = amount;
    l.balanceAfter = newBal; l.refId = code; l.remark = QStringLiteral("充值码兑换");
    QString e; BalanceLogsDAO::insert(l, e);

    db.commit();
    outBalance = newBal;
    return true;
}
