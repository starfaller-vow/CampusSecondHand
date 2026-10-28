#include "GoodsCardWidget.h"
#include "ImageLoader.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QFile>
#include <QPainter>
#include <QEvent>

GoodsCardWidget::GoodsCardWidget(const Goods &goods, QWidget *parent)
    : QFrame(parent), m_goods(goods)
{
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(
        "GoodsCardWidget { background: white; border: 1px solid #e0e0e0; "
        "border-radius: 8px; }"
        "GoodsCardWidget:hover { border-color: #1976D2; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }"
    );
    setFixedSize(240, 280);
    setCursor(Qt::PointingHandCursor);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 10);
    mainLayout->setSpacing(0);

    // ===== 图片（缩略图）=====
    m_imageLabel = new QLabel(this);
    m_imageLabel->setFixedHeight(150);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background: #f5f5f5; border-radius: 8px 8px 0 0;");

    // 加载首图（C/S：本地无缓存时从服务端下载到 images/）
    QStringList imgs = goods.imageList();
    QPixmap pix;
    if (!imgs.isEmpty()) {
        const QString local = ensureLocalImage(imgs.first());
        if (!local.isEmpty()) pix.load(local);
    }
    if (pix.isNull()) {
        // 占位图
        pix = QPixmap(200, 150);
        pix.fill(QColor("#e0e0e0"));
        QPainter p(&pix);
        p.setPen(QColor("#999"));
        QFont f; f.setPointSize(12);
        p.setFont(f);
        p.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("🖼️ 暂无图片"));
    }
    QPixmap scaled = pix.scaled(220, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
    mainLayout->addWidget(m_imageLabel);

    // ===== 内容区域 =====
    auto *contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(10, 8, 10, 0);
    contentLayout->setSpacing(4);

    // 标题
    m_titleLabel = new QLabel(goods.title, this);
    m_titleLabel->setWordWrap(true);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setMaximumHeight(32);
    contentLayout->addWidget(m_titleLabel);

    // 价格行
    auto *priceRow = new QHBoxLayout();
    priceRow->setSpacing(6);
    m_priceLabel = new QLabel(QString("¥%1").arg(goods.currentPrice, 0, 'f', 0), this);
    QFont priceFont = m_priceLabel->font();
    priceFont.setBold(true);
    priceFont.setPointSize(14);
    m_priceLabel->setFont(priceFont);
    m_priceLabel->setStyleSheet("color: #e53935;");

    m_originalLabel = new QLabel(QString("¥%1").arg(goods.originalPrice, 0, 'f', 0), this);
    QFont origFont = m_originalLabel->font();
    origFont.setStrikeOut(true);
    origFont.setPointSize(9);
    m_originalLabel->setFont(origFont);
    m_originalLabel->setStyleSheet("color: #999;");

    priceRow->addWidget(m_priceLabel);
    priceRow->addWidget(m_originalLabel);
    priceRow->addStretch();
    contentLayout->addLayout(priceRow);

    // 磨损 + 分类
    auto *tagRow = new QHBoxLayout();
    tagRow->setSpacing(6);
    m_conditionLabel = new QLabel(goods.condition, this);
    m_conditionLabel->setStyleSheet(
        "background: #fff3e0; color: #e65100; padding: 1px 6px; border-radius: 3px; font-size: 9px;"
    );
    // C/S：分类名与卖家用户名由服务端随商品 JSON 下发（goods.categoryName / goods.sellerName）
    m_categoryLabel = new QLabel(
        goods.categoryName.isEmpty() ? QStringLiteral("未分类") : goods.categoryName, this);
    m_categoryLabel->setStyleSheet(
        "background: #e3f2fd; color: #1565c0; padding: 1px 6px; border-radius: 3px; font-size: 9px;"
    );
    tagRow->addWidget(m_conditionLabel);
    tagRow->addWidget(m_categoryLabel);
    tagRow->addStretch();
    contentLayout->addLayout(tagRow);

    // 卖家
    m_sellerLabel = new QLabel(
        QStringLiteral("@%1").arg(goods.sellerName.isEmpty() ? QStringLiteral("未知卖家") : goods.sellerName), this);
    m_sellerLabel->setStyleSheet("color: #666; font-size: 10px;");
    contentLayout->addWidget(m_sellerLabel);

    mainLayout->addLayout(contentLayout);

    // 整个卡片可点击
    this->installEventFilter(this);
    m_imageLabel->installEventFilter(this);
    m_titleLabel->installEventFilter(this);
}

bool GoodsCardWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        emit cardClicked(m_goods.id);
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

void GoodsCardWidget::onCardClicked()
{
    emit cardClicked(m_goods.id);
}
