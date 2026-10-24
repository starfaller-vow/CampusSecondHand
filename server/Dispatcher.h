#pragma once
#include <QJsonObject>
#include <QString>
class Dispatcher { public: static Dispatcher &instance(); QJsonObject handle(const QString &, const QJsonObject &, const QString &, QString &); };
