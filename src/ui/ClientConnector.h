#ifndef CLIENTCONNECTOR_H
#define CLIENTCONNECTOR_H

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QtGlobal>
#include <QHash>
#include <QSet>

class QTcpSocket;
class QEventLoop;
class FrameReader;

// ============================================================
// 客户端网络层（单例）：
// · 伪同步请求：request() 发命令并阻塞等待响应（带超时）——UI 线程调用
// · socket 与解析全部走 UI 线程事件循环，无需额外线程
// · 服务端推送帧（无 reqId）转成 pushed 信号（M5 启用）
// ============================================================
class ClientConnector : public QObject
{
    Q_OBJECT

public:
    static ClientConnector &instance();

    // 阻塞式连接（3 秒超时）；成功后记住地址供后续自动重连
    bool connectToServer(const QString &host, quint16 port, QString &err);
    void disconnectFromServer();
    bool isConnected() const;

    /**
     * @brief 发送命令并等待响应（伪同步，超时 timeOutMs 毫秒）
     * @return 成功 dataOut 已填充；失败 err 为原因（连接断/超时/服务端错误）
     */
    bool request(const QString &cmd, const QJsonObject &params,
                 QJsonObject &dataOut, QString &err, int timeOutMs = 15000);

    // 发送图片二进制帧（上传协议：request(image.upload) 声明 → sendImageFrame(bytes) → request(image.uploadDone)）
    bool sendImageFrame(const QByteArray &payload);

    /**
     * @brief 从服务端下载图片（伪同步，超时 timeOutMs 毫秒）
     *        协议：request(image.download{name}) → 响应 JSON 后紧接一个 Image 帧
     * @return 成功 out 为图片字节；失败 err 为原因
     */
    bool downloadImage(const QString &name, QByteArray &out,
                       QString &err, int timeOutMs = 20000);

    // ===== 会话 =====
    QString token() const { return m_token; }
    void setToken(const QString &t) { m_token = t; }
    int myUserId() const { return m_userId; }
    void setMyUserId(int id) { m_userId = id; }

    // 最近一次连接的服务器地址（登录窗默认值，重连不必重填 IP）
    QString lastHost() const { return m_lastHost; }
    quint16 lastPort() const { return m_lastPort; }

signals:
    void pushed(const QString &type, const QJsonObject &data); // 服务端推送
    void disconnectedFromServer();                             // 连接断开

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    explicit ClientConnector(QObject *parent = nullptr);

    QTcpSocket *m_socket = nullptr;
    FrameReader *m_reader = nullptr;
    quint32 m_nextReqId = 1;

    QString m_token;
    int m_userId = 0;
    QString m_lastHost = "localhost";
    quint16 m_lastPort = 9527;

    // 一个在途 JSON 请求的等待项（支持同连接多请求并发在途，响应按 reqId 各自唤醒）
    struct PendingCall {
        QEventLoop *loop = nullptr;
        bool done = false;
        bool ok = false;
        QJsonObject data;
        QString err;
    };
    QHash<quint32, PendingCall> m_pending;   // reqId -> 等待项
    QSet<QString> m_inflightWrites;          // 在途写命令指纹（cmd+参数），防短时间重复提交

    // 图片下载独立等待槽：Image 帧不带 reqId，同一连接同时只允许一个下载在途
    quint32 m_imgReqId = 0;
    QEventLoop *m_imgLoop = nullptr;
    int m_imgPhase = 0;        // 0=空闲 1=已收到响应等图片帧
    QByteArray m_imgBuffer;
    QString m_imgErr;
    bool m_imgDone = false;
};

#endif // CLIENTCONNECTOR_H