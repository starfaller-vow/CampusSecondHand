#ifndef SETTINGSDAO_H
#define SETTINGSDAO_H

#include <QString>

/**
 * @brief 系统设置 DAO（手续费规则、磨损系数等）
 */
class SettingsDAO
{
public:
    static QString get(const QString &key, const QString &defaultValue = "");
    static bool   set(const QString &key, const QString &value);

    // ===== 类型安全的便捷方法 =====
    static double getDouble(const QString &key, double def = 0.0);
    static int    getInt(const QString &key, int def = 0);

    // ===== 手续费 =====
    static double feeThreshold();    // 起征额
    static double feeRate();         // 费率

    // ===== 推荐价系数 =====
    static double conditionCoef(const QString &condition);

    // ===== 超时 =====
    static int transactionTimeoutDays();
};

#endif // SETTINGSDAO_H
