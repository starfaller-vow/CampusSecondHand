#include "PaymentConfirmDialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString money(double value)
{
    return QStringLiteral("¥%1").arg(value, 0, 'f', 2);
}

QLabel *plainLabel(const QString &text, QWidget *parent, const char *name)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(name));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
} // namespace

PaymentConfirmDialog::PaymentConfirmDialog(const QString &goodsTitle,
        const QString &sellerName, double amount, double balance,
        double sellerFee, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("paymentConfirmDialog"));
    setWindowTitle(QStringLiteral("支付确认"));
    setMinimumWidth(460);
    resize(500, 540);
    setStyleSheet(QStringLiteral(
        "QDialog#paymentConfirmDialog { background: #FFFFFF; }"
        "QLabel { color: #263238; font-size: 14px; background: transparent; }"
        "QLabel#heading { font-size: 21px; font-weight: bold; }"
        "QLabel#sellerName, QLabel#balanceCaption, QLabel#remainingCaption { color: #667085; }"
        "QFrame#paymentSummary { background: #F7F8FA; border-radius: 10px; }"
        "QLabel#amountDue { color: #E53935; font-size: 28px; font-weight: bold; }"
        "QLabel#balance, QLabel#remainingBalance { font-size: 16px; font-weight: bold; }"
        "QLabel#sellerFeeNote, QLabel#escrowNote { color: #667085; font-size: 12px; }"
        "QLabel#insufficientBalance { color: #C62828; font-size: 13px; }"
        "QLineEdit { padding: 10px; border: 1px solid #D0D5DD; border-radius: 6px; font-size: 14px; }"
        "QPushButton { min-height: 40px; padding: 0 16px; border-radius: 6px; font-size: 14px; }"
        "QPushButton#cancelPayment { background: #FFFFFF; color: #475467; border: 1px solid #D0D5DD; }"
        "QPushButton#confirmPayment { background: #E53935; color: #FFFFFF; font-weight: bold; border: none; }"
        "QPushButton#confirmPayment:hover { background: #C62828; }"
        "QPushButton#confirmPayment:disabled { background: #E4E7EC; color: #98A2B3; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);
    layout->addWidget(plainLabel(QStringLiteral("确认付款"), this, "heading"));

    auto *productLayout = new QVBoxLayout();
    productLayout->setSpacing(5);
    productLayout->addWidget(plainLabel(goodsTitle, this, "goodsTitle"));
    productLayout->addWidget(plainLabel(QStringLiteral("卖家：@") + sellerName, this, "sellerName"));
    layout->addLayout(productLayout);

    const bool canPay = balance >= amount;
    auto *summary = new QFrame(this);
    summary->setObjectName(QStringLiteral("paymentSummary"));
    auto *grid = new QGridLayout(summary);
    grid->setContentsMargins(18, 16, 18, 16);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(15);
    grid->setColumnStretch(1, 1);

    auto addAmountRow = [&](int row, const QString &caption, const QString &value,
                            const char *captionName, const char *valueName) {
        auto *label = plainLabel(caption, summary, captionName);
        auto *number = plainLabel(value, summary, valueName);
        number->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
        grid->addWidget(number, row, 1);
    };
    addAmountRow(0, QStringLiteral("应付金额"), money(amount), "amountCaption", "amountDue");
    auto *divider = new QFrame(summary);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color: #E4E7EC;"));
    grid->addWidget(divider, 1, 0, 1, 2);
    addAmountRow(2, QStringLiteral("你的余额"), money(balance), "balanceCaption", "balance");
    addAmountRow(3, QStringLiteral("支付后余额"),
                 canPay ? money(balance - amount) : QStringLiteral("—"),
                 "remainingCaption", "remainingBalance");
    layout->addWidget(summary);

    if (!canPay) {
        layout->addWidget(plainLabel(
            QStringLiteral("余额不足，还差 %1，请充值后重新发起购买。")
                .arg(money(amount - balance)), this, "insufficientBalance"));
    }

    auto *feeLayout = new QHBoxLayout();
    auto *feeCaption = plainLabel(QStringLiteral("卖家预计手续费"), this, "sellerFeeCaption");
    auto *feeValue = plainLabel(money(sellerFee), this, "sellerFee");
    feeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    feeLayout->addWidget(feeCaption);
    feeLayout->addStretch();
    feeLayout->addWidget(feeValue);
    layout->addLayout(feeLayout);
    layout->addWidget(plainLabel(
        QStringLiteral("由卖家承担，在结算时扣除；你无需另付手续费。金额按当前平台规则预估。"),
        this, "sellerFeeNote"));
    layout->addWidget(plainLabel(
        QStringLiteral("货款由平台托管，确认收货或超时自动结算后转给卖家。"), this, "escrowNote"));

    layout->addWidget(plainLabel(QStringLiteral("给卖家的留言（可选）"), this, "remarkCaption"));
    m_remarkEdit = new QLineEdit(this);
    m_remarkEdit->setObjectName(QStringLiteral("paymentRemark"));
    m_remarkEdit->setPlaceholderText(QStringLiteral("例如：方便取货的时间、地点"));
    layout->addWidget(m_remarkEdit);
    layout->addStretch();

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(12);
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setObjectName(QStringLiteral("cancelPayment"));
    auto *confirm = new QPushButton(canPay
        ? QStringLiteral("确认支付 %1").arg(money(amount))
        : QStringLiteral("余额不足"), this);
    confirm->setObjectName(QStringLiteral("confirmPayment"));
    confirm->setEnabled(canPay);
    // 回车不直接触发扣款，要求明确点击确认。
    cancel->setAutoDefault(false);
    confirm->setAutoDefault(false);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(cancel, 1);
    buttons->addWidget(confirm, 2);
    layout->addLayout(buttons);
}

QString PaymentConfirmDialog::remark() const
{
    return m_remarkEdit->text().trimmed();
}
