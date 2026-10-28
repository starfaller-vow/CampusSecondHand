#ifndef GOODSCARDWIDGET_H
#define GOODSCARDWIDGET_H

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include "models/Models.h"

/**
 * @brief 商品卡片组件（浏览列表里每一个商品）
 */
class GoodsCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit GoodsCardWidget(const Goods &goods, QWidget *parent = nullptr);
    int goodsId() const { return m_goods.id; }

signals:
    void cardClicked(int goodsId);

private slots:
    void onCardClicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Goods m_goods;
    QLabel *m_imageLabel;
    QLabel *m_titleLabel;
    QLabel *m_priceLabel;
    QLabel *m_originalLabel;
    QLabel *m_conditionLabel;
    QLabel *m_sellerLabel;
    QLabel *m_categoryLabel;
};

#endif
