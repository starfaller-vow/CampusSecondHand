#include "PublishGoodsDialog.h"
#include "ClientConnector.h"
#include "../shared/Protocol.h"
#include "../shared/JsonConv.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QIntValidator>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <cmath>

PublishGoodsDialog::PublishGoodsDialog(int sellerId, QWidget *parent)
    : QDialog(parent), m_sellerId(sellerId)
{
    setupUI();
}

void PublishGoodsDialog::setupUI()
{
    setWindowTitle(QStringLiteral("📤 发布商品"));
    resize(560, 620);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 25, 30, 20);
    mainLayout->setSpacing(12);

    // 标题
    auto *title = new QLabel(QStringLiteral("📤 发布新商品"), this);
    QFont tf = title->font(); tf.setPointSize(16); tf.setBold(true);
    title->setFont(tf);
    mainLayout->addWidget(title);

    // 表单
    auto *form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(QStringLiteral("简短标题，如「高等数学 同济第七版」"));
    form->addRow(QStringLiteral("标题*："), m_titleEdit);

    m_descEdit = new QTextEdit(this);
    m_descEdit->setPlaceholderText(QStringLiteral("详细描述成色、使用情况、有无笔记等..."));
    m_descEdit->setFixedHeight(120);
    form->addRow(QStringLiteral("描述*："), m_descEdit);

    // 原价
    m_originalEdit = new QLineEdit(this);
    m_originalEdit->setPlaceholderText(QStringLiteral("当初买来多少钱"));
    m_originalEdit->setValidator(new QDoubleValidator(0, 99999, 2, this));
    connect(m_originalEdit, &QLineEdit::textChanged, this, &PublishGoodsDialog::onOriginalPriceChanged);
    form->addRow(QStringLiteral("原价(元)*："), m_originalEdit);

    // 磨损程度
    m_conditionCombo = new QComboBox(this);
    m_conditionCombo->addItem(QStringLiteral("全新"));
    m_conditionCombo->addItem(QStringLiteral("九成新"));
    m_conditionCombo->addItem(QStringLiteral("八成新"));
    m_conditionCombo->addItem(QStringLiteral("七成新"));
    m_conditionCombo->addItem(QStringLiteral("六成新及以下"));
    connect(m_conditionCombo, &QComboBox::currentIndexChanged, this, &PublishGoodsDialog::onConditionChanged);
    form->addRow(QStringLiteral("磨损程度*："), m_conditionCombo);

    // 推荐价 + 现价
    auto *priceRow = new QHBoxLayout();
    m_currentEdit = new QLineEdit(this);
    m_currentEdit->setPlaceholderText(QStringLiteral("自动填入，可修改"));
    m_currentEdit->setValidator(new QDoubleValidator(0, 99999, 2, this));
    m_suggestLabel = new QLabel(QStringLiteral("🤖 系统推荐：¥—"), this);
    m_suggestLabel->setStyleSheet("color: #1976D2; font-weight: bold;");
    priceRow->addWidget(m_currentEdit, 1);
    priceRow->addSpacing(10);
    priceRow->addWidget(m_suggestLabel);
    form->addRow(QStringLiteral("现价(元)*："), priceRow);

    // 分类（C/S：从服务端拉取）
    m_categoryCombo = new QComboBox(this);
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
    form->addRow(QStringLiteral("分类*："), m_categoryCombo);

    // 联系方式
    m_contactEdit = new QLineEdit(this);
    m_contactEdit->setPlaceholderText(QStringLiteral("微信/QQ/手机号"));
    form->addRow(QStringLiteral("联系方式*："), m_contactEdit);

    // 图片选择
    auto *imgRow = new QHBoxLayout();
    auto *selectImgBtn = new QPushButton(QStringLiteral("📷 选择图片（最多5张）"), this);
    m_imagesLabel = new QLabel(QStringLiteral("已选 0/5"), this);
    m_imagesLabel->setStyleSheet("color: #666;");
    connect(selectImgBtn, &QPushButton::clicked, this, &PublishGoodsDialog::onSelectImages);
    imgRow->addWidget(selectImgBtn);
    imgRow->addWidget(m_imagesLabel);
    imgRow->addStretch();
    form->addRow(QStringLiteral("图片："), imgRow);

    mainLayout->addLayout(form);
    mainLayout->addStretch();

    // 按钮
    auto *btnRow = new QHBoxLayout();
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    auto *submitBtn = new QPushButton(QStringLiteral("✅ 发布"), this);
    submitBtn->setMinimumHeight(40);
    submitBtn->setStyleSheet(
        "QPushButton { background: #2E7D32; color: white; padding: 8px 30px; border-radius: 6px; font-size: 14px; }"
        "QPushButton:hover { background: #1B5E20; }"
    );
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(submitBtn, &QPushButton::clicked, this, &PublishGoodsDialog::onSubmit);
    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(submitBtn);
    mainLayout->addLayout(btnRow);
}

void PublishGoodsDialog::updateSuggestPrice()
{
    QString origText = m_originalEdit->text().trimmed();
    bool ok;
    double orig = origText.toDouble(&ok);
    if (!ok || orig <= 0) {
        m_suggestLabel->setText(QStringLiteral("🤖 系统推荐：¥—"));
        return;
    }

    const QString condition = m_conditionCombo->currentText();

    // C/S：推荐价与系数都从服务端取
    QJsonObject data;
    QString err;
    double price = 0, coef = 0;
    if (ClientConnector::instance().request(cs::cmd::SuggestPrice,
            QJsonObject{ {"condition", condition}, {"originalPrice", orig} },
            data, err)) {
        price = data.value("price").toDouble();
        coef  = data.value("coef").toDouble();
    } else {
        m_suggestLabel->setText(QStringLiteral("🤖 推荐价获取失败"));
        return;
    }

    const double oldSuggest = m_suggestPrice;
    m_suggestPrice = price;

    m_suggestLabel->setText(
        QStringLiteral("🤖 系统推荐：¥%1 (%2 × %3)")
            .arg(m_suggestPrice, 0, 'f', 0)
            .arg(orig, 0, 'f', 0)
            .arg(coef, 0, 'f', 2)
    );

    // 自动填入现价：为空，或现价等于旧推荐值（说明是系统之前填的，用户没改过）
    QString currentText = m_currentEdit->text().trimmed();
    if (currentText.isEmpty()) {
        m_currentEdit->setText(QString::number(m_suggestPrice, 'f', 0));
    } else {
        bool ok2;
        double currentVal = currentText.toDouble(&ok2);
        if (ok2 && oldSuggest > 0 && std::abs(currentVal - oldSuggest) < 0.01) {
            m_currentEdit->setText(QString::number(m_suggestPrice, 'f', 0));
        }
    }
}

void PublishGoodsDialog::onOriginalPriceChanged(const QString &)
{
    updateSuggestPrice();
}

void PublishGoodsDialog::onConditionChanged(int)
{
    updateSuggestPrice();
}

void PublishGoodsDialog::onSelectImages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择商品图片"),
        "",
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp)")
    );

    if (files.size() > 5) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("最多选5张图片，已取前5张"));
        files = files.mid(0, 5);
    }
    m_selectedImages = files;
    m_imagesLabel->setText(QStringLiteral("已选 %1/5").arg(m_selectedImages.size()));
}

void PublishGoodsDialog::onSubmit()
{
    QString title = m_titleEdit->text().trimmed();
    QString desc = m_descEdit->toPlainText().trimmed();
    QString origText = m_originalEdit->text().trimmed();
    QString currentText = m_currentEdit->text().trimmed();
    QString contact = m_contactEdit->text().trimmed();
    int categoryId = m_categoryCombo->currentData().toInt();
    QString condition = m_conditionCombo->currentText();

    bool ok1, ok2;
    double originalPrice = origText.toDouble(&ok1);
    double currentPrice = currentText.toDouble(&ok2);

    if (title.isEmpty() || desc.isEmpty() || !ok1 || !ok2) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请完整填写标题/描述/价格"));
        return;
    }

    auto &conn = ClientConnector::instance();
    QString err;
    QJsonObject dataOut;

    // 1. 逐张上传图片到服务端（upload 声明 → 图片帧 → uploadDone 校验）
    QStringList uploadedNames;
    const QString ts = QString::number(QDateTime::currentMSecsSinceEpoch());
    for (int i = 0; i < m_selectedImages.size(); ++i) {
        QFile f(m_selectedImages[i]);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray bytes = f.readAll();
        f.close();
        const QString ext = QFileInfo(m_selectedImages[i]).suffix();
        const QString name = QStringLiteral("%1_%2.%3").arg(ts).arg(i).arg(ext);

        if (!conn.request(cs::cmd::ImageUpload,
                QJsonObject{ {"name", name}, {"size", bytes.size()} }, dataOut, err)) {
            QMessageBox::critical(this, QStringLiteral("发布失败"), err);
            return;
        }
        if (!conn.sendImageFrame(bytes)) {
            QMessageBox::critical(this, QStringLiteral("发布失败"), QStringLiteral("图片传输失败"));
            return;
        }
        if (!conn.request(cs::cmd::ImageUploadDone, {}, dataOut, err)) {
            QMessageBox::critical(this, QStringLiteral("发布失败"), err);
            return;
        }
        uploadedNames.append(name);
    }

    // 2. 发布商品（服务端存图片名并上架）
    QJsonArray imgArr;
    for (const QString &n : uploadedNames) imgArr.append(n);

    bool success = conn.request(cs::cmd::PublishGoods,
        QJsonObject{
            {"title", title}, {"description", desc},
            {"originalPrice", originalPrice}, {"condition", condition},
            {"currentPrice", currentPrice}, {"categoryId", categoryId},
            {"contact", contact}, {"images", imgArr}
        }, dataOut, err);

    if (success) {
        QMessageBox::information(this, QStringLiteral("发布成功"),
                                 QStringLiteral("「%1」已发布上架！").arg(title));
        accept();
    } else {
        QMessageBox::critical(this, QStringLiteral("发布失败"), err);
    }
}
