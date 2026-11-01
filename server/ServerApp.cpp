#include "ServerApp.h"
#include "ClientConnection.h"
#include "DbLock.h"
#include "TransactionsManager.h"

#include <QThread>
#include <QTimer>
#include <QWriteLocker>
#include <QDebug>

// 每批自动结算笔数：每批独立持写锁，批间回到事件循环让锁，
// 避免积压大量超时交易时长时间独占写锁导致全站请求卡顿/超时
static constexpr int kSettleBatchSize = 10;

ServerApp::ServerApp(QObject *parent)
    : QTcpServer(parent)
{
    m_settleTimer = new QTimer(this);
    m_settleTimer->setInterval(60 * 1000); // 每分钟扫一次超时交易
    connect(m_settleTimer, &QTimer::timeout, this, &ServerApp::onAutoSettleTimer);
}

void ServerApp::startAutoSettle()
{
    // 启动时立即扫描一次（服务端重启后过期交易不丢失），随后每分钟一次
    onAutoSettleTimer();
    m_settleTimer->start();
}

void ServerApp::onAutoSettleTimer()
{
    int settled = 0;
    {
        // 与所有写请求共用同一把写锁；读请求持读锁仍可并发浏览
        QWriteLocker locker(&serverDbLock());
        settled = TransactionsManager::instance().autoSettleExpired(kSettleBatchSize);
    }
    if (settled > 0) {
        qInfo() << "自动结算：本批结算" << settled << "笔超时交易";
    }
    // 本批做满说明可能还有积压：稍后在事件循环空闲时继续下一批（已让锁）
    if (settled >= kSettleBatchSize) {
        QTimer::singleShot(0, this, &ServerApp::onAutoSettleTimer);
    }
}

void ServerApp::incomingConnection(qintptr socketDescriptor)
{
    // 每个连接一个线程：线程只负责该连接（收包/解析/持锁处理/回包）
    auto *thread = new QThread(this);
    auto *conn   = new ClientConnection(socketDescriptor);
    conn->moveToThread(thread);

    // 线程启动后接管 socket；连接结束 → 线程退出并清理
    connect(thread, &QThread::started, conn, &ClientConnection::init);
    connect(conn,   &ClientConnection::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, conn, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        m_threads.remove(thread);
        thread->deleteLater();
    });

    m_threads.insert(thread);
    thread->start();
}