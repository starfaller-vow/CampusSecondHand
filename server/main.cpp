#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>

#include "DatabaseManager.h"
#include "ServerApp.h"

// ============================================================
// CampusServer：校园二手交易系统 - 服务端入口
// 数据（SQLite data.db + images/）在本进程所在目录
// 端口：9527
// ============================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 1. 初始化数据库（服务端独占）
    QString err;
    if (!DatabaseManager::instance().init(err)) {
        qCritical() << "数据库初始化失败:" << err;
        return 1;
    }

    // 2. 启动监听
    ServerApp server;
    if (!server.listen(QHostAddress::Any, 9527)) {
        qCritical() << "监听失败:" << server.errorString();
        return 1;
    }
    qInfo() << "CampusServer 已启动，端口 9527";

    // 3. 启动超时自动结算任务（启动即扫一次，之后每分钟扫一次）


    return app.exec();
}