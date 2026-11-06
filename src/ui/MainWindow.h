#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QHash>
#include <functional>

#include "../models/Models.h"

class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const User &user, QWidget *parent = nullptr);

    const User &currentUser() const { return m_user; }

private:
    void setupUI();

    User m_user;
    QTabWidget *m_tabWidget;
    bool m_goingBackToLogin = false; // 防止断线/踢下线同时触发重复回登录

    // 懒加载：每个页面首次显示时才执行的刷新函数（避免启动瞬间的请求风暴）
    QHash<QWidget*, std::function<void()>> m_lazyRefreshers;

    // 新消息推送防抖定时器：200ms 内的多条推送合并为一次消息列表刷新
    QTimer *m_msgDebounceTimer = nullptr;

    // 余额流水自动刷新钩子：Tab6「个人中心」填装；收到交易/结算/退款推送时自动刷新流水
    std::function<void()> m_balanceRefreshHook;

    // === 各 Tab（指针先占位，后续模块填充）===
    QWidget *m_tabBrowse;       // 商品浏览
    QWidget *m_tabMyGoods;      // 我的商品
    QWidget *m_tabOrders;       // 交易记录
    QWidget *m_tabFavorites;    // 收藏夹
    QWidget *m_tabMessages;     // 消息
    QWidget *m_tabProfile;      // 个人中心
    QWidget *m_tabAdmin;        // 管理员面板（仅管理员）
};

#endif // MAINWINDOW_H
