#include "ClientConnection.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>

#include "Dispatcher.h"
#include "DbLock.h"
#include "PushRegistry.h"
#include "DatabaseManager.h"
#include "Protocol.h"

namespace {
// 纯只读命令白名单：处理过程中不写任何表、不开写事务。
// 未列入者一律按写命令加写锁（保守安全）。
bool isReadOnlyCommand(const QString &cmd)
{
    static const QSet<QString> kReadOnly = {
        cs::cmd::Login,            // 仅校验密码 + 内存发 token
        cs::cmd::Self,
        cs::cmd::UserGet,
        cs::cmd::ListCategories,
        cs::cmd::MessageList,
        cs::cmd::MessageUnread,
        cs::cmd::SettingsGet,
    };
    return kReadOnly.contains(cmd);
}
} // namespace

ClientConnection::ClientConnection(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_socketDescriptor(socketDescriptor)
{
}

ClientConnection::~ClientConnection()
{
    // 对象生存在工作线程，析构仍在该线程：安全关闭并移除线程专属 DB 连接
    DatabaseManager::instance().releaseCurrentThreadConnection();
}

void ClientConnection::init()
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(m_socketDescriptor)) {
        qWarning() << "接管 socket 失败:" << m_socket->errorString();
        emit finished();
        return;
    }
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientConnection::onDisconnected);
    qInfo() << "客户端接入:" << m_socket->peerAddress().toString()
            << ":" << m_socket->peerPort();
}

void ClientConnection::onReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        const QByteArray data = m_socket->readAll();
        QList<QPair<cs::FrameType, QByteArray>> frames;
        QString err;
        if (!m_reader.append(data, frames, err)) {
            qWarning() << "帧解析错误:" << err;
            emit finished();
            return;
        }
        for (const auto &f : frames) {
            handleFrame(f.first, f.second);
        }
    }
}

void ClientConnection::onDisconnected()
{
    PushRegistry::instance().unregisterConn(this);
    qInfo() << "客户端断开";
    emit finished();
}

void ClientConnection::pushJson(const QJsonObject &obj)
{
    if (m_socket) {
        m_socket->write(makeJsonFrame(obj));
        m_socket->flush();
    }
}

void ClientConnection::handleFrame(cs::FrameType type, const QByteArray &payload)
{
    // ---- 图片二进制帧：写入待上传文件 ----

    if (type != cs::FrameType::Json) {
        qWarning() << "收到未支持帧类型:" << static_cast<int>(type);
        return;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "JSON 解析失败:" << pe.errorString();
        return;
    }

    const QJsonObject req = doc.object();
    const quint32 reqId = static_cast<quint32>(req.value("reqId").toInt());
    const QString cmd   = req.value("cmd").toString();
    const QString token = req.value("token").toString();
    const QJsonObject params = req.value("params").toObject();

    QString err;
    QJsonObject data;

    // 解析出有效会话的用户，注册到推送表（每个请求都带 token，顺带刷新归属）
    int sessUid = 0;
    if (Dispatcher::instance().tokenUserId(token, sessUid) && sessUid > 0) {
        PushRegistry::instance().registerConn(sessUid, this);
    }

    {
        // 常规命令：读命令持读锁（多连接可并发查询），写命令持写锁（全局独占）。
        // 整个业务处理（含事务）全程持锁，处理完再释放。
        if (isReadOnlyCommand(cmd)) {
            QReadLocker locker(&serverDbLock());
            data = Dispatcher::instance().handle(cmd, params, token, err);
        } else {
            QWriteLocker locker(&serverDbLock());
            data = Dispatcher::instance().handle(cmd, params, token, err);
        }
    }

    QJsonObject resp;
    resp.insert("reqId", static_cast<int>(reqId));
    resp.insert("ok", err.isEmpty());
    if (err.isEmpty()) resp.insert("data", data);
    else resp.insert("err", err);

    if (cmd != cs::cmd::ImageUpload && cmd != cs::cmd::ImageUploadDone) {
        qInfo() << "请求" << cmd << "→" << (err.isEmpty() ? "ok" : ("失败:" + err));
    }
    sendJson(resp);


}



void ClientConnection::sendJson(const QJsonObject &obj)
{
    if (!m_socket) return;
    m_socket->write(makeJsonFrame(obj));
    m_socket->flush();
}