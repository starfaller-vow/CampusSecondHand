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
        cs::cmd::SuggestPrice,
        cs::cmd::ListCategories,
        cs::cmd::ListGoods,
        cs::cmd::GoodsDetail,
        cs::cmd::MineGoods,
        cs::cmd::FavoriteList,
        cs::cmd::MessageList,
        cs::cmd::MessageUnread,
        cs::cmd::BalanceLogs,
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
    if (type == cs::FrameType::Image) {
        handleImageFrame(payload);
        return;
    }
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

    // ===== 图片上传协议（连接层拦截，不经 Dispatcher）=====
    if (cmd == cs::cmd::ImageUpload) {
        const QString name = QFileInfo(params.value("name").toString()).fileName();
        const qint64 size  = static_cast<qint64>(params.value("size").toDouble());
        if (name.isEmpty() ||
            name.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")))) {
            err = QStringLiteral("非法文件名");
        } else {
            m_uploadName = name;
            m_uploadSize = size;
        }
    } else if (cmd == cs::cmd::ImageUploadDone) {
        const QString dir  = QCoreApplication::applicationDirPath() + "/images";
        const QString path = dir + "/" + m_uploadName;
        QFile f(path);
        if (m_uploadName.isEmpty() || !f.exists() || f.size() != m_uploadSize) {
            err = QStringLiteral("图片校验失败（传输不完整）");
        }
        m_uploadName.clear();
        m_uploadSize = 0;
    } else if (cmd == cs::cmd::ImageDownload) {
        // ===== 图片下载协议（连接层拦截）：响应 JSON 后紧跟发 Image 帧 =====
        const QString name = QFileInfo(params.value("name").toString()).fileName();
        if (name.isEmpty() ||
            name.contains(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")))) {
            err = QStringLiteral("非法文件名");
        } else {
            const QString path = QCoreApplication::applicationDirPath() + "/images/" + name;
            QFile f(path);
            if (!f.exists()) {
                err = QStringLiteral("图片不存在");
            } else if (!f.open(QIODevice::ReadOnly)) {
                err = QStringLiteral("图片读取失败");
            } else {
                m_pendingDownload = f.readAll();
                f.close();
                data.insert("name", name);
                data.insert("size", static_cast<qint64>(m_pendingDownload.size()));
            }
        }
    } else {
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

    // 图片下载：响应发出后紧接着发图片二进制帧（客户端按此顺序解析）
    if (cmd == cs::cmd::ImageDownload && err.isEmpty() && !m_pendingDownload.isEmpty()) {
        m_socket->write(makeFrame(cs::FrameType::Image, m_pendingDownload));
        m_socket->flush();
        m_pendingDownload.clear();
    }
}

void ClientConnection::handleImageFrame(const QByteArray &payload)
{
    if (m_uploadName.isEmpty()) {
        qWarning() << "收到图片帧但未声明上传，忽略";
        return;
    }
    const QString dir = QCoreApplication::applicationDirPath() + "/images";
    QDir().mkpath(dir);
    QFile f(dir + "/" + m_uploadName);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "图片写入失败:" << f.errorString();
        return;
    }
    f.write(payload);
    f.close();
    qInfo() << "图片已接收:" << m_uploadName << "(" << payload.size() << "字节)";
}

void ClientConnection::sendJson(const QJsonObject &obj)
{
    if (!m_socket) return;
    m_socket->write(makeJsonFrame(obj));
    m_socket->flush();
}