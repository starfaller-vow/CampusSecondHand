#include "ReportsDAO.h"
#include "../db/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

bool ReportsDAO::insert(const Report &r, QString &err)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT INTO reports (reporter_id, target_type, target_id, reason_type, description) "
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(r.reporterId); q.addBindValue(r.targetType); q.addBindValue(r.targetId);
    q.addBindValue(r.reasonType); q.addBindValue(r.description);
    if (!q.exec()) { err = q.lastError().text(); return false; }
    return true;
}






