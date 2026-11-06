#ifndef PAYMENTCONFIRMDIALOG_H
#define PAYMENTCONFIRMDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;

// 支付预览：金额分别绑定到独立控件，买家付款与卖家结算费用分开展示。
class PaymentConfirmDialog : public QDialog
{
public:
    PaymentConfirmDialog(const QString &goodsTitle, const QString &sellerName,
                         double amount, double balance, double sellerFee,
                         QWidget *parent = nullptr);

    QString remark() const;

private:
    QLineEdit *m_remarkEdit = nullptr;
};

#endif
