#include "Models.h"

#include <QJsonDocument>
#include <QJsonArray>

QStringList Goods::imageList() const
//**把数据库里存的图片 JSON 字符串，解析成 Qt 字符串列表 QStringList 给程序使用**。
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
