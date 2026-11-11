#include "ReportReview.h"

#include "CategoriesDAO.h"
#include "DatabaseManager.h"
#include "GoodsDAO.h"
#include "Protocol.h"
#include "ReportsDAO.h"
#include "TransactionsDAO.h"
#include "UserDAO.h"
#include "../shared/PushRegistry.h"

#include <QBuffer>
#include <QImageReader>
#include <QSqlError>
#include <QSqlQuery>

namespace {

struct Case {
    int id = 0;
    int reportId = 0;
    int goodsId = 0;
    int sellerId = 0;
    QString state;
    QString reason;
    int txId = 0;
};

bool run(QSqlQuery &query, const QString &sql, const QVariantList &values, QString &error)
{
    if (query.prepare(sql)) {
        for (const QVariant &value : values) query.addBindValue(value);
        if (query.exec()) return true;
    }
    error = query.lastError().text();
    return false;
}

bool write(const QString &sql, const QVariantList &values, QString &error)
{
    QSqlQuery query(DatabaseManager::instance().db());
    return run(query, sql, values, error);
}

Case findCase(const QString &column, int value, QString &error)
{
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT id, report_id, goods_id, seller_id, state, reason, tx_id "
                    "FROM report_dispositions WHERE " + column + " = ?", {value}, error)
        || !query.next()) return {};
    return {query.value(0).toInt(), query.value(1).toInt(), query.value(2).toInt(),
            query.value(3).toInt(), query.value(4).toString(), query.value(5).toString(),
            query.value(6).toInt()};
}

bool begin(QSqlDatabase &db, QString &error)
{
    if (db.transaction()) return true;
    error = db.lastError().text();
    return false;
}

bool commit(QSqlDatabase &db, QString &error)
{
    if (db.commit()) return true;
    error = db.lastError().text();
    db.rollback();
    return false;
}

bool notice(int sellerId, const QString &title, const QString &content, QString &error)
{
    return write("INSERT INTO messages (user_id, title, content, type) VALUES (?, ?, ?, 'system')",
                 {sellerId, title, content}, error);
}

void notify(int sellerId, const QString &title)
{
    PushRegistry::instance().pushToUser(sellerId, cs::push::Message, {{"title", title}});
}

const QString downTitle = QStringLiteral("商品因举报被下架");
const QString reviewTitle = QStringLiteral("商品复核结果");

bool downNotice(const Goods &goods, const QString &reason, QString &error)
{
    return notice(goods.sellerId, downTitle,
        QStringLiteral("商品「%1」(#%2) 已被下架。\n原因：%3\n"
                       "如有异议，请在「我的商品」中选择「申请复核」。")
            .arg(goods.title).arg(goods.id).arg(reason), error);
}

bool hasOtherHold(int goodsId, int caseId, bool &found, QString &error)
{
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT 1 FROM report_dispositions WHERE goods_id=? AND id<>? "
                    "AND state IN ('awaiting_trade','awaiting_action','down') LIMIT 1",
             {goodsId, caseId}, error)) return false;
    found = query.next();
    return true;
}

bool canRestore(const Goods &goods, QString &error)
{
    if (!goods.isReportRestricted() || !UserDAO::getById(goods.sellerId).isActive()
        || CategoriesDAO::getById(goods.categoryId).id == 0 || goods.currentPrice <= 0) {
        error = QStringLiteral("商品或卖家不符合恢复上架条件");
        return false;
    }
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT 1 FROM transactions WHERE goods_id=? AND status='paid' LIMIT 1",
             {goods.id}, error)) return false;
    if (!query.next()) return true;
    error = QStringLiteral("存在交易中的订单，暂不能恢复上架");
    return false;
}

bool lift(const Case &entry, bool &restored, QString &error)
{
    const Goods goods = GoodsDAO::getById(entry.goodsId);
    if (!canRestore(goods, error)) return false;
    bool otherHold = false;
    if (!hasOtherHold(goods.id, entry.id, otherHold, error)) return false;
    restored = !otherHold;
    if (restored && !GoodsDAO::restoreFromReport(goods.id, error)) return false;
    return write("UPDATE report_dispositions SET state='lifted' WHERE id=?", {entry.id}, error);
}

bool reviewNotice(const Case &entry, bool approved, bool restored, const QString &reason, QString &error)
{
    QString outcome = QStringLiteral("维持下架");
    if (approved) {
        outcome = restored ? QStringLiteral("已恢复上架")
                           : QStringLiteral("本次复核通过，商品还有其他有效处置，暂未恢复");
    }
    return notice(entry.sellerId, reviewTitle, QStringLiteral("商品 #%1：%2\n理由：%3")
        .arg(entry.goodsId).arg(outcome, reason), error);
}

bool addValidatedPhotos(int caseId, const QJsonArray &photos, QString &error)
{
    QSqlQuery count(DatabaseManager::instance().db());
    if (!run(count, "SELECT COUNT(*) FROM report_review_photos WHERE case_id=?", {caseId}, error)
        || !count.next()) return false;
    const int existing = count.value(0).toInt();
    count.finish();
    if (existing + photos.size() > cs::ReviewPhotoLimit) {
        error = QStringLiteral("每份申请最多上传 %1 张复核照片").arg(cs::ReviewPhotoLimit);
        return false;
    }
    for (const auto &value : photos) {
        const QByteArray encoded = value.toString().toLatin1();
        if (encoded.isEmpty() || encoded.size() > ((cs::ReviewPhotoBytes + 2) / 3) * 4) {
            error = QStringLiteral("照片为空或超过 %1 MiB").arg(cs::ReviewPhotoMiB);
            return false;
        }
        const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded || decoded.decoded.isEmpty() || decoded.decoded.size() > cs::ReviewPhotoBytes) {
            error = QStringLiteral("照片编码无效或超过 %1 MiB").arg(cs::ReviewPhotoMiB);
            return false;
        }
        QBuffer buffer;
        buffer.setData(decoded.decoded);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QByteArray format = reader.format();
        const QSize size = reader.size();
        if ((format != "png" && format != "jpeg") || !size.isValid()
            || size.width() > 4096 || size.height() > 4096 || reader.read().isNull()) {
            error = QStringLiteral("请上传可读取的 JPG 或 PNG，边长最多 4096 像素");
            return false;
        }
        if (!write("INSERT INTO report_review_photos (case_id, content) VALUES (?, ?)",
                   {caseId, decoded.decoded}, error)) return false;
    }
    return true;
}

QJsonObject reviewSummary(int caseId, QString &error)
{
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT status, description, create_time, handler_id, reason, handle_time "
                    "FROM report_reviews WHERE case_id=?", {caseId}, error) || !query.next()) return {};
    return {{"status", query.value(0).toString()}, {"description", query.value(1).toString()},
            {"createTime", query.value(2).toString()}, {"handlerId", query.value(3).toInt()},
            {"reason", query.value(4).toString()}, {"handleTime", query.value(5).toString()}};
}

} // namespace

namespace reportreview {

bool submitReport(const Report &report, QString &error)
{
    if (report.targetType != "goods") return ReportsDAO::insert(report, error);
    if (GoodsDAO::getById(report.targetId).id == 0 || report.reasonType.isEmpty()) {
        error = QStringLiteral("商品不存在或举报原因为空");
        return false;
    }
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT status, description FROM reports WHERE reporter_id=? AND target_type='goods' "
                    "AND target_id=? AND trim(reason_type)=? COLLATE NOCASE "
                    "AND status IN ('pending','rejected')",
             {report.reporterId, report.targetId, report.reasonType}, error)) return false;
    bool repeatedRejected = false;
    while (query.next()) {
        if (query.value(0).toString() == "pending") return true;
        repeatedRejected |= query.value(1).toString().simplified() == report.description.simplified();
    }
    query.finish();
    if (repeatedRejected) {
        error = QStringLiteral("相同举报曾被驳回，请补充新的事实或证据");
        return false;
    }
    return ReportsDAO::insert(report, error);
}

QString handleReport(int reportId, int adminId, const QString &action,
                     const QString &reason, QString &error)
{
    const Report report = ReportsDAO::getById(reportId);
    if (report.id == 0) { error = QStringLiteral("举报不存在"); return {}; }
    const Case entry = findCase("report_id", reportId, error);
    if (!error.isEmpty()) return {};
    const bool pending = report.status == "pending";
    if (pending && (action == "rejected" || (action == "resolved" && report.targetType != "goods"))) {
        return ReportsDAO::handle(reportId, adminId, action, reason, error) ? action : QString{};
    }
    const bool completing = entry.state == "awaiting_action";
    const bool legacy = entry.id == 0 && (report.status == "resolved" || report.status == "refunded")
                        && action == "enforce";
    if (report.targetType != "goods" || (!pending && !completing && !legacy)
        || (action != "resolved" && !legacy && !(completing && action == "release"))) {
        error = QStringLiteral("举报处理状态已变化，请刷新");
        return {};
    }
    if (reason.isEmpty() || reason.size() > 1000) {
        error = QStringLiteral("请填写具体处理原因，最多 1000 字");
        return {};
    }
    const Goods goods = GoodsDAO::getById(report.targetId);
    if (goods.id == 0) { error = QStringLiteral("被举报商品已不存在"); return {}; }
    QSqlQuery trades(DatabaseManager::instance().db());
    if (!run(trades, "SELECT id FROM transactions WHERE goods_id=? AND status='paid' ORDER BY id DESC LIMIT 1",
             {goods.id}, error)) return {};
    const int txId = trades.next() ? trades.value(0).toInt() : 0;
    trades.finish();
    if (completing && (txId || !goods.isReportRestricted())) {
        error = QStringLiteral("商品或订单状态已变化，请刷新");
        return {};
    }
    if (!txId && goods.isReserved()) {
        error = QStringLiteral("商品处于交易中，暂不能执行下架");
        return {};
    }
    const bool canApplyDown = goods.isOnSale() || goods.isReportRestricted();
    const QString state = txId ? "awaiting_trade" : canApplyDown ? "down" : "closed";
    QSqlDatabase db = DatabaseManager::instance().db();
    if (!begin(db, error)) return {};
    bool ok = false;
    if (action == "release") {
        bool restored = false;
        ok = lift(entry, restored, error)
            && write("INSERT INTO report_reviews (case_id, description, status, handler_id, reason, handle_time) "
                     "VALUES (?, '', 'approved', ?, ?, datetime('now','localtime'))",
                     {entry.id, adminId, reason}, error)
            && reviewNotice(entry, true, restored, reason, error);
    } else {
        if (completing) {
            ok = write("UPDATE report_dispositions SET state='down', reason=?, down_admin_id=?, "
                       "down_time=datetime('now','localtime') WHERE id=?", {reason, adminId, entry.id}, error);
        } else {
            ok = (legacy || ReportsDAO::handle(reportId, adminId, "resolved", reason, error))
                && write("INSERT INTO report_dispositions "
                         "(report_id, goods_id, seller_id, tx_id, state, reason, down_admin_id, down_time) "
                         "VALUES (?, ?, ?, ?, ?, ?, CASE WHEN ?='down' THEN ? END, "
                         "CASE WHEN ?='down' THEN datetime('now','localtime') END)",
                         {reportId, goods.id, goods.sellerId, txId, state, reason, state, adminId, state}, error);
        }
        if (ok && state == "down") {
            ok = (goods.isOnSale() ? GoodsDAO::offShelfByReport(goods.id, error)
                                  : GoodsDAO::forceOffShelfReport(goods.id, error))
                && downNotice(goods, reason, error);
        }
    }
    if (!ok) { db.rollback(); return {}; }
    if (!commit(db, error)) return {};
    if (action == "release") {
        notify(goods.sellerId, reviewTitle);
        return "lifted";
    }
    if (state == "down") notify(goods.sellerId, downTitle);
    return state;
}

QJsonObject reportContext(int reportId, QString &error)
{
    const Case entry = findCase("report_id", reportId, error);
    if (entry.id == 0) return {};
    const QJsonObject review = reviewSummary(entry.id, error);
    return {{"id", entry.id}, {"state", entry.state}, {"reason", entry.reason}, {"review", review},
            {"tradeStatus", entry.txId ? TransactionsDAO::getById(entry.txId).status : QString{}}};
}

QJsonArray sellerCases(int goodsId, int sellerId, QString &error)
{
    const Goods goods = GoodsDAO::getById(goodsId);
    if (goods.id == 0 || (sellerId > 0 && goods.sellerId != sellerId)) {
        error = QStringLiteral("商品不存在或无权查看复核");
        return {};
    }
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT d.id, d.report_id, d.state, d.reason, r.status FROM report_dispositions d "
                    "LEFT JOIN report_reviews r ON r.case_id=d.id WHERE d.goods_id=? "
                    "AND (d.state='down' OR r.case_id IS NOT NULL) ORDER BY d.id DESC",
             {goodsId}, error)) return {};
    QJsonArray result;
    while (query.next()) {
        result.append(QJsonObject{{"id", query.value(0).toInt()}, {"reportId", query.value(1).toInt()},
             {"state", query.value(2).toString()}, {"reason", query.value(3).toString()},
             {"reviewStatus", query.value(4).toString()}});
    }
    return result;
}

QJsonObject caseDetails(int caseId, int userId, bool admin, QString &error)
{
    const Case entry = findCase("id", caseId, error);
    if (!error.isEmpty()) return {};
    if (!entry.id || (!admin && entry.sellerId != userId)) {
        error = QStringLiteral("复核记录不存在或无权查看");
        return {};
    }
    const Goods goods = GoodsDAO::getById(entry.goodsId);
    QJsonObject result{{"id", entry.id}, {"state", entry.state}, {"reportId", entry.reportId},
                       {"goodsTitle", goods.title}, {"reason", entry.reason},
                       {"review", reviewSummary(entry.id, error)}};
    if (!error.isEmpty()) return {};
    if (entry.reportId) {
        const Report report = ReportsDAO::getById(entry.reportId);
        if (cs::goodsReportCategories().contains(report.reasonType)) {
            result.insert("reportCategory", report.reasonType);
        }
        if (admin) {
            result.insert("reportReason", report.reasonType);
            result.insert("reportDescription", report.description);
            result.insert("reportDecision", report.handlerNote);
        }
    }
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT id FROM report_review_photos WHERE case_id=? ORDER BY id", {caseId}, error)) return {};
    QJsonArray photos;
    while (query.next()) photos.append(query.value(0).toInt());
    result.insert("photos", photos);
    return result;
}

bool submitReview(int caseId, int sellerId, const QString &description,
                  const QJsonArray &photos, QString &error)
{
    const Case entry = findCase("id", caseId, error);
    if (!error.isEmpty()) return false;
    if (!entry.id || entry.sellerId != sellerId || entry.state != "down") {
        error = QStringLiteral("该下架处置不可申请复核");
        return false;
    }
    if (description.isEmpty() || description.size() > 2000) {
        error = QStringLiteral("请填写复核说明，最多 2000 字");
        return false;
    }
    const QJsonObject existing = reviewSummary(caseId, error);
    if (!error.isEmpty()) return false;
    if (!existing.isEmpty()) {
        error = QStringLiteral("同一次下架只允许提交一次复核申请");
        return false;
    }
    QSqlDatabase db = DatabaseManager::instance().db();
    if (!begin(db, error)) return false;
    if (!write("INSERT INTO report_reviews (case_id, description) VALUES (?, ?)", {caseId, description}, error)
        || !addValidatedPhotos(caseId, photos, error)) {
        db.rollback();
        return false;
    }
    return commit(db, error);
}

bool addPhotos(int caseId, int sellerId, const QJsonArray &photos, QString &error)
{
    const Case entry = findCase("id", caseId, error);
    if (!error.isEmpty()) return false;
    const QJsonObject review = reviewSummary(caseId, error);
    if (!error.isEmpty()) return false;
    if (!entry.id || entry.sellerId != sellerId || entry.state != "down"
        || review.value("status").toString() != "pending" || photos.isEmpty()) {
        error = QStringLiteral("仅申请卖家可在待复核时补交照片");
        return false;
    }
    QSqlDatabase db = DatabaseManager::instance().db();
    if (!begin(db, error)) return false;
    if (!addValidatedPhotos(caseId, photos, error)) { db.rollback(); return false; }
    return commit(db, error);
}

QJsonObject photo(int photoId, int userId, bool admin, QString &error)
{
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT p.content, d.seller_id FROM report_review_photos p "
                    "JOIN report_dispositions d ON d.id=p.case_id WHERE p.id=?", {photoId}, error)) return {};
    if (!query.next() || (!admin && query.value(1).toInt() != userId)) {
        error = QStringLiteral("照片不存在或无权查看");
        return {};
    }
    return {{"content", QString::fromLatin1(query.value(0).toByteArray().toBase64())}};
}

bool decideReview(int caseId, int adminId, const QString &decision,
                  const QString &reason, bool &restored, QString &error)
{
    const Case entry = findCase("id", caseId, error);
    if (!error.isEmpty()) return false;
    const QJsonObject review = reviewSummary(caseId, error);
    if (!error.isEmpty()) return false;
    if (entry.state != "down" || review.value("status").toString() != "pending") {
        error = QStringLiteral("复核已处理或商品状态已变化");
        return false;
    }
    if ((decision != "approved" && decision != "denied") || reason.isEmpty() || reason.size() > 1000) {
        error = QStringLiteral("请选择复核结果并填写理由，最多 1000 字");
        return false;
    }
    const bool approved = decision == "approved";
    QSqlDatabase db = DatabaseManager::instance().db();
    if (!begin(db, error)) return false;
    restored = false;
    if ((approved && !lift(entry, restored, error))
        || !write("UPDATE report_reviews SET status=?, handler_id=?, reason=?, "
                  "handle_time=datetime('now','localtime') WHERE case_id=?",
                  {decision, adminId, reason, caseId}, error)
        || !reviewNotice(entry, approved, restored, reason, error)) {
        db.rollback();
        return false;
    }
    if (!commit(db, error)) return false;
    notify(entry.sellerId, reviewTitle);
    return true;
}

bool restoreLegacy(int goodsId, int adminId, const QString &reason, QString &error)
{
    const Goods goods = GoodsDAO::getById(goodsId);
    bool active = false;
    if (!canRestore(goods, error) || !hasOtherHold(goodsId, 0, active, error)) return false;
    if (active || !goods.isReportOff()) {
        error = QStringLiteral("请在举报中心完成对应的处置或复核");
        return false;
    }
    if (reason.isEmpty() || reason.size() > 1000) {
        error = QStringLiteral("请填写恢复理由，最多 1000 字");
        return false;
    }
    QSqlDatabase db = DatabaseManager::instance().db();
    if (!begin(db, error)) return false;
    QSqlQuery audit(db);
    if (!run(audit, "INSERT INTO report_dispositions (goods_id, seller_id, state, reason) "
                    "VALUES (?, ?, 'lifted', ?)",
             {goods.id, goods.sellerId, QStringLiteral("历史下架，未关联原举报")}, error)) {
        db.rollback();
        return false;
    }
    const Case entry{audit.lastInsertId().toInt(), 0, goods.id, goods.sellerId, {}, {}, 0};
    audit.finish();
    if (!GoodsDAO::restoreFromReport(goods.id, error)
        || !write("INSERT INTO report_reviews (case_id, description, status, handler_id, reason, handle_time) "
                  "VALUES (?, '', 'approved', ?, ?, datetime('now','localtime'))",
                  {entry.id, adminId, reason}, error)
        || !reviewNotice(entry, true, true, reason, error)) {
        db.rollback();
        return false;
    }
    if (!commit(db, error)) return false;
    notify(goods.sellerId, reviewTitle);
    return true;
}

bool returnCancelledGoods(const Transaction &trade, QString &error)
{
    QSqlQuery update(DatabaseManager::instance().db());
    if (!run(update, "UPDATE report_dispositions SET state='awaiting_action' "
                     "WHERE tx_id=? AND state='awaiting_trade'", {trade.id}, error)) return false;
    if (update.numRowsAffected() == 0) return GoodsDAO::setStatus(trade.goodsId, "on_sale", error);
    return write("UPDATE goods SET status='off_shelf', off_reason='report_pending', "
                 "update_time=datetime('now','localtime') WHERE id=?", {trade.goodsId}, error);
}

bool finishRefund(const Transaction &trade, int adminId, QString &error)
{
    const Goods goods = GoodsDAO::getById(trade.goodsId);
    QSqlQuery query(DatabaseManager::instance().db());
    if (!run(query, "SELECT id, reason FROM report_dispositions WHERE tx_id=? AND state='awaiting_trade'",
             {trade.id}, error)) return false;
    QList<QPair<int, QString>> cases;
    while (query.next()) cases.append({query.value(0).toInt(), query.value(1).toString()});
    query.finish();
    if (cases.isEmpty()) {
        const QString reason = QStringLiteral("管理员强制退款并下架，交易 #%1").arg(trade.id);
        return write("INSERT INTO report_dispositions (goods_id, seller_id, tx_id, state, reason, "
                     "down_admin_id, down_time) VALUES (?, ?, ?, 'down', ?, ?, datetime('now','localtime'))",
                     {goods.id, goods.sellerId, trade.id, reason, adminId}, error)
            && downNotice(goods, reason, error);
    }
    for (const auto &item : cases) {
        if (!write("UPDATE report_dispositions SET state='down', down_admin_id=?, "
                   "down_time=datetime('now','localtime') WHERE id=?", {adminId, item.first}, error)
            || !downNotice(goods, item.second, error)) return false;
    }
    return true;
}

bool finishSale(int txId, QString &error)
{
    return write("UPDATE report_dispositions SET state='closed' WHERE tx_id=? AND state='awaiting_trade'",
                 {txId}, error);
}

} // namespace reportreview
