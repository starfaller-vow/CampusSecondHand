#ifndef SERVERAPP_H
#define SERVERAPP_H

#include <QTcpServer>
#include <QSet>

class QThread;
class QTimer;

// 服务端主对象：监听端口；每个接入的连接分配一个独立线程 + ClientConnection
// 主线程另跑一个定时器：周期性扫描超时未确认收货的交易并自动结算（担保交易闭环）
class ServerApp : public QTcpServer
{
    Q_OBJECT

public:
    explicit ServerApp(QObject *parent = nullptr);

    // 在 listen 成功后调用：立即扫描一次过期交易，之后每分钟定时扫描

protected:
    // QTcpServer 回调：客户端接入
    void incomingConnection(qintptr socketDescriptor) override;

private slots:

private:
    QSet<QThread *> m_threads; // 生命周期管理：连接结束即回收线程
    QTimer *m_settleTimer = nullptr;
};

#endif // SERVERAPP_H