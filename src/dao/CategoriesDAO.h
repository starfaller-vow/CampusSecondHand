#ifndef CATEGORIESDAO_H
#define CATEGORIESDAO_H

#include <QList>
#include "../models/Models.h"

class CategoriesDAO
{
public:
    static QList<Category> getAll();
    static Category getById(int id);
    static bool insert(const Category &c, QString &errorMsg);
    static bool update(const Category &c, QString &errorMsg);
    static bool remove(int id, QString &errorMsg);
};

#endif // CATEGORIESDAO_H
