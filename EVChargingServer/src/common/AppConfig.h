#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QSettings>

// ---------------------------------------------------------------------------
// 全局配置

// 配置文件为程序运行目录下的 config.ini。
// 首次运行若文件不存在,会以本文件中的默认值自动创建一份,便于修改。
// ---------------------------------------------------------------------------

class AppConfig
{
public:
    static AppConfig& instance();

    // 载入配置。cfgPath 为空时使用运行目录下的 config.ini
    void load(const QString &cfgPath = QString());

    // ---- 网络 ----
    quint16 listenPort() const      { return m_listenPort; }
    int     maxPendingConn() const  { return m_maxPendingConn; }
    int     heartbeatTimeoutSec() const { return m_heartbeatTimeoutSec; }

    // ---- 数据库 ----
    QString dbPath() const          { return m_dbPath; }
    // 首次运行时若库为空,是否自动建表并灌入演示数据
    bool    autoSeed() const        { return m_autoSeed; }

    // ---- 计费 ----
    // 充电单价(元/kWh)。数据库 stations / chargers 表均无价格字段,
    // 依决策改为在 ini 中配置快充 / 慢充两档价,不改动数据库表结构。
    double  priceFast() const       { return m_priceFast; }
    double  priceSlow() const       { return m_priceSlow; }
    // 依据充电桩类型取价: fast -> 快充价, 其余 -> 慢充价
    double  priceOf(const QString &chargerType) const;

    // ---- 安全 ----
    // 密码处理模式。当前按决策沿用明文,后续可切换为加盐哈希而不改调用方。
    enum class PasswordMode {
        Plain,      // 明文存储/比对(当前默认)
        SaltedHash  // SHA256(客户端SHA256哈希 + 随机盐) —— 预留,暂不启用
    };
    PasswordMode passwordMode() const { return m_passwordMode; }

    // 会话 token 有效期(天)。协议 7.2 建议 7 天
    int     tokenValidDays() const  { return m_tokenValidDays; }

    // ---- 短信验证码(模拟) ----
    // 系统不接入真实短信网关,验证码仅在服务端生成并写入日志。
    // devCode 非空时,任何手机号都固定返回该验证码,方便联调。
    QString smsDevCode() const      { return m_smsDevCode; }
    int     smsValidSec() const     { return m_smsValidSec; }

    // ---- 界面 ----
    QString adminDefaultUser() const { return m_adminDefaultUser; }

    QString configFilePath() const  { return m_cfgPath; }

private:
    AppConfig();
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    quint16 m_listenPort        = 8888;
    int     m_maxPendingConn    = 128;
    int     m_heartbeatTimeoutSec = 120;

    QString m_dbPath            = "charging.db";
    bool    m_autoSeed          = true;

    double  m_priceFast         = 1.80;
    double  m_priceSlow         = 0.90;

    PasswordMode m_passwordMode = PasswordMode::Plain;

    int     m_tokenValidDays    = 7;

    QString m_smsDevCode        = "1234";
    int     m_smsValidSec       = 300;

    QString m_adminDefaultUser  = "admin";

    QString m_cfgPath;
};

#endif // APPCONFIG_H
