#ifndef GOODSDETAILDIALOG_H
#define GOODSDETAILDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include "models/Models.h"

class GoodsDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoodsDetailDialog(int goodsId, int currentUserId,
                               QWidget *parent = nullptr,
                               bool viewMode = false); // viewMode=true 只读查看（管理员审核），隐藏购买/收藏/举报按钮

private slots:
    void onPrevImage();
    void onNextImage();
    void onBuyClicked();
    void onFavClicked();
    void onReportGoods();

private:
    void setupUI();
    void loadData();

    Goods m_goods;
    int m_currentUserId;
    int m_currentImageIndex = 0;
    bool m_viewMode = false;
    bool m_isFav = false;     // 当前是否已收藏（服务端查询结果）
    double m_fee = 0;         // 预计手续费（服务端随详情下发）
    QString m_sellerUsername; // 卖家用户名（loadData 时从服务端取）

    // UI 组件
    QLabel *m_imageLabel;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QLabel *m_titleLabel;
    QLabel *m_priceLabel;
    QLabel *m_originalLabel;
    QLabel *m_conditionLabel;
    QLabel *m_descLabel;
    QLabel *m_sellerLabel;
    QLabel *m_contactLabel;
    QLabel *m_statusLabel;
    QLabel *m_categoryLabel;
    QLabel *m_createTimeLabel;
    QPushButton *m_favBtn;
    QPushButton *m_buyBtn;
};

#endif
