#ifndef PASSWORDUTIL_H
#define PASSWORDUTIL_H

#include <QString>

// ---------------------------------------------------------------------------
// 口令处理工具
//
// 本模块把"如何表示一个可存储的口令"收敛成几个函数:
//      clientHash()      客户端侧哈希(协议说明用, 服务端一般不调用)
//      hashForStorage()  入库前对客户端传来的口令做加工
//      verify()          登录时比对
// 调用方只面向 hashForStorage() / verify() 两个函数。
//
// 当前策略: 口令由客户端做一次 SHA-256 后发送, 服务端原样存储与比对。
// 因此网络上传输的、数据库里保存的都不是用户的原始明文口令。
// 服务端不再做二次加工(原先预留的加盐分支已移除)。
// ---------------------------------------------------------------------------

class PasswordUtil
{
public:
    // 客户端侧的口令哈希: SHA-256(原始口令) 的 64 位小写十六进制串。
    // 协议要求网络传输中绝不出现明文, 客户端发送前必须先做这一步。
    static QString clientHash(const QString &rawPassword);

    // 服务端入库加工: 当前原样返回客户端传来的哈希串。
    static QString hashForStorage(const QString &clientHashed);

    // 比对。stored 为数据库中取出的值, clientHashed 为客户端传来的哈希串。
    static bool verify(const QString &clientHashed, const QString &stored);
};

#endif // PASSWORDUTIL_H
