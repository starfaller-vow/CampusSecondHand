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
//用来**检查传入的文本里是否包含敏感词**，如果命中，把匹配到的敏感词放到引用参数 `hitWord` 里，返回 true；没命中返回 false
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
//客户端把本地图片路径传给服务端，服务端把客户端发来的这些图片，**复制到服务端自己的程序运行目录下的 images 文件夹**，
// 然后生成一段 JSON，JSON 里面存 `images/xxx.jpg` 这种相对路径，之后这条 JSON 一般会存到数据库里
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
    if (!g.isOnSale()) { errorMsg = QStringLiteral("仅在售商品可以主动下架"); return false; }
    return GoodsDAO::setStatus(goodsId, "off_shelf", errorMsg);//`off_shelf`：下架
}

bool GoodsManager::onShelf(int goodsId, int operatorId, QString &errorMsg)
{
    Goods g = GoodsDAO::getById(goodsId);
    if (g.id == 0) { errorMsg = QStringLiteral("商品不存在"); return false; }
    if (g.sellerId != operatorId) { errorMsg = QStringLiteral("无权操作"); return false; }
    if (g.isReserved()) { errorMsg = QStringLiteral("商品已被预订，无法上架"); return false; }
    // 被举报下架的商品：卖家不能自行重新上架，只能由管理员在举报中心恢复
    if (g.isReportRestricted()) {
        errorMsg = QStringLiteral("商品受举报处置限制，请查看我的商品中的处理状态");
        return false;
    }
    if (!g.isOffShelf() && !g.isOnSale()) {
        errorMsg = QStringLiteral("当前商品状态不可上架");
        return false;
    }
    return GoodsDAO::setStatus(goodsId, "on_sale", errorMsg);//`on_sale`：上架售卖
}

bool GoodsManager::markSold(int goodsId, QString &errorMsg)
{
    return GoodsDAO::setStatus(goodsId, "sold", errorMsg);//`sold`：已售出
}

// ==================== 删除 ====================

bool GoodsManager::deleteGoods(int goodsId, int operatorId, QString &errorMsg)
{
    Goods g = GoodsDAO::getById(goodsId);
    if (g.id == 0) { errorMsg = QStringLiteral("商品不存在"); return false; }
    if (g.sellerId != operatorId) {
        // 管理员也可以删
        //管理员（可以删除任何人的商品）
        User u = UserDAO::getById(operatorId);
        if (!u.isAdmin()) {
            errorMsg = QStringLiteral("无权删除该商品");
            return false;
        }
    }
    if (g.isReportRestricted() || g.isReserved()) {
        errorMsg = QStringLiteral("举报处置中或交易中的商品不可删除");
        return false;
    }
    return GoodsDAO::deleteById(goodsId, errorMsg);
}

// ==================== 查询 ====================

QList<Goods> GoodsManager::browseOnSale(const GoodsDAO::QueryOptions &opt)
//浏览在售商品
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

void GoodsManager::autoOffShelfBySeller(int sellerId)
{
    QList<Goods> list = GoodsDAO::getBySeller(sellerId);
    for (const Goods &g : list) {
        if (g.isOnSale()) {
            QString err;
            GoodsDAO::setStatus(g.id, "off_shelf", err);
        }
    }
}
