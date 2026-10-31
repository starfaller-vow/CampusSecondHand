#include "EscrowDAO.h"
#include "../db/DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>

bool EscrowDAO::create(int txId, double amount, double fee, QString &errorMsg)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT INTO escrow (tx_id, amount, fee) VALUES (?, ?, ?)");
    q.addBindValue(txId);
    q.addBindValue(amount);
    q.addBindValue(fee);
    if (!q.exec()) {
        errorMsg = QStringLiteral("托管记录创建失败：") + q.lastError().text();
        return false;
    }
    return true;
}



