#ifndef JSONCONV_H
#define JSONCONV_H

#include <QJsonArray>
#include <QJsonObject>

#include "../models/Models.h"

// 模型 <-> JSON 序列化工具（客户端与服务端共享）
// 后续每接入一批命令，在此补充对应模型的转换函数
namespace jsoncv {

// ---- User ----
QJsonObject userToJson(const User &u);
User userFromJson(const QJsonObject &o);

// ---- Goods ----
QJsonObject goodsToJson(const Goods &g);
Goods goodsFromJson(const QJsonObject &o);

// ---- Category ----
QJsonObject categoryToJson(const Category &c);
Category categoryFromJson(const QJsonObject &o);

// ---- Transaction ----
QJsonObject transactionToJson(const Transaction &t);
Transaction transactionFromJson(const QJsonObject &o);

// ---- Message ----
QJsonObject messageToJson(const Message &m);
Message messageFromJson(const QJsonObject &o);

// ---- Report ----
QJsonObject reportToJson(const Report &r);
Report reportFromJson(const QJsonObject &o);

// ---- BalanceLog ----
QJsonObject balanceLogToJson(const BalanceLog &l);
BalanceLog balanceLogFromJson(const QJsonObject &o);

} // namespace jsoncv

#endif // JSONCONV_H