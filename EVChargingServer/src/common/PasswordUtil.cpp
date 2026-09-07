#include "common/PasswordUtil.h"
#include "common/AppConfig.h"

#include <QCryptographicHash>
#include <QUuid>

QString PasswordUtil::clientHash(const QString &rawPassword)
{
    const QByteArray hash = QCryptographicHash::hash(
                                rawPassword.toUtf8(),
                                QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());   // 64 位小写十六进制
}

bool PasswordUtil::isSaltedMode()
{
    return AppConfig::instance().passwordMode()
            == AppConfig::PasswordMode::SaltedHash;
}

QString PasswordUtil::hashForStorage(const QString &clientHashed)
{
    if (!isSaltedMode()) {
        // 明文模式: 直接存客户端传来的哈希串本身。
        // 注意这仍不是用户的原始明文口令 —— 客户端已做过一次 SHA-256,
        // 所以即使在此模式下, 网络上与数据库里也不会出现原始口令。
        return clientHashed;
    }

    // 加盐模式: 用 UUID 的 32 位十六进制串作为随机盐(128 bit 随机量),
    // 存储为 "salt$hash"。用 QUuid 避开 QRandomGenerator 各版本的 API 差异。
    const QString saltHex = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 盐的参与形式为 saltHex 的 ASCII 字节(verify 处保持一致)
    const QByteArray input = clientHashed.toUtf8() + saltHex.toLatin1();
    const QByteArray hash  = QCryptographicHash::hash(
                                 input, QCryptographicHash::Sha256);

    return saltHex + "$" + QString::fromLatin1(hash.toHex());
}

bool PasswordUtil::verify(const QString &clientHashed, const QString &stored)
{
    if (stored.isEmpty()) {
        return false;
    }

    if (!isSaltedMode()) {
        // 明文模式: 直接相等比较(定长字符串, 未做常量时间比较,
        // 若后续切换到加盐模式建议一并改为常量时间比较)
        return clientHashed == stored;
    }

    const int sep = stored.indexOf('$');
    if (sep <= 0) {
        // 库里的值是旧格式(例如切换模式前存入的明文哈希), 拒绝登录
        return false;
    }

    const QString saltHex = stored.left(sep);
    const QString expect  = stored.mid(sep + 1);

    // 与 hashForStorage 保持一致: 盐参与形式为 ASCII 字节
    const QByteArray input = clientHashed.toUtf8() + saltHex.toLatin1();
    const QByteArray hash  = QCryptographicHash::hash(
                                 input, QCryptographicHash::Sha256);

    return QString::fromLatin1(hash.toHex()) == expect;
}
