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

bool EscrowDAO::transitionIfHeld(int txId, const QString &toStatus,
                                 double &amountOut, double &feeOut,
                                 QString &errorMsg)
{
    // 1. 先取托管记录（老库数据无记录 → 保险拦截，杜绝双付/凭空退款）
    QSqlQuery sel(DatabaseManager::instance().db());
    sel.prepare("SELECT amount, fee, status FROM escrow WHERE tx_id = ?");
    sel.addBindValue(txId);
    if (!sel.exec()) {
        errorMsg = sel.lastError().text();
        return false;
    }
    if (!sel.next()) {
        errorMsg = QStringLiteral("该交易为旧版数据（无托管记录），不支持结算/退款，请删除 data.db 重置后重试");
        return false;
    }
    const QString curStatus = sel.value("status").toString();
    amountOut = sel.value("amount").toDouble();
    feeOut    = sel.value("fee").toDouble();
    if (curStatus != "held") {
        errorMsg = QStringLiteral("托管资金已%1，不可重复操作").arg(curStatus);
        return false;
    }

    // 2. 乐观锁：仅 held 可流转
    QSqlQuery up(DatabaseManager::instance().db());
    up.prepare("UPDATE escrow SET status = ?, finish_time = datetime('now','localtime') "
               "WHERE tx_id = ? AND status = 'held'");
    up.addBindValue(toStatus);
    up.addBindValue(txId);
    if (!up.exec()) {
        errorMsg = up.lastError().text();
        return false;
    }
    if (up.numRowsAffected() != 1) {
        errorMsg = QStringLiteral("托管状态已变化，请重试");
        return false;
    }
    return true;
}

std::optional<double> EscrowDAO::amountForTransaction(int txId)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT amount FROM escrow WHERE tx_id = ?");
    q.addBindValue(txId);
    if (q.exec() && q.next()) {
        return q.value(0).toDouble();
    }
    return std::nullopt;
}

double EscrowDAO::heldTotal()
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT COALESCE(SUM(amount), 0) FROM escrow WHERE status = 'held'");
    if (q.exec() && q.next()) {
        return q.value(0).toDouble();
    }
    return 0.0;
}
