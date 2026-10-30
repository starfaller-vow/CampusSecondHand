#include "GoodsDetailDialog.h"
#include "ClientConnector.h"
#include "ImageLoader.h"
#include "../shared/Protocol.h"
#include "../shared/JsonConv.h"
#include "../shared/PasswordUtil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPixmap>
#include <QPainter>
#include <QFile>
#include <QMessageBox>
#include <QSplitter>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QDialog>
#include <QPushButton>
#include <QFrame>

GoodsDetailDialog::GoodsDetailDialog(int goodsId, int currentUserId, QWidget *parent, bool viewMode)
    : QDialog(parent), m_currentUserId(currentUserId), m_viewMode(viewMode)
{
    // C/S：商品详情与预计手续费从服务端拉取
    QString err;
    QJsonObject data;
    if (ClientConnector::instance().request(cs::cmd::GoodsDetail,
            QJsonObject{ {"goodsId", goodsId} }, data, err)) {
        m_goods = jsoncv::goodsFromJson(data.value("goods").toObject());
        m_fee   = data.value("fee").toDouble();
    }
    setupUI();
    loadData();
}

void GoodsDetailDialog::setupUI()
{
    setWindowTitle(m_viewMode ? QStringLiteral("商品详情（审核查看）")
                              : QStringLiteral("商品详情"));
    resize(780, 620);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ===== 左侧：图片区 =====
    auto *leftPanel = new QVBoxLayout();
    m_imageLabel = new QLabel(this);
    m_imageLabel->setFixedSize(400, 300);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background: #f5f5f5; border-radius: 8px;");
    leftPanel->addWidget(m_imageLabel);

    // 上一张/下一张
    auto *imgNav = new QHBoxLayout();
    m_prevBtn = new QPushButton(QStringLiteral("◀ 上一张"), this);
    m_nextBtn = new QPushButton(QStringLiteral("下一张 ▶"), this);
    m_prevBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    connect(m_prevBtn, &QPushButton::clicked, this, &GoodsDetailDialog::onPrevImage);
    connect(m_nextBtn, &QPushButton::clicked, this, &GoodsDetailDialog::onNextImage);
    imgNav->addStretch();
    imgNav->addWidget(m_prevBtn);
    imgNav->addSpacing(20);
    imgNav->addWidget(m_nextBtn);
    imgNav->addStretch();
    leftPanel->addLayout(imgNav);
    leftPanel->addStretch();

    mainLayout->addLayout(leftPanel);

    // ===== 右侧：信息区 =====
    auto *rightPanel = new QVBoxLayout();
    rightPanel->setSpacing(8);

    // 标题
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);
    rightPanel->addWidget(m_titleLabel);

    // 价格
    auto *priceRow = new QHBoxLayout();
    m_priceLabel = new QLabel(this);
    QFont pf; pf.setBold(true); pf.setPointSize(22);
    m_priceLabel->setFont(pf);
    m_priceLabel->setStyleSheet("color: #e53935;");

    m_originalLabel = new QLabel(this);
    QFont of; of.setStrikeOut(true); of.setPointSize(12);
    m_originalLabel->setFont(of);
    m_originalLabel->setStyleSheet("color: #999;");

    m_conditionLabel = new QLabel(this);
    m_conditionLabel->setStyleSheet(
        "background: #fff3e0; color: #e65100; padding: 2px 10px; border-radius: 4px;");

    priceRow->addWidget(m_priceLabel);
    priceRow->addSpacing(12);
    priceRow->addWidget(m_originalLabel);
    priceRow->addSpacing(12);
    priceRow->addWidget(m_conditionLabel);
    priceRow->addStretch();
    rightPanel->addLayout(priceRow);

    // 基础信息网格
    auto *infoGrid = new QGridLayout();
    infoGrid->setHorizontalSpacing(10);
    infoGrid->setVerticalSpacing(6);

    auto makeInfoRow = [&](const QString &label, QLabel *&valueLabel, int row) {
        auto *lbl = new QLabel(label, this);
        lbl->setStyleSheet("color: #666;");
        valueLabel = new QLabel(this);
        valueLabel->setWordWrap(true);
        infoGrid->addWidget(lbl, row, 0);
        infoGrid->addWidget(valueLabel, row, 1);
    };

    makeInfoRow(QStringLiteral("分类："), m_categoryLabel, 0);
    makeInfoRow(QStringLiteral("状态："), m_statusLabel, 1);
    makeInfoRow(QStringLiteral("发布时间："), m_createTimeLabel, 2);

    rightPanel->addLayout(infoGrid);

    // 分隔线
    auto *line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setStyleSheet("color: #e0e0e0;");
    rightPanel->addWidget(line1);

    // 描述
    auto *descTitle = new QLabel(QStringLiteral("📝 详细描述："), this);
    descTitle->setStyleSheet("font-weight: bold;");
    rightPanel->addWidget(descTitle);

    m_descLabel = new QLabel(this);
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet("color: #333; padding: 8px; background: #fafafa; border-radius: 4px;");
    m_descLabel->setMinimumHeight(80);
    rightPanel->addWidget(m_descLabel);

    auto *line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet("color: #e0e0e0;");
    rightPanel->addWidget(line2);

    // 卖家信息
    auto *sellerTitle = new QLabel(QStringLiteral("👤 卖家信息："), this);
    sellerTitle->setStyleSheet("font-weight: bold;");
    rightPanel->addWidget(sellerTitle);

    m_sellerLabel = new QLabel(this);
    m_contactLabel = new QLabel(this);
    m_contactLabel->setStyleSheet("color: #1976D2;");
    rightPanel->addWidget(m_sellerLabel);
    rightPanel->addWidget(m_contactLabel);

    rightPanel->addStretch();

    auto *btnRow = new QHBoxLayout();
    if (!m_viewMode) {
        m_favBtn = new QPushButton(QStringLiteral("收藏"), this);
        connect(m_favBtn, &QPushButton::clicked, this, &GoodsDetailDialog::onFavClicked);
        btnRow->addWidget(m_favBtn);
        auto *reportBtn = new QPushButton(QStringLiteral("举报"), this);
        connect(reportBtn, &QPushButton::clicked, this, &GoodsDetailDialog::onReportGoods);
        btnRow->addWidget(reportBtn);
    }
    btnRow->addStretch();

    rightPanel->addLayout(btnRow);

    mainLayout->addLayout(rightPanel, 1);
}

void GoodsDetailDialog::loadData()
{
    // C/S：卖家公开信息与分类名从服务端取
    User seller;
    {
        QJsonObject d; QString e;
        if (ClientConnector::instance().request(cs::cmd::UserGet,
                QJsonObject{ {"userId", m_goods.sellerId} }, d, e)) {
            seller = jsoncv::userFromJson(d.value("user").toObject());
        }
    }
    m_sellerUsername = seller.username;
    QString catName = QStringLiteral("—");
    {
        QJsonObject d; QString e;
        if (ClientConnector::instance().request(cs::cmd::ListCategories, {}, d, e)) {
            const QJsonArray arr = d.value("categories").toArray();
            for (const auto &v : arr) {
                const Category c = jsoncv::categoryFromJson(v.toObject());
                if (c.id == m_goods.categoryId) { catName = c.name; break; }
            }
        }
    }

    m_titleLabel->setText(m_goods.title);
    m_priceLabel->setText(QString("¥%1").arg(m_goods.currentPrice, 0, 'f', 2));
    m_originalLabel->setText(QString("¥%1").arg(m_goods.originalPrice, 0, 'f', 2));
    m_conditionLabel->setText(m_goods.condition);
    m_categoryLabel->setText(catName);
    m_statusLabel->setText(
        m_goods.isOnSale()    ? QStringLiteral("🟢 在售") :
        m_goods.isReserved()  ? QStringLiteral("🟡 已预订") :
        m_goods.isSold()      ? QStringLiteral("🔴 已售出") :
                                 QStringLiteral("⚪ 已下架")
    );
    m_createTimeLabel->setText(m_goods.createTime);
    m_descLabel->setText(m_goods.description);
    m_sellerLabel->setText(QStringLiteral("@%1（学号：%2）").arg(seller.username, seller.studentNo));
    m_contactLabel->setText(QStringLiteral("📱 联系方式：%1").arg(seller.contact));

    // 图片
    QStringList imgs = m_goods.imageList();
    if (imgs.isEmpty()) {
        QPixmap pix(400, 300);
        pix.fill(QColor("#e0e0e0"));
        QPainter p(&pix);
        p.setPen(QColor("#999"));
        QFont f; f.setPointSize(16);
        p.setFont(f);
        p.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("🖼️ 暂无图片"));
        m_imageLabel->setPixmap(pix);
        m_prevBtn->setEnabled(false);
        m_nextBtn->setEnabled(false);
    } else {
        m_prevBtn->setEnabled(imgs.size() > 1);
        m_nextBtn->setEnabled(imgs.size() > 1);
        // C/S：逐张确保本地有缓存（数量少，一次性预取，切换图片无等待）
        for (const QString &img : imgs) {
            ensureLocalImage(img);
        }
        // 显示第一张
        QPixmap pix;
        const QString local = ensureLocalImage(imgs.first());
        if (!local.isEmpty()) pix.load(local);
        if (pix.isNull()) {
            pix.load(":/placeholder.png"); // 占位
        }
        if (pix.isNull()) {
            pix = QPixmap(400, 300);
            pix.fill(QColor("#e0e0e0"));
        }
        m_imageLabel->setPixmap(pix.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // 只读模式下不处理收藏/购买状态（按钮不存在）
    if (m_viewMode) return;

    // 收藏状态（C/S：拉收藏列表判断）
    m_isFav = false;
    {
        QJsonObject d; QString e;
        if (ClientConnector::instance().request(cs::cmd::FavoriteList, {}, d, e)) {
            const QJsonArray arr = d.value("goods").toArray();
            for (const auto &v : arr) {
                if (jsoncv::goodsFromJson(v.toObject()).id == m_goods.id) {
                    m_isFav = true;
                    break;
                }
            }
        }
    }
    if (m_isFav) {
        m_favBtn->setText(QStringLiteral("❤️ 已收藏"));
        m_favBtn->setStyleSheet("background: #ffeb3b; border-radius: 6px; padding: 6px 16px;");
    }


}

void GoodsDetailDialog::onPrevImage()
{
    QStringList imgs = m_goods.imageList();
    if (imgs.isEmpty()) return;
    m_currentImageIndex = (m_currentImageIndex - 1 + imgs.size()) % imgs.size();
    QPixmap pix;
    const QString local = ensureLocalImage(imgs[m_currentImageIndex]);
    if (!local.isEmpty()) pix.load(local);
    if (!pix.isNull()) {
        m_imageLabel->setPixmap(pix.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void GoodsDetailDialog::onNextImage()
{
    QStringList imgs = m_goods.imageList();
    if (imgs.isEmpty()) return;
    m_currentImageIndex = (m_currentImageIndex + 1) % imgs.size();
    QPixmap pix;
    const QString local = ensureLocalImage(imgs[m_currentImageIndex]);
    if (!local.isEmpty()) pix.load(local);
    if (!pix.isNull()) {
        m_imageLabel->setPixmap(pix.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void GoodsDetailDialog::onFavClicked()
{
    QJsonObject d; QString e;
    if (m_isFav) {
        if (!ClientConnector::instance().request(cs::cmd::FavoriteRemove,
                QJsonObject{ {"goodsId", m_goods.id} }, d, e)) {
            QMessageBox::warning(this, QStringLiteral("失败"), e);
            return;
        }
        m_isFav = false;
        m_favBtn->setText(QStringLiteral("❤️ 收藏"));
        m_favBtn->setStyleSheet("");
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("已取消收藏"));
    } else {
        if (!ClientConnector::instance().request(cs::cmd::FavoriteAdd,
                QJsonObject{ {"goodsId", m_goods.id} }, d, e)) {
            QMessageBox::warning(this, QStringLiteral("失败"), e);
            return;
        }
        m_isFav = true;
        m_favBtn->setText(QStringLiteral("❤️ 已收藏"));
        m_favBtn->setStyleSheet("background: #ffeb3b; border-radius: 6px; padding: 6px 16px;");
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("收藏成功！"));
    }
}

void GoodsDetailDialog::onReportGoods()
{
    // 简化版举报弹窗
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("🚩 举报商品"));
    dlg->resize(400, 320);

    auto *layout = new QVBoxLayout(dlg);

    auto *typeLabel = new QLabel(QStringLiteral("举报类型："), dlg);
    auto *typeCombo = new QComboBox(dlg);
    typeCombo->addItem(QStringLiteral("虚假商品"));
    typeCombo->addItem(QStringLiteral("描述不符"));
    typeCombo->addItem(QStringLiteral("欺诈行为"));
    typeCombo->addItem(QStringLiteral("其他"));

    auto *descLabel = new QLabel(QStringLiteral("详细描述（至少10字）："), dlg);
    auto *descEdit = new QTextEdit(dlg);
    descEdit->setPlaceholderText(QStringLiteral("请具体描述问题..."));

    auto *btnRow = new QHBoxLayout();
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), dlg);
    auto *submitBtn = new QPushButton(QStringLiteral("提交举报"), dlg);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    connect(submitBtn, &QPushButton::clicked, this, [=]() {
        QString desc = descEdit->toPlainText().trimmed();
        if (desc.length() < 10) {
            QMessageBox::warning(dlg, QStringLiteral("提示"), QStringLiteral("详细描述至少10字"));
            return;
        }
        // C/S：举报走服务端
        QJsonObject d; QString err;
        if (ClientConnector::instance().request(cs::cmd::ReportSubmit,
                QJsonObject{
                    {"targetType", "goods"},
                    {"targetId", m_goods.id},
                    {"reasonType", typeCombo->currentText()},
                    {"description", desc}
                }, d, err)) {
            QMessageBox::information(dlg, QStringLiteral("提交成功"),
                                     QStringLiteral("举报已提交，管理员会尽快处理"));
            dlg->accept();
        } else {
            QMessageBox::critical(dlg, QStringLiteral("失败"), err);
        }
    });

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(submitBtn);

    layout->addWidget(typeLabel);
    layout->addWidget(typeCombo);
    layout->addWidget(descLabel);
    layout->addWidget(descEdit);
    layout->addLayout(btnRow);

    dlg->exec();
}


