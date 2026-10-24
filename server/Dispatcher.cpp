#include "Dispatcher.h"
Dispatcher &Dispatcher::instance() { static Dispatcher d; return d; }
QJsonObject Dispatcher::handle(const QString &, const QJsonObject &, const QString &, QString &err) { err = QStringLiteral("当前阶段仅开放连接，业务尚未接入"); return {}; }
