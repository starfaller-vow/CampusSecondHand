#include "Protocol.h"

#include <QJsonDocument>
#include <QtEndian>

QByteArray makeFrame(cs::FrameType type, const QByteArray &payload)
{
    QByteArray frame;
    frame.reserve(payload.size() + 5);
    frame.append(static_cast<char>(type));
    quint32 len = static_cast<quint32>(payload.size());
    // 大端写入长度
    char lenBytes[4];
    qToBigEndian(len, lenBytes);
    frame.append(lenBytes, 4);
    frame.append(payload);
    return frame;
}

QByteArray makeJsonFrame(const QJsonObject &obj)
{
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return makeFrame(cs::FrameType::Json, payload);
}

bool FrameReader::append(const QByteArray &data,
                         QList<QPair<cs::FrameType, QByteArray>> &outFrames,
                         QString &err)
{
    if (!data.isEmpty()) {
        m_buf.append(data);
    }

    for (;;) {
        // 头部 = 1字节类型 + 4字节长度
        if (m_buf.size() < 5) {
            break; // 半包：头部还没齐，等下次数据
        }

        const quint8 typeByte = static_cast<quint8>(m_buf.at(0));
        if (typeByte != static_cast<quint8>(cs::FrameType::Json) &&
            typeByte != static_cast<quint8>(cs::FrameType::Image)) {
            err = QStringLiteral("非法帧类型: 0x%1").arg(typeByte, 2, 16, QLatin1Char('0'));
            return false;
        }
        const cs::FrameType type = static_cast<cs::FrameType>(typeByte);

        // 读【单个】大端 quint32（单元素重载：只写 sizeof(quint32)=4 字节）
        // 注意：不能用三参重载 qFromBigEndian<T>(src, count, dest)——count 是"元素个数"，
        // 传 4 会被当成 4 个 quint32（16 字节）写进 dest，造成栈溢出
        const quint32 len = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar *>(m_buf.constData() + 1));
        if (len > cs::MaxPayload) {
            err = QStringLiteral("帧长度超限: %1 字节").arg(len);
            return false;
        }
        if (static_cast<quint32>(m_buf.size()) < 5u + len) {
            break; // 半包：负载没到齐，等下次数据
        }

        const QByteArray payload = m_buf.mid(5, static_cast<int>(len));
        m_buf.remove(0, 5 + static_cast<int>(len));
        outFrames.append(qMakePair(type, payload));
    }
    return true;
}