#ifndef REPORTSDAO_H
#define REPORTSDAO_H
#include "../models/Models.h"
class ReportsDAO {
public:
    static bool insert(const Report &r, QString &err);
};
#endif
