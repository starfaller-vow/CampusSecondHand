#include "BrowseGoodsPage.h"
#include "GoodsCardWidget.h"
#include "GoodsDetailDialog.h"
#include "ClientConnector.h"
#include "../shared/Protocol.h"
#include "../shared/JsonConv.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>

BrowseGoodsPage::BrowseGoodsPage(int currentUserId, QWidget *parent)
    : QWidget(parent), m_currentUserId(currentUserId)
{
    setupUI();
    // 注意：不再在构造时立刻拉商品列表——由 MainWindow 的懒加载机制在首次显示时调用 refresh()
}

void BrowseGoodsPage::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    // ===== 搜索栏 =====
    auto *searchBar = new QHBoxLayout();
    searchBar->setSpacing(8);

    m_keywordEdit = new QLineEdit(this);
    m_keywordEdit->setPlaceholderText(QStringLiteral("🔍 搜索商品标题"));
    m_keywordEdit->setMinimumHeight(34);

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItem(QStringLiteral("全部分类"), 0);
    // C/S：分类列表从服务端拉取
    {
        QJsonObject data;
        QString e;
        if (ClientConnector::instance().request(cs::cmd::ListCategories, {}, data, e)) {
            const QJsonArray arr = data.value("categories").toArray();
            for (const auto &v : arr) {
                const Category c = jsoncv::categoryFromJson(v.toObject());
                m_categoryCombo->addItem(c.name, c.id);
            }
        }
    }

    m_minPriceEdit = new QLineEdit(this);
    m_minPriceEdit->setPlaceholderText(QStringLiteral("最低¥"));
    m_minPriceEdit->setFixedWidth(70);

    m_maxPriceEdit = new QLineEdit(this);
    m_maxPriceEdit->setPlaceholderText(QStringLiteral("最高¥"));
    m_maxPriceEdit->setFixedWidth(70);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(QStringLiteral("最新发布"), 0);
    m_sortCombo->addItem(QStringLiteral("价格从低到高"), 1);
    m_sortCombo->addItem(QStringLiteral("价格从高到低"), 2);

    auto *searchBtn = new QPushButton(QStringLiteral("🔎 搜索"), this);
    searchBtn->setMinimumHeight(34);
    searchBtn->setStyleSheet(
        "QPushButton { background: #1976D2; color: white; padding: 6px 18px; border-radius: 5px; }"
        "QPushButton:hover { background: #1565C0; }"
    );
    connect(searchBtn, &QPushButton::clicked, this, &BrowseGoodsPage::onSearch);
    connect(m_keywordEdit, &QLineEdit::returnPressed, this, &BrowseGoodsPage::onSearch);
    connect(m_sortCombo, &QComboBox::currentIndexChanged, this, &BrowseGoodsPage::onSearch);

    searchBar->addWidget(m_keywordEdit, 2);
    searchBar->addWidget(m_categoryCombo);
    searchBar->addWidget(m_minPriceEdit);
    searchBar->addWidget(new QLabel(QStringLiteral("—"), this));
    searchBar->addWidget(m_maxPriceEdit);
    searchBar->addWidget(m_sortCombo);
    searchBar->addWidget(searchBtn);

    mainLayout->addLayout(searchBar);

    // ===== 商品卡片网格（在 ScrollArea 里）=====
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; }");

    m_gridContainer = new QWidget();
    m_gridLayout = new QVBoxLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(5, 5, 5, 5);

    m_scrollArea->setWidget(m_gridContainer);
    mainLayout->addWidget(m_scrollArea, 1);
}

void BrowseGoodsPage::refresh()
{
    onSearch();
}

void BrowseGoodsPage::onSearch()
{
    // 组装查询参数（不再依赖本地 GoodsDAO 结构体，直接发服务端）
    const QString keyword    = m_keywordEdit->text().trimmed();
    const int     categoryId = m_categoryCombo->currentData().toInt();

    bool okMin, okMax;
    double minPrice = m_minPriceEdit->text().trimmed().toDouble(&okMin);
    double maxPrice = m_maxPriceEdit->text().trimmed().toDouble(&okMax);
    if (!okMin) minPrice = 0;
    if (!okMax) maxPrice = 0;

    QString sortField, sortOrder;
    const int sortIdx = m_sortCombo->currentData().toInt();
    if (sortIdx == 1) { sortField = "current_price"; sortOrder = "ASC"; }
    else if (sortIdx == 2) { sortField = "current_price"; sortOrder = "DESC"; }

    // 销毁旧容器（Qt 父子机制自动清理里面所有卡片和布局），然后重建
    delete m_gridContainer;
    m_gridContainer = new QWidget();
    m_gridLayout = new QVBoxLayout(m_gridContainer);
    m_gridLayout->setContentsMargins(5, 5, 5, 5);
    m_scrollArea->setWidget(m_gridContainer);

    QList<Goods> list;
    {
        // C/S：商品列表从服务端拉取
        QJsonObject params{
            {"keyword",    keyword},
            {"categoryId", categoryId},
            {"minPrice",   minPrice},
            {"maxPrice",   maxPrice},
            {"sortField",  sortField},
            {"sortOrder",  sortOrder}
        };
        QJsonObject data;
        QString err;
        if (!ClientConnector::instance().request(cs::cmd::ListGoods, params, data, err)) {
            auto *empty = new QLabel(
                QStringLiteral("⚠️ 加载失败：%1\n请确认服务端已启动").arg(err),
                m_gridContainer);
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet("color: #C62828; font-size: 14px;");
            m_gridLayout->addWidget(empty);
            m_gridLayout->addStretch();
            return;
        }
        const QJsonArray arr = data.value("goods").toArray();
        for (const auto &v : arr) {
            list.append(jsoncv::goodsFromJson(v.toObject()));
        }
    }

    if (list.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("😕 没有找到符合条件的商品\n试试换个关键词或清除筛选条件吧"), m_gridContainer);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color: #999; font-size: 14px;");
        m_gridLayout->addWidget(empty);
        m_gridLayout->addStretch();
        return;
    }

    // 网格布局卡片
    auto *grid = new QGridLayout();
    grid->setSpacing(16);
    const int COLS = 4;
    for (int i = 0; i < list.size(); ++i) {
        auto *card = new GoodsCardWidget(list[i], m_gridContainer);
        connect(card, &GoodsCardWidget::cardClicked, this, &BrowseGoodsPage::onGoodsCardClicked);
        int row = i / COLS;
        int col = i % COLS;
        grid->addWidget(card, row, col, Qt::AlignCenter);
    }
    grid->setColumnStretch(COLS, 1);

    m_gridLayout->addLayout(grid);
    m_gridLayout->addStretch();
}

void BrowseGoodsPage::onGoodsCardClicked(int goodsId)
{
    GoodsDetailDialog dlg(goodsId, m_currentUserId, this);
    dlg.exec();
}
