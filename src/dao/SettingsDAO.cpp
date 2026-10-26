#include "SettingsDAO.h"
#include "../db/DatabaseManager.h"

#include <QSqlQuery>

QString SettingsDAO::get(const QString &key, const QString &defaultValue)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return defaultValue;
}

bool SettingsDAO::set(const QString &key, const QString &value)
{
    QSqlQuery q(DatabaseManager::instance().db());
    q.prepare("INSERT INTO settings (key, value) VALUES (?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

double SettingsDAO::getDouble(const QString &key, double def)
{
    QString s = get(key);
    bool ok;
    double v = s.toDouble(&ok);
    return ok ? v : def;
}

int SettingsDAO::getInt(const QString &key, int def)
{
    QString s = get(key);
    bool ok;
    int v = s.toInt(&ok);
    return ok ? v : def;
}

double SettingsDAO::feeThreshold()     { return getDouble("fee_threshold", 100.0); }
double SettingsDAO::feeRate()          { return getDouble("fee_rate", 0.005); }
double SettingsDAO::conditionCoef(const QString &c) { return getDouble("coef_" + c, 0.5); }
int    SettingsDAO::transactionTimeoutDays() { return getInt("transaction_timeout_days", 7); }
