#ifndef BALANCELOGSDAO_H
#define BALANCELOGSDAO_H

#include "../models/Models.h"
#include <QString>

class BalanceLogsDAO
{
public:
    static bool insert(const BalanceLog &l, QString &errorMsg);
    static QList<BalanceLog> getByUser(int userId);
};

#endif
