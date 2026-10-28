#ifndef PUBLISHGOODSDIALOG_H
#define PUBLISHGOODSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QList>

class PublishGoodsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PublishGoodsDialog(int sellerId, QWidget *parent = nullptr);

private slots:
    void onConditionChanged(int index);
    void onOriginalPriceChanged(const QString &text);
    void onSelectImages();
    void onSubmit();

private:
    void setupUI();
    void updateSuggestPrice();

    int m_sellerId;
    double m_suggestPrice = 0;
    QList<QString> m_selectedImages;  // 用户选的本地临时路径

    // UI
    QLineEdit *m_titleEdit;
    QTextEdit *m_descEdit;
    QLineEdit *m_originalEdit;
    QComboBox *m_conditionCombo;
    QLineEdit *m_currentEdit;
    QLabel *m_suggestLabel;
    QComboBox *m_categoryCombo;
    QLineEdit *m_contactEdit;
    QLabel *m_imagesLabel;
};

#endif
