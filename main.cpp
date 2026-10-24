#include <QApplication>
#include <QDir>

#include "ui/LoginWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("CampusSecondHand");
    a.setApplicationVersion("1.0.0");

    // ===== 1. 确保程序同目录下有 images 文件夹（图片下载缓存目录）=====
    QDir().mkpath(QStringLiteral("images"));

    // ===== 2. 显示登录窗口（数据/数据库全部在服务端，客户端只走网络）=====
    LoginWindow w;
    w.show();

    return a.exec();
}