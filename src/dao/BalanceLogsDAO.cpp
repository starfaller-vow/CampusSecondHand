#include "BalanceLogsDAO.h"
#include "../db/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>

bool BalanceLogsDAO::insert(const BalanceLog &l, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare(
        "INSERT INTO balance_logs (user_id, type, amount, balance_after, ref_id, remark) "
        "VALUES (?, ?, ?, ?, ?, ?)"
    );
    q.addBindValue(l.userId);
    q.addBindValue(l.type);
    q.addBindValue(l.amount);
    q.addBindValue(l.balanceAfter);
    q.addBindValue(l.refId);
    q.addBindValue(l.remark);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

QList<BalanceLog> BalanceLogsDAO::getByUser(int userId)
{
    QList<BalanceLog> list;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM balance_logs WHERE user_id = ? ORDER BY id DESC");
    q.addBindValue(userId);
    if (q.exec()) {
        while (q.next()) {
            BalanceLog l;
            l.id = q.value("id").toInt();
            l.userId = q.value("user_id").toInt();
            l.type = q.value("type").toString();
            l.amount = q.value("amount").toDouble();
            l.balanceAfter = q.value("balance_after").toDouble();
            l.refId = q.value("ref_id").toString();
            l.remark = q.value("remark").toString();
            l.createTime = q.value("create_time").toString();
            list.append(l);
        }
    }
    return list;
}
