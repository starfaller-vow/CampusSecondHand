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

    // 事务启动失败时立即返回，避免后续语句在无事务状态下执行
    if (!db.transaction()) {
        err = QStringLiteral("无法开启充值事务");
        return false;
    }

    // 查询充值码基本信息（不在这里判断 used，避免“先查后改”的并发窗口）
    QSqlQuery q(db);
    q.prepare("SELECT id, amount FROM redeem_codes WHERE code = ?");
    q.addBindValue(code);
    if (!q.exec()) {
        err = q.lastError().text();
        db.rollback();
        return false;
    }
    if (!q.next()) {
        err = QStringLiteral("充值码无效");
        db.rollback();
        return false;
    }
    const int id = q.value(0).toInt();
    const double amount = q.value(1).toDouble();
    if (amount <= 0) {
        err = QStringLiteral("充值码金额无效");
        db.rollback();
        return false;
    }

    // 条件更新：只有未使用的充值码才能被当前用户占用，保证并发下只成功一次
    q.prepare("UPDATE redeem_codes SET used=1, used_by=?, used_time=datetime('now','localtime') "
              "WHERE id=? AND used=0");
    q.addBindValue(userId);
    q.addBindValue(id);
    if (!q.exec()) {
        err = q.lastError().text();
        db.rollback();
        return false;
    }
    if (q.numRowsAffected() != 1) {
        err = QStringLiteral("该充值码已被使用");
        db.rollback();
        return false;
    }

    // 给用户加余额（外层已开事务，传 false 避免嵌套）
    double newBal = 0;
    if (!UserDAO::changeBalance(userId, amount, newBal, err, false)) {
        db.rollback();
        return false;
    }

    // 写余额流水：写入失败时必须回滚，不能出现“加了余额但没有流水”
    BalanceLog l;
    l.userId = userId; l.type = "recharge"; l.amount = amount;
    l.balanceAfter = newBal; l.refId = code; l.remark = QStringLiteral("充值码兑换");
    QString logErr;
    if (!BalanceLogsDAO::insert(l, logErr)) {
        err = QStringLiteral("余额流水写入失败：%1").arg(logErr);
        db.rollback();
        return false;
    }

    // 提交失败时也要回滚，避免连接停留在未完成的事务中
    if (!db.commit()) {
        err = db.lastError().text();
        db.rollback();
        return false;
    }

    outBalance = newBal;
    return true;
}
