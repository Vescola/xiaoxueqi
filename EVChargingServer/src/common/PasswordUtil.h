#ifndef PASSWORDUTIL_H
#define PASSWORDUTIL_H

#include <QString>

// ---------------------------------------------------------------------------
// 口令处理工具
//
// 因此本模块把"如何表示一个可存储的口令"收敛成两个函数:
//      hashForStorage()  入库前对客户端传来的口令做加工
//      verify()          登录时比对
// 调用方只面向这两个函数, 切换 PasswordMode 时无需改动任何业务代码。
//
// 【切换到加盐哈希的步骤】
//   config.ini -> [Security] PasswordMode=saltedhash
//   前提: 需要 users / admins 表能容纳 "salt$hash" 形式的字符串。
//         现有 password 列为 TEXT NOT NULL, 可直接容纳, 无需改表。
//   注意: 切换后旧明文账号将无法登录, 需先重置口令。
// ---------------------------------------------------------------------------

class PasswordUtil
{
public:
    // 客户端侧的口令哈希: SHA-256(原始口令) 的 64 位小写十六进制串。
    // 协议要求网络传输中绝不出现明文, 客户端发送前必须先做这一步。
    static QString clientHash(const QString &rawPassword);

    // 服务端入库加工。
    //   Plain      : 原样返回(当前默认, 与数据库端 Demo 行为一致)
    //   SaltedHash : 返回 "salt$hash", 其中 hash = SHA256(clientHash + salt)
    static QString hashForStorage(const QString &clientHashed);

    // 比对。stored 为数据库中取出的值, clientHashed 为客户端传来的哈希串。
    static bool verify(const QString &clientHashed, const QString &stored);

    // 当前是否为加盐模式
    static bool isSaltedMode();
};

#endif // PASSWORDUTIL_H
