#ifndef VALIDATORS_H
#define VALIDATORS_H

#include <QString>

// ============================================================
// 用户输入格式校验（客户端与服务端共享同一套规则）：
// · 客户端：注册/改密时在明文阶段本地校验
// · 服务端：注册流程复用，保证两边规则一致
// ============================================================
namespace uservalid {

bool isValidUsername(const QString &username, QString &err);
bool isValidPassword(const QString &password, QString &err);
bool isValidStudentNo(const QString &studentNo, QString &err);

} // namespace uservalid

#endif // VALIDATORS_H