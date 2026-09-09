#ifndef CLIENTPROTOCOL_H
#define CLIENTPROTOCOL_H

#include <QByteArray>
#include <QDataStream>
#include <QVariantMap>

namespace ClientProtocol {

const quint32 MAGIC = 0xE7CB1234;
const int HEADER_SIZE = 12;
const quint32 MAX_BODY_SIZE = 1024 * 1024;

namespace Cmd {
    const quint16 REGISTER_REQ       = 0x1001;
    const quint16 REGISTER_RESP      = 0x1002;
    const quint16 LOGIN_REQ          = 0x1003;
    const quint16 LOGIN_RESP         = 0x1004;
    const quint16 LOGOUT_REQ         = 0x1005;
    const quint16 LOGOUT_RESP        = 0x1006;
    const quint16 FIND_PWD_REQ       = 0x1007;
    const quint16 FIND_PWD_RESP      = 0x1008;
    const quint16 CHANGE_PWD_REQ     = 0x1009;
    const quint16 CHANGE_PWD_RESP    = 0x100A;
    const quint16 RECHARGE_REQ       = 0x100B;
    const quint16 RECHARGE_RESP      = 0x100C;
    const quint16 BALANCE_QUERY_REQ  = 0x100D;
    const quint16 BALANCE_QUERY_RESP = 0x100E;
    const quint16 TRANSACTION_REQ    = 0x100F;
    const quint16 TRANSACTION_RESP   = 0x1010;
    const quint16 SMS_CODE_REQ       = 0x1011;
    const quint16 SMS_CODE_RESP      = 0x1012;
    const quint16 STATIONS_REQ       = 0x2001;
    const quint16 STATIONS_RESP      = 0x2002;
    const quint16 STATION_DETAIL_REQ = 0x2003;
    const quint16 STATION_DETAIL_RESP= 0x2004;
    const quint16 CHECK_ORDER_REQ    = 0x3001;
    const quint16 CHECK_ORDER_RESP   = 0x3002;
    const quint16 START_CHARGE_REQ   = 0x3003;
    const quint16 START_CHARGE_RESP  = 0x3004;
    const quint16 STOP_CHARGE_REQ    = 0x3005;
    const quint16 STOP_CHARGE_RESP   = 0x3006;
    const quint16 ERROR_RESP         = 0x9FFF;
}

namespace Err {
    const int OK = 0;
}

struct MessageHeader {
    quint32 magic = 0;
    quint32 requestId = 0;
    quint32 bodyLength = 0;
};

QByteArray packMessage(quint16 cmd, const QVariantMap &params, quint32 requestId);
bool tryParseMessage(QByteArray &buffer,
                     quint16 &outCmd,
                     QVariantMap &outParams,
                     quint32 &outReqId);

void writeHeader(QDataStream &stream, const MessageHeader &header);
void readHeader(QDataStream &stream, MessageHeader &header);

} // namespace ClientProtocol

#endif // CLIENTPROTOCOL_H
