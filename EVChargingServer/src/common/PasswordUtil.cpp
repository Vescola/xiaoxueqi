#include "common/passwordutil.h"

#include <QCryptographicHash>

QString PasswordUtil::clientHash(const QString &rawPassword)
{
    const QByteArray hash = QCryptographicHash::hash(
                                rawPassword.toUtf8(),
                                QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());   // 64 位小写十六进制
}

QString PasswordUtil::hashForStorage(const QString &clientHashed)
{
    // 口令在客户端已做过一次 SHA-256, 因此网络上传输的、以及数据库里保存的
    // 都不是用户的原始明文口令, 而是这一个 64 位哈希串。
    // 服务端不再做二次加工(原先预留的加盐分支已移除), 原样入库。
    return clientHashed;
}

bool PasswordUtil::verify(const QString &clientHashed, const QString &stored)
{
    if (stored.isEmpty()) {
        return false;
    }
    return clientHashed == stored;
}
