#include "ServerApp.h"
#include "ClientConnection.h"
#include "DbLock.h"

#include <QThread>
#include <QTimer>
#include <QWriteLocker>
#include <QDebug>

// 每批自动结算笔数：每批独立持写锁，批间回到事件循环让锁，
// 避免积压大量超时交易时长时间独占写锁导致全站请求卡顿/超时

ServerApp::ServerApp(QObject *parent)
    : QTcpServer(parent)
{

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