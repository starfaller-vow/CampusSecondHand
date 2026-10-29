#include "GoodsManager.h"
#include "../dao/GoodsDAO.h"
#include "../dao/SettingsDAO.h"
#include "../dao/UserDAO.h"
#include "../dao/CategoriesDAO.h"
#include "../db/DatabaseManager.h"

#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

GoodsManager &GoodsManager::instance()
{
    static GoodsManager inst;
    return inst;
}

// ==================== 推荐价 ====================

double GoodsManager::suggestPrice(const QString &condition, double originalPrice)
{
    double coef = SettingsDAO::conditionCoef(condition);
    double raw = originalPrice * coef;
    return std::floor(raw);  // 向下取整到元
}

// ==================== 敏感词检查 ====================

bool GoodsManager::containsSensitiveWord(const QString &text, QString &hitWord)
{
    const QStringList &words = DatabaseManager::instance().sensitiveWords();
    for (const QString &w : words) {
        if (!w.isEmpty() && text.contains(w)) {
            hitWord = w;
            return true;
        }
    }
    return false;
}

// ==================== 内部：图片拷贝到 images/ 并生成 JSON ====================
static QString copyImagesAndMakeJson(const QStringList &localPaths, QString &errorMsg)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString imagesDir = appDir + "/images";
    QDir().mkpath(imagesDir);

    QJsonArray arr;
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    for (int i = 0; i < localPaths.size(); ++i) {
        QString src = localPaths[i];
        if (src.isEmpty()) continue;

        QFile srcFile(src);
        if (!srcFile.exists()) continue;

        // 生成目标文件名：时间戳_序号_原扩展名
        QString ext = QFileInfo(src).suffix();
        QString dstName = QString("%1_%2.%3").arg(ts).arg(i).arg(ext);
        QString dstPath = imagesDir + "/" + dstName;

        if (!QFile::copy(src, dstPath)) {
            errorMsg = QStringLiteral("图片拷贝失败：%1").arg(src);
            return {};
        }

        arr.append(QString("images/%1").arg(dstName));
    }

    QJsonDocument doc(arr);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

// ==================== 发布 ====================

bool GoodsManager::publishGoods(int sellerId,
                                const QString &title, const QString &desc,
                                double originalPrice, const QString &condition,
                                double currentPrice, int categoryId,
                                const QString &contact,
                                const QStringList &imageLocalPaths,
                                QString &errorMsg)
{
    // 1. 基础校验
    if (title.trimmed().isEmpty())      { errorMsg = QStringLiteral("标题不能为空"); return false; }
    if (desc.trimmed().isEmpty())       { errorMsg = QStringLiteral("描述不能为空"); return false; }
    if (originalPrice <= 0)             { errorMsg = QStringLiteral("原价必须大于0"); return false; }
    if (currentPrice <= 0)              { errorMsg = QStringLiteral("现价必须大于0"); return false; }
    if (condition.isEmpty())            { errorMsg = QStringLiteral("请选择磨损程度"); return false; }
    if (categoryId <= 0)                { errorMsg = QStringLiteral("请选择分类"); return false; }
    if (contact.trimmed().isEmpty())    { errorMsg = QStringLiteral("请填写联系方式"); return false; }

    // 2. 图片数量限制 1-5
    if (imageLocalPaths.size() > 5) {
        errorMsg = QStringLiteral("最多上传5张图片");
        return false;
    }

    // 3. 敏感词
    QString hit;
    if (containsSensitiveWord(title, hit)) {
        errorMsg = QStringLiteral("标题包含敏感词：%1").arg(hit);
        return false;
    }
    if (containsSensitiveWord(desc, hit)) {
        errorMsg = QStringLiteral("描述包含敏感词：%1").arg(hit);
        return false;
    }
    if (containsSensitiveWord(contact, hit)) {
        errorMsg = QStringLiteral("联系方式包含敏感词：%1").arg(hit);
        return false;
    }

    // 4. 拷贝图片
    QString imagesJson;
    if (!imageLocalPaths.isEmpty()) {
        imagesJson = copyImagesAndMakeJson(imageLocalPaths, errorMsg);
        if (imagesJson.isEmpty()) return false;
    }

    // 5. 组装 Goods 并插入
    Goods g;
    g.sellerId      = sellerId;
    g.title         = title.trimmed();
    g.description   = desc.trimmed();
    g.originalPrice = originalPrice;
    g.condition     = condition;
    g.currentPrice  = currentPrice;
    g.categoryId    = categoryId;
    g.imagesJson    = imagesJson;
    g.status        = "on_sale";

    return GoodsDAO::insert(g, errorMsg);
}

// ==================== 编辑 ====================

bool GoodsManager::editGoods(int goodsId, int operatorId,
                             const QString &title, const QString &desc,
                             double originalPrice, const QString &condition,
                             double currentPrice, int categoryId,
                             const QString &contact,
                             const QStringList &imageLocalPaths,
                             QString &errorMsg)
{
    Goods existing = GoodsDAO::getById(goodsId);
    if (existing.id == 0) {
        errorMsg = QStringLiteral("商品不存在");
        return false;
    }
    if (existing.sellerId != operatorId) {
        errorMsg = QStringLiteral("无权编辑该商品");
        return false;
    }

    // 交易中（reserved/sold/off_shelf）不可编辑
    if (!existing.isOnSale()) {
        errorMsg = QStringLiteral("该商品状态下不可编辑（仅在售状态可编辑）");
        return false;
    }

    // 基础校验
    if (title.trimmed().isEmpty())      { errorMsg = QStringLiteral("标题不能为空"); return false; }
    if (desc.trimmed().isEmpty())       { errorMsg = QStringLiteral("描述不能为空"); return false; }
    if (originalPrice <= 0)             { errorMsg = QStringLiteral("原价必须大于0"); return false; }
    if (currentPrice <= 0)              { errorMsg = QStringLiteral("现价必须大于0"); return false; }
    if (imageLocalPaths.size() > 5) { errorMsg = QStringLiteral("最多上传5张图片"); return false; }

    QString hit;
    if (containsSensitiveWord(title, hit))   { errorMsg = QStringLiteral("标题包含敏感词"); return false; }
    if (containsSensitiveWord(desc, hit))    { errorMsg = QStringLiteral("描述包含敏感词"); return false; }
    if (containsSensitiveWord(contact, hit)) { errorMsg = QStringLiteral("联系方式包含敏感词"); return false; }

    QString imagesJson = existing.imagesJson;
    if (!imageLocalPaths.isEmpty()) {
        imagesJson = copyImagesAndMakeJson(imageLocalPaths, errorMsg);
        if (imagesJson.isEmpty()) return false;
    }

    existing.title         = title.trimmed();
    existing.description   = desc.trimmed();
    existing.originalPrice = originalPrice;
    existing.condition     = condition;
    existing.currentPrice  = currentPrice;
    existing.categoryId    = categoryId;
    existing.imagesJson    = imagesJson;

    return GoodsDAO::update(existing, errorMsg);
}

// ==================== 状态操作 ====================

bool GoodsManager::offShelf(int goodsId, int operatorId, QString &errorMsg)//`operatorId`：操作人的用户 ID（当前登录用户）
{
    Goods g = GoodsDAO::getById(goodsId);
    if (g.id == 0) { errorMsg = QStringLiteral("商品不存在"); return false; }
    if (g.sellerId != operatorId) { errorMsg = QStringLiteral("无权操作"); return false; }
    return GoodsDAO::setStatus(goodsId, "off_shelf", errorMsg);
}

bool GoodsManager::onShelf(int goodsId, int operatorId, QString &errorMsg)
{
    Goods g = GoodsDAO::getById(goodsId);
    if (g.id == 0) { errorMsg = QStringLiteral("商品不存在"); return false; }
    if (g.sellerId != operatorId) { errorMsg = QStringLiteral("无权操作"); return false; }
    if (g.isReserved()) { errorMsg = QStringLiteral("商品已被预订，无法上架"); return false; }
    // 被举报下架的商品：卖家不能自行重新上架，只能由管理员在举报中心恢复
    if (g.isReportOff()) {
        errorMsg = QStringLiteral("商品因举报被下架，请联系管理员申请恢复");
        return false;
    }
    return GoodsDAO::setStatus(goodsId, "on_sale", errorMsg);
}



// ==================== 删除 ====================

bool GoodsManager::deleteGoods(int goodsId, int operatorId, QString &errorMsg)
{
    Goods g = GoodsDAO::getById(goodsId);
    if (g.id == 0) { errorMsg = QStringLiteral("商品不存在"); return false; }
    if (g.sellerId != operatorId) {
        // 管理员也可以删
        User u = UserDAO::getById(operatorId);
        if (!u.isAdmin()) {
            errorMsg = QStringLiteral("无权删除该商品");
            return false;
        }
    }
    return GoodsDAO::deleteById(goodsId, errorMsg);
}

// ==================== 查询 ====================

QList<Goods> GoodsManager::browseOnSale(const GoodsDAO::QueryOptions &opt)
{
    GoodsDAO::QueryOptions o = opt;
    o.status = "on_sale";
    return GoodsDAO::query(o);
}

QList<Goods> GoodsManager::myGoods(int sellerId)
{
    return GoodsDAO::getBySeller(sellerId);
}

// ==================== 卖家封禁时自动下架 ====================


