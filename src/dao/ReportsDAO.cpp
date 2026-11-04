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

QList<Report> ReportsDAO::getAll(const QString &status)
{
    QList<Report> list;
    QSqlQuery q(DatabaseManager::instance().db());
    QString sql = "SELECT * FROM reports";
    if (!status.isEmpty()) {
        sql += " WHERE status = ?";
        q.prepare(sql);
        q.addBindValue(status);
    } else {
        sql += " ORDER BY id DESC";
        q.prepare(sql);
    }
    if (q.exec()) {
        while (q.next()) {
            Report r;
            r.id = q.value("id").toInt();
            r.reporterId = q.value("reporter_id").toInt();
            r.targetType = q.value("target_type").toString();
            r.targetId = q.value("target_id").toInt();
            r.reasonType = q.value("reason_type").toString();
            r.description = q.value("description").toString();
            r.status = q.value("status").toString();
            r.handlerId = q.value("handler_id").toInt();
            r.handlerNote = q.value("handler_note").toString();
            r.createTime = q.value("create_time").toString();
            r.handleTime = q.value("handle_time").toString();
            list.append(r);
        }
    }
    return list;
}

bool ReportsDAO::handle(int reportId, int handlerId, const QString &newStatus,
                        const QString &handlerNote, QString &err)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("UPDATE reports SET status=?, handler_id=?, handler_note=?, handle_time=datetime('now','localtime') "
              "WHERE id=?");
    q.addBindValue(newStatus); q.addBindValue(handlerId);
    q.addBindValue(handlerNote); q.addBindValue(reportId);
    if (!q.exec()) { err = q.lastError().text(); return false; }
    return true;
}

Report ReportsDAO::getById(int id)
{
    Report r;
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT * FROM reports WHERE id = ?");
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        r.id = q.value("id").toInt();
        r.reporterId = q.value("reporter_id").toInt();
        r.targetType = q.value("target_type").toString();
        r.targetId = q.value("target_id").toInt();
        r.reasonType = q.value("reason_type").toString();
        r.description = q.value("description").toString();
        r.status = q.value("status").toString();
        r.createTime = q.value("create_time").toString();
    }
    return r;
}
