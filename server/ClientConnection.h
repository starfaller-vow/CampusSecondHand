#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <QObject>
#include <QJsonObject>
#include <QtGlobal>

#include "Protocol.h"

class QTcpSocket;

// 单个客户端连接（每个连接运行在独立线程中）：
// 本线程只负责 socket 收发与帧解析；
// 业务处理时持 serverDbLock()（读命令共享读锁、写命令独占写锁）全程持锁，
// 配合每线程独立 SQLite 连接与 WAL 实现多读单写
class ClientConnection : public QObject
{
    Q_OBJECT

public:
    explicit ClientConnection(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientConnection(); // 在所属工作线程析构：释放其专属 DB 连接

public slots:
    void init(); // 线程启动后调用：接管 socket 描述符
    void pushJson(const QJsonObject &obj); // 推送帧（PushRegistry 跨线程 Queued 调用）

signals:
    void finished(); // 连接结束：通知外层退出线程

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void handleFrame(cs::FrameType type, const QByteArray &payload);
    void handleImageFrame(const QByteArray &payload); // 图片二进制帧：写入待上传文件
    void sendJson(const QJsonObject &obj);

    qintptr m_socketDescriptor;
    QTcpSocket *m_socket = nullptr;
    FrameReader m_reader;

    // 图片上传挂起槽（上传协议：upload 声明 → Image 帧 → uploadDone 校验）
    QString m_uploadName;
    qint64 m_uploadSize = 0;

    // 图片下载挂起缓冲（下载协议：image.download 响应后紧接发 Image 帧）
    QByteArray m_pendingDownload;
};

#endif // CLIENTCONNECTION_H