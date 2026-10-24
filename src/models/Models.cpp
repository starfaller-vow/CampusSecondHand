#include "Models.h"

#include <QJsonDocument>
#include <QJsonArray>

QStringList Goods::imageList() const
{
    QStringList result;
    if (imagesJson.isEmpty()) return result;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(imagesJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return result;

    QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        result.append(v.toString());
    }
    return result;
}
