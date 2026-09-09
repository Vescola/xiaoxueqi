#include "clientprotocol.h"

#include <QIODevice>

namespace ClientProtocol {

void writeHeader(QDataStream &stream, const MessageHeader &header)
{
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << header.magic << header.requestId << header.bodyLength;
}

void readHeader(QDataStream &stream, MessageHeader &header)
{
    stream.setByteOrder(QDataStream::LittleEndian);
    stream >> header.magic >> header.requestId >> header.bodyLength;
}

QByteArray packMessage(quint16 cmd, const QVariantMap &params, quint32 requestId)
{
    QByteArray body;
    {
        QDataStream bodyStream(&body, QIODevice::WriteOnly);
        bodyStream.setByteOrder(QDataStream::LittleEndian);
        bodyStream << cmd;
        bodyStream << params;
    }

    MessageHeader header;
    header.magic = MAGIC;
    header.requestId = requestId;
    header.bodyLength = static_cast<quint32>(body.size());

    QByteArray packet;
    {
        QDataStream packetStream(&packet, QIODevice::WriteOnly);
        packetStream.setByteOrder(QDataStream::LittleEndian);
        writeHeader(packetStream, header);
        packetStream.writeRawData(body.constData(), body.size());
    }
    return packet;
}

bool tryParseMessage(QByteArray &buffer,
                     quint16 &outCmd,
                     QVariantMap &outParams,
                     quint32 &outReqId)
{
    if (buffer.size() < HEADER_SIZE) {
        return false;
    }

    MessageHeader header;
    {
        QDataStream headerStream(buffer.left(HEADER_SIZE));
        readHeader(headerStream, header);
    }

    if (header.magic != MAGIC) {
        buffer.remove(0, 1);
        return false;
    }

    if (header.bodyLength > MAX_BODY_SIZE) {
        buffer.clear();
        return false;
    }

    if (buffer.size() < HEADER_SIZE + static_cast<int>(header.bodyLength)) {
        return false;
    }

    const QByteArray body = buffer.mid(HEADER_SIZE, static_cast<int>(header.bodyLength));
    {
        QDataStream bodyStream(body);
        bodyStream.setByteOrder(QDataStream::LittleEndian);
        bodyStream >> outCmd;
        bodyStream >> outParams;
    }

    outReqId = header.requestId;
    buffer.remove(0, HEADER_SIZE + static_cast<int>(header.bodyLength));
    return true;
}

} // namespace ClientProtocol
