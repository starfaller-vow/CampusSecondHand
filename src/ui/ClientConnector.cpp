#include "ClientConnector.h"

#include <QTcpSocket>
#include <QTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QDebug>

#include "../shared/Protocol.h"

// 会改变服务端状态的命令：同一命令+相同参数在途时拒绝重复提交（防双击/短时间重复点击）
static const QSet<QString> kWriteCmds = {
};

ClientConnector::ClientConnector(QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    m_reader = new FrameReader();
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientConnector::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientConnector::onDisconnected);
}

ClientConnector &ClientConnector::instance()
{
    static ClientConnector c;
    return c;
}

bool ClientConnector::connectToServer(const QString &host, quint16 port, QString &err)
{
    if (isConnected()) return true;
    m_lastHost = host;
    m_lastPort = port;
    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(3000)) {
        err = QStringLiteral("无法连接服务器 %1:%2（%3）")
                  .arg(host).arg(port).arg(m_socket->errorString());
        return false;
    }
    return true;
}

void ClientConnector::disconnectFromServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->disconnectFromHost();
}

bool ClientConnector::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

bool ClientConnector::request(const QString &cmd, const QJsonObject &params,
                              QJsonObject &dataOut, QString &err, int timeOutMs)
{
    if (!isConnected()) {
        err = QStringLiteral("未连接服务器");
        return false;
    }

    // 写命令防重复提交：同 cmd+参数 的请求在途时直接拒绝（嵌套事件循环下仍生效）
    QString fingerprint;
    const bool isWrite = kWriteCmds.contains(cmd);
    if (isWrite) {
        fingerprint = cmd + QString::fromUtf8(
            QJsonDocument(params).toJson(QJsonDocument::Compact));
        if (m_inflightWrites.contains(fingerprint)) {
            err = QStringLiteral("操作正在处理中，请勿重复点击");
            return false;
        }
    }

    const quint32 reqId = m_nextReqId++;
    QJsonObject req;
    req.insert("reqId", static_cast<int>(reqId));
    req.insert("cmd", cmd);
    req.insert("token", m_token);
    req.insert("params", params);

    // 注册独立等待项：多个请求可同时在途，响应按 reqId 各回各家，互不覆盖
    PendingCall pc;
    QEventLoop loop;
    pc.loop = &loop;
    m_pending.insert(reqId, pc);
    if (isWrite) m_inflightWrites.insert(fingerprint);

    m_socket->write(makeJsonFrame(req));
    m_socket->flush();

    // 伪同步等待：本地事件循环保持运转（推送/嵌套请求均可处理），本请求只等自己的响应
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeOutMs);
    loop.exec();

    // 超时后清理自己的等待项；迟到响应会因 reqId 已不在表中而被安全丢弃
    const PendingCall result = m_pending.take(reqId);
    if (isWrite) m_inflightWrites.remove(fingerprint);

    if (!result.done) {
        err = QStringLiteral("请求超时：%1").arg(cmd);
        return false;
    }
    if (!result.ok) {
        err = result.err;
        return false;
    }
    dataOut = result.data;
    return true;
}





void ClientConnector::onReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        QList<QPair<cs::FrameType, QByteArray>> frames;
        QString e;
        if (!m_reader->append(m_socket->readAll(), frames, e)) {
            qWarning() << "客户端帧解析错误:" << e;
            m_socket->disconnectFromHost();
            return;
        }
        for (const auto &f : frames) {

            const QJsonDocument doc = QJsonDocument::fromJson(f.second);
            if (!doc.isObject()) continue;
            const QJsonObject obj = doc.object();

            // 推送帧：无 reqId，交给 UI（此时可能正处于任意请求的嵌套事件循环中）
            if (obj.contains("push")) {
                emit pushed(obj.value("push").toString(),
                            obj.value("data").toObject());
                continue;
            }

            const quint32 reqId = static_cast<quint32>(obj.value("reqId").toInt());

            // 普通响应：按 reqId 唤醒对应的等待项（嵌套请求互不影响）
            auto it = m_pending.find(reqId);
            if (it != m_pending.end()) {
                it->done = true;
                if (obj.value("ok").toBool()) {
                    it->ok   = true;
                    it->data = obj.value("data").toObject();
                } else {
                    it->ok  = false;
                    it->err = obj.value("err").toString();
                }
                if (it->loop) it->loop->quit();
            }
        }
    }
}

void ClientConnector::onDisconnected()
{
    emit disconnectedFromServer();
    // 唤醒全部在途 JSON 请求，避免任何一个嵌套等待卡死到超时
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        it->done = true;
        it->ok   = false;
        it->err  = QStringLiteral("与服务器的连接已断开");
        if (it->loop) it->loop->quit();
    }

}