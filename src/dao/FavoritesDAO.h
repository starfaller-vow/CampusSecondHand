#ifndef FAVORITESDAO_H
#define FAVORITESDAO_H
#include "../models/Models.h"
class FavoritesDAO {
public:
    static bool add(int userId, int goodsId);
    static bool remove(int userId, int goodsId);
    static bool exists(int userId, int goodsId);
    static QList<Favorite> getByUser(int userId);
};
#endif
