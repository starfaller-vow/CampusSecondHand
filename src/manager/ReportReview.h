#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct Goods;
struct Report;
struct Transaction;

namespace reportreview {

bool submitReport(const Report &report, QString &error);
QString handleReport(int reportId, int adminId, const QString &action,
                     const QString &reason, QString &error);
QJsonObject reportContext(int reportId, QString &error);
QJsonArray sellerCases(int goodsId, int sellerId, QString &error);
QJsonObject caseDetails(int caseId, int userId, bool admin, QString &error);
bool submitReview(int caseId, int sellerId, const QString &description,
                  const QJsonArray &photos, QString &error);
bool addPhotos(int caseId, int sellerId, const QJsonArray &photos, QString &error);
QJsonObject photo(int photoId, int userId, bool admin, QString &error);
bool decideReview(int caseId, int adminId, const QString &decision,
                  const QString &reason, bool &restored, QString &error);
bool restoreLegacy(int goodsId, int adminId, const QString &reason, QString &error);

// These hooks run inside the existing transaction and database write lock.
bool returnCancelledGoods(const Transaction &trade, QString &error);
bool finishRefund(const Transaction &trade, int adminId, QString &error);
bool finishSale(int txId, QString &error);

} // namespace reportreview
