#include "common/appconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

AppConfig::AppConfig()
{
}

AppConfig& AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

void AppConfig::load(const QString &cfgPath)
{
    if (cfgPath.isEmpty()) {
        m_cfgPath = QCoreApplication::applicationDirPath() + "/config.ini";
    } else {
        m_cfgPath = cfgPath;
    }

    QSettings set(m_cfgPath, QSettings::IniFormat);

    // 若配置文件不存在, 用默认值落一份, 方便直接改
    if (!QFileInfo::exists(m_cfgPath)) {
        set.beginGroup("Network");
        set.setValue("ListenPort",        m_listenPort);
        set.setValue("MaxPendingConn",    m_maxPendingConn);
        set.endGroup();

        set.beginGroup("Database");
        set.setValue("DbPath",   m_dbPath);
        set.setValue("AutoSeed", m_autoSeed);
        set.endGroup();

        set.beginGroup("Billing");
        set.setValue("PriceFast", m_priceFast);
        set.setValue("PriceSlow", m_priceSlow);
        set.endGroup();

        set.beginGroup("Security");
        set.setValue("PasswordMode", "plain");
        set.setValue("TokenValidDays", m_tokenValidDays);
        set.endGroup();

        set.beginGroup("Sms");
        set.setValue("DevCode",  m_smsDevCode);
        set.setValue("ValidSec", m_smsValidSec);
        set.endGroup();

        set.sync();
        return;
    }

    set.beginGroup("Network");
    m_listenPort          = static_cast<quint16>(
                                set.value("ListenPort", m_listenPort).toUInt());
    m_maxPendingConn      = set.value("MaxPendingConn", m_maxPendingConn).toInt();
    set.endGroup();

    set.beginGroup("Database");
    m_dbPath   = set.value("DbPath", m_dbPath).toString();
    m_autoSeed = set.value("AutoSeed", m_autoSeed).toBool();
    set.endGroup();

    set.beginGroup("Billing");
    m_priceFast = set.value("PriceFast", m_priceFast).toDouble();
    m_priceSlow = set.value("PriceSlow", m_priceSlow).toDouble();
    set.endGroup();

    set.beginGroup("Security");
    const QString mode = set.value("PasswordMode", "plain")
                            .toString().trimmed().toLower();
    // 目前只识别 saltedhash / sha256salt 为加盐模式, 其余一律按明文处理。
    // 后续切换到加盐哈希时, 只需把 ini 里的值改成 saltedhash, 无需改代码。
    if (mode == "saltedhash" || mode == "sha256salt" || mode == "salt") {
        m_passwordMode = PasswordMode::SaltedHash;
    } else {
        m_passwordMode = PasswordMode::Plain;
    }
    m_tokenValidDays = set.value("TokenValidDays", m_tokenValidDays).toInt();
    set.endGroup();

    set.beginGroup("Sms");
    m_smsDevCode  = set.value("DevCode", m_smsDevCode).toString();
    m_smsValidSec = set.value("ValidSec", m_smsValidSec).toInt();
    set.endGroup();

    // 相对路径统一解析到程序所在目录, 避免工作目录不同导致找不到库
    if (QDir::isRelativePath(m_dbPath)) {
        m_dbPath = QCoreApplication::applicationDirPath() + "/" + m_dbPath;
    }
}

double AppConfig::priceOf(const QString &chargerType) const
{
    return (chargerType.trimmed().toLower() == "fast") ? m_priceFast : m_priceSlow;
}
