#ifndef BROWSEGOODSPAGE_H
#define BROWSEGOODSPAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>
#include "models/Models.h"

class BrowseGoodsPage : public QWidget
{
    Q_OBJECT

public:
    explicit BrowseGoodsPage(int currentUserId, QWidget *parent = nullptr);

    int currentUserId() const { return m_currentUserId; }

public slots:
    void refresh();

private slots:
    void onSearch();
    void onGoodsCardClicked(int goodsId);

private:
    void setupUI();
    void populateCards();

    int m_currentUserId;

    QLineEdit *m_keywordEdit;
    QComboBox *m_categoryCombo;
    QLineEdit *m_minPriceEdit;
    QLineEdit *m_maxPriceEdit;
    QComboBox *m_sortCombo;
    QScrollArea *m_scrollArea;
    QWidget *m_gridContainer;
    QVBoxLayout *m_gridLayout;
};

#endif
