#include "ReportReviewDialog.h"

#include "ClientConnector.h"
#include "Protocol.h"

#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

bool request(QWidget *parent, const char *command, const QJsonObject &params, QJsonObject &data)
{
    QString error;
    if (ClientConnector::instance().request(command, params, data, error)) return true;
    QMessageBox::warning(parent, QStringLiteral("操作失败"), error);
    return false;
}

QString statusText(const QString &status)
{
    if (status == "pending") return QStringLiteral("待复核");
    if (status == "approved") return QStringLiteral("复核通过");
    if (status == "denied") return QStringLiteral("维持下架");
    return QStringLiteral("尚未申请复核");
}

void preview(QWidget *parent, int photoId)
{
    QJsonObject data;
    if (!request(parent, cs::cmd::ReviewPhoto, {{"photoId", photoId}}, data)) return;
    QPixmap photo;
    if (!photo.loadFromData(QByteArray::fromBase64(data.value("content").toString().toLatin1()))) {
        QMessageBox::warning(parent, QStringLiteral("照片"), QStringLiteral("无法显示照片"));
        return;
    }
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("复核照片"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *image = new QLabel(&dialog);
    image->setPixmap(photo.scaled(740, 560, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(image);
    dialog.exec();
}

QJsonArray choosePhotos(QWidget *parent, int remaining)
{
    QJsonArray photos;
    const QStringList paths = QFileDialog::getOpenFileNames(parent, QStringLiteral("选择复核照片"),
                                      {}, QStringLiteral("实物照片 (*.jpg *.jpeg *.png)"));
    if (paths.size() > remaining) {
        QMessageBox::warning(parent, QStringLiteral("照片"), QStringLiteral("最多还能选择 %1 张").arg(remaining));
        return {};
    }
    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() < 1 || file.size() > cs::ReviewPhotoBytes) {
            QMessageBox::warning(parent, QStringLiteral("照片"),
                                 QStringLiteral("照片不可读或超过 %1 MiB").arg(cs::ReviewPhotoMiB));
            return {};
        }
        photos.append(QString::fromLatin1(file.readAll().toBase64()));
    }
    return photos;
}

} // namespace

void openGoodsReviews(QWidget *parent, int goodsId, bool admin)
{
    QJsonObject data;
    if (!request(parent, cs::cmd::ReviewCases, {{"goodsId", goodsId}}, data)) return;
    const auto cases = data.value("cases").toArray();
    if (cases.isEmpty()) {
        if (admin && QMessageBox::question(parent, QStringLiteral("历史下架记录"),
                QStringLiteral("此商品没有可关联的复核记录。是否校验条件并恢复上架？"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            bool ok = false;
            const QString reason = QInputDialog::getMultiLineText(parent, QStringLiteral("历史商品恢复"),
                QStringLiteral("请输入恢复理由"), {}, &ok);
            if (ok && request(parent, cs::cmd::RestoreReport, {{"goodsId", goodsId}, {"reason", reason}}, data)) {
                QMessageBox::information(parent, QStringLiteral("处理完成"), QStringLiteral("商品已恢复上架"));
            }
        } else if (!admin) {
            QMessageBox::information(parent, QStringLiteral("复核"),
                                      QStringLiteral("暂无可关联的处置记录，请联系管理员"));
        }
        return;
    }
    QStringList labels;
    for (const auto &value : cases) {
        const QJsonObject entry = value.toObject();
        labels.append(QStringLiteral("处置 #%1 · %2 · %3")
            .arg(entry.value("id").toInt())
            .arg(statusText(entry.value("reviewStatus").toString()),
                 entry.value("reason").toString().simplified().left(60)));
    }
    int selected = 0;
    if (labels.size() > 1) {
        bool ok = false;
        const QString label = QInputDialog::getItem(parent, QStringLiteral("选择下架处置"),
                                                   QStringLiteral("请选择需要查看的处置记录"),
                                                   labels, 0, false, &ok);
        if (!ok) return;
        selected = labels.indexOf(label);
    }
    openReportReview(parent, cases.at(selected).toObject().value("id").toInt(), admin);
}

void openReportReview(QWidget *parent, int caseId, bool admin)
{
    QJsonObject data;
    if (!request(parent, cs::cmd::ReviewDetails, {{"caseId", caseId}}, data)) return;
    const QJsonObject review = data.value("review").toObject();
    const QString status = review.value("status").toString();
    const auto storedPhotos = data.value("photos").toArray();
    const bool maySubmit = !admin && status.isEmpty() && data.value("state").toString() == "down";
    const bool mayAdd = !admin && status == "pending" && storedPhotos.size() < cs::ReviewPhotoLimit;

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("下架复核 #%1").arg(caseId));
    dialog.resize(600, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *summary = new QTextEdit(&dialog);
    summary->setReadOnly(true);
    const QString category = data.value(admin ? "reportReason" : "reportCategory").toString();
    QString text = QStringLiteral("商品：%1\n举报类别：%2\n下架原因：%3\n复核状态：%4")
        .arg(data.value("goodsTitle").toString(), category.isEmpty() ? QStringLiteral("未记录") : category,
             data.value("reason").toString(), statusText(status));
    if (admin && data.value("reportId").toInt()) {
        text += QStringLiteral("\n\n原举报 #%1\n%2\n首次审核：%3")
            .arg(data.value("reportId").toInt())
            .arg(data.value("reportDescription").toString(), data.value("reportDecision").toString());
    }
    if (!review.isEmpty()) {
        text += QStringLiteral("\n\n卖家说明：%1").arg(review.value("description").toString());
        if (!review.value("reason").toString().isEmpty()) {
            text += QStringLiteral("\n复核理由：%1\n处理时间：%2")
                .arg(review.value("reason").toString(), review.value("handleTime").toString());
        }
    }
    summary->setPlainText(text);
    layout->addWidget(summary);
    auto *photosLayout = new QHBoxLayout;
    for (const auto &value : storedPhotos) {
        const int photoId = value.toInt();
        auto *button = new QPushButton(QStringLiteral("查看照片 #%1").arg(photoId), &dialog);
        QObject::connect(button, &QPushButton::clicked, &dialog, [&dialog, photoId]() { preview(&dialog, photoId); });
        photosLayout->addWidget(button);
    }
    layout->addLayout(photosLayout);
    if (maySubmit || mayAdd) {
        auto *description = new QTextEdit(&dialog);
        description->setPlaceholderText(QStringLiteral("请填写复核说明，最多 2000 字"));
        if (maySubmit) layout->addWidget(description);
        else description->hide();
        QJsonArray selected;
        const int remaining = cs::ReviewPhotoLimit - storedPhotos.size();
        const QString photoButtonText = QStringLiteral("选择照片（JPG/PNG，每张最多 %1 MiB）")
            .arg(cs::ReviewPhotoMiB);
        auto *choose = new QPushButton(photoButtonText, &dialog);
        auto *send = new QPushButton(maySubmit ? QStringLiteral("提交复核申请") : QStringLiteral("补交照片"), &dialog);
        layout->addWidget(choose);
        layout->addWidget(send);
        QObject::connect(choose, &QPushButton::clicked, &dialog, [&]() {
            const QJsonArray files = choosePhotos(&dialog, remaining);
            if (files.isEmpty()) return;
            selected = files;
            choose->setText(QStringLiteral("已选 %1 张，点击重新选择").arg(selected.size()));
        });
        if (maySubmit) {
            auto *clear = new QPushButton(QStringLiteral("移除本次选择的照片"), &dialog);
            layout->addWidget(clear);
            QObject::connect(clear, &QPushButton::clicked, &dialog, [&]() {
                selected = QJsonArray{};
                choose->setText(photoButtonText);
            });
        }
        QObject::connect(send, &QPushButton::clicked, &dialog, [&]() {
            QJsonObject result;
            if (request(&dialog, maySubmit ? cs::cmd::ReviewSubmit : cs::cmd::ReviewAddPhotos,
                        {{"caseId", caseId}, {"description", description->toPlainText()}, {"photos", selected}},
                        result)) {
                QMessageBox::information(&dialog, QStringLiteral("复核"), QStringLiteral("材料已提交，等待管理员复核"));
                dialog.accept();
            }
        });
        dialog.exec();
        return;
    }
    if (admin && status == "pending") {
        auto *actions = new QHBoxLayout;
        auto *approve = new QPushButton(QStringLiteral("恢复上架"), &dialog);
        auto *deny = new QPushButton(QStringLiteral("维持下架"), &dialog);
        actions->addWidget(approve);
        actions->addWidget(deny);
        layout->addLayout(actions);
        const auto decide = [&](const QString &decision) {
            bool ok = false;
            const QString reason = QInputDialog::getMultiLineText(&dialog, QStringLiteral("复核决定"),
                QStringLiteral("请输入给卖家的复核理由，避免包含举报人的身份信息"), {}, &ok);
            if (!ok) return;
            QJsonObject result;
            if (!request(&dialog, cs::cmd::ReviewDecide, {{"caseId", caseId}, {"decision", decision},
                         {"reason", reason}}, result)) return;
            const bool restored = result.value("restored").toBool();
            QMessageBox::information(&dialog, QStringLiteral("复核完成"),
                decision == "denied" ? QStringLiteral("已维持下架并通知卖家") :
                restored ? QStringLiteral("商品已恢复上架并通知卖家")
                         : QStringLiteral("本次复核通过，其他有效处置仍限制上架"));
            dialog.accept();
        };
        QObject::connect(approve, &QPushButton::clicked, &dialog, [&]() { decide("approved"); });
        QObject::connect(deny, &QPushButton::clicked, &dialog, [&]() { decide("denied"); });
        dialog.exec();
        return;
    }
    dialog.exec();
}
