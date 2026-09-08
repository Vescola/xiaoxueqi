#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QDataStream>
#include <QVariantMap>

// ---------------------------------------------------------------------------
// 服务器端与用户端通信协议实现(与《CS通信协议.md》1.0 对齐)
//
// 数据包 = MessageHeader(12 字节, 小端) + Body(CommandPacket 序列化)
//   MessageHeader { quint32 magic; quint32 requestId; quint32 bodyLength; }
//   CommandPacket { quint16 cmd; QVariantMap params; }
// ---------------------------------------------------------------------------

namespace Protocol {

// 魔数。协议文档原文写作 0xEVCB1234, 其中 'V' 不是十六进制字符,
// 无法在 C++ 中作为整数字面量编译, 此处取形近的合法值 0xE7CB1234。
// 客户端与服务端只要使用同一份 Protocol 头文件即保持一致。
const quint32 MAGIC          = 0xE7CB1234;

// 头部固定 12 字节
const int     HEADER_SIZE    = 12;

// 包体上限 1MB, 防止恶意超大包(协议 3.5)
const quint32 MAX_BODY_SIZE  = 1024 * 1024;

// 命令码(协议第 4 章)
namespace Cmd {
    // 账户管理 0x1000~0x1FFF
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

    // 短信验证码(需求矩阵 #8/#10 要求验证码; 服务端模拟发送)
    const quint16 SMS_CODE_REQ       = 0x1011;
    const quint16 SMS_CODE_RESP      = 0x1012;

    // 充电站/桩查询 0x2000~0x2FFF
    const quint16 STATIONS_REQ       = 0x2001;
    const quint16 STATIONS_RESP      = 0x2002;
    const quint16 STATION_DETAIL_REQ = 0x2003;
    const quint16 STATION_DETAIL_RESP= 0x2004;

    // 充电业务 0x3000~0x3FFF
    const quint16 CHECK_ORDER_REQ    = 0x3001;
    const quint16 CHECK_ORDER_RESP   = 0x3002;
    const quint16 START_CHARGE_REQ   = 0x3003;
    const quint16 START_CHARGE_RESP  = 0x3004;
    const quint16 STOP_CHARGE_REQ    = 0x3005;
    const quint16 STOP_CHARGE_RESP   = 0x3006;
    const quint16 CHARGE_STATUS_REQ  = 0x3007;
    const quint16 CHARGE_STATUS_RESP = 0x3008;

    // 服务器主动推送 0x5000~0x5FFF(预留)
    const quint16 CHARGE_STATUS_NOTIFY = 0x5001;

    // 注: 原协议 0x6000~0x6FFF 为心跳(HEARTBEAT_REQ/RESP), 本工程未实现,
    // 收到该命令码会走 default 分支返回"未知命令码"。客户端不应发送。

    // 通用错误响应 0x9FFF
    const quint16 ERROR_RESP         = 0x9FFF;
}

// 全局错误码(协议 5.17)
namespace Err {
    const int OK               = 0;
    // 账户
    const int PHONE_EXISTS     = 1001;
    const int VERIFY_CODE_ERR  = 1002;
    const int NICKNAME_ERR     = 1003;
    const int PASSWORD_ERR     = 1004;
    const int PHONE_NOT_EXIST  = 1005;
    const int WRONG_PASSWORD   = 1006;
    const int ACCOUNT_FROZEN   = 1007;
    const int PHONE_UNREGISTER = 1008;
    const int OLD_PWD_ERR      = 1010;
    const int AMOUNT_INVALID   = 1011;
    // 充电业务
    const int BALANCE_LOW      = 3001;
    const int PILE_OCCUPIED    = 3002;
    const int PILE_FAULT       = 3003;
    const int UNFINISHED_ORDER = 3004;
    // 全局
    const int PARAM_MISSING    = 4001;
    const int PARAM_FORMAT     = 4002;
    const int NOT_LOGGED_IN    = 4003;
    const int ACCOUNT_DISABLED = 4004;
    const int DB_ERROR         = 4005;
    const int TIMESTAMP_EXPIRED= 4006;
    const int TOKEN_EXPIRED    = 4007;
    const int UNKNOWN          = 9999;
}

// 传输层头部
struct MessageHeader {
    quint32 magic      = 0;
    quint32 requestId  = 0;
    quint32 bodyLength = 0;
};

// 业务命令包
struct CommandPacket {
    quint16     cmd    = 0;
    QVariantMap params;
};

// 把一个命令包序列化为完整的网络数据包(含 12 字节头部)
QByteArray packMessage(quint16 cmd, const QVariantMap &params, quint32 requestId);

// 从缓冲区尝试取出一个完整包。成功返回 true 并从 buffer 中移除该包。
bool tryParseMessage(QByteArray &buffer,
                     quint16 &outCmd,
                     QVariantMap &outParams,
                     quint32 &outReqId);

// 把 MessageHeader 序列化进字节流(小端)
void writeHeader(QDataStream &s, const MessageHeader &h);
void readHeader(QDataStream &s, MessageHeader &h);

} // namespace Protocol

#endif // PROTOCOL_H
