#include <QApplication>
#include <QWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "ClientConnector.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle(QStringLiteral("校园二手交易系统 — 连接框架"));
    auto *layout = new QFormLayout(&window);
    auto *host = new QLineEdit("127.0.0.1");
    auto *status = new QLabel(QStringLiteral("请先启动 CampusServer"));
    auto *button = new QPushButton(QStringLiteral("连接服务端"));
    layout->addRow(QStringLiteral("服务器地址"), host);
    layout->addRow(button); layout->addRow(status);
    QObject::connect(button, &QPushButton::clicked, [&]() {
        QString error;
        bool ok=ClientConnector::instance().connectToServer(host->text(),9527,error);
        status->setText(ok ? QStringLiteral("连接成功，当前阶段仅有通信框架") : error);
    });
    window.resize(480,180); window.show(); return app.exec();
}
