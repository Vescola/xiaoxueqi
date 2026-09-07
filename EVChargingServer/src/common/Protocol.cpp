#include "common/Protocol.h"

#include <QDataStream>
#include <QIODevice>

namespace Protocol {

void writeHeader(QDataStream &s, const MessageHeader &h)
{
    s.setByteOrder(QDataStream::LittleEndian);
    s << h.magic << h.requestId << h.bodyLength;
}

void readHeader(QDataStream &s, MessageHeader &h)
{
    s.setByteOrder(QDataStream::LittleEndian);
    s >> h.magic >> h.requestId >> h.bodyLength;
}

QByteArray packMessage(quint16 cmd, const QVariantMap &params, quint32 requestId)
{
    // 1. 先序列化包体: cmd(2 字节) + params
    QByteArray body;
    {
        QDataStream bs(&body, QIODevice::WriteOnly);
        bs.setByteOrder(QDataStream::LittleEndian);
        bs << cmd;
        bs << params;
    }

    // 2. 组装头部 + 包体
    MessageHeader header;
    header.magic      = MAGIC;
    header.requestId  = requestId;
    header.bodyLength = static_cast<quint32>(body.size());

    QByteArray result;
    {
        QDataStream rs(&result, QIODevice::WriteOnly);
        rs.setByteOrder(QDataStream::LittleEndian);
        writeHeader(rs, header);
        rs.writeRawData(body.constData(), body.size());
    }
    return result;
}

bool tryParseMessage(QByteArray &buffer,
                     quint16 &outCmd,
                     QVariantMap &outParams,
                     quint32 &outReqId)
{
    // 1. 头部不足 12 字节, 等待更多数据
    if (buffer.size() < HEADER_SIZE) {
        return false;
    }

    MessageHeader header;
    {
        QDataStream hs(buffer.left(HEADER_SIZE));
        readHeader(hs, header);
    }

    // 2. 魔数校验
    if (header.magic != MAGIC) {
        // 数据错乱: 丢弃头部一个字节继续尝试, 避免卡死
        buffer.remove(0, 1);
        return false;
    }

    // 3. 包体长度上限
    if (header.bodyLength > MAX_BODY_SIZE) {
        buffer.clear();   // 恶意超大包, 断开由调用方处理(返回一个错误标志)
        return false;
    }

    // 4. 完整包未到齐
    if (buffer.size() < HEADER_SIZE + static_cast<int>(header.bodyLength)) {
        return false;
    }

    // 5. 取出包体并反序列化
    const QByteArray bodyData =
        buffer.mid(HEADER_SIZE, static_cast<int>(header.bodyLength));

    {
        QDataStream bs(bodyData);
        bs.setByteOrder(QDataStream::LittleEndian);
        bs >> outCmd;
        bs >> outParams;
    }

    outReqId = header.requestId;

    // 6. 从缓冲区移除已处理的完整包
    buffer.remove(0, HEADER_SIZE + static_cast<int>(header.bodyLength));
    return true;
}

} // namespace Protocol
