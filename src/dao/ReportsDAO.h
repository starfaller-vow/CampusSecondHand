#ifndef REPORTSDAO_H
#define REPORTSDAO_H
#include "../models/Models.h"
class ReportsDAO {
public:
    static bool insert(const Report &r, QString &err);
    static QList<Report> getAll(const QString &status = "");
    static bool handle(int reportId, int handlerId, const QString &newStatus,
                       const QString &handlerNote, QString &err);
    static Report getById(int id);
};
#endif
