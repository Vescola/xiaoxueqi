#ifndef DBTYPES_H
#define DBTYPES_H

#include <QString>
#include <QList>
#include <QVariantList>

// ---------------------------------------------------------------------------
// 数据结构定义。字段与数据库端 database.sql 六张表严格对齐:
//   users / admins / stations / chargers / orders / wallet_records
// 不改动任何表结构, 仅在服务器端用这些结构体承载查询结果。
// ---------------------------------------------------------------------------

// 用户
struct UserInfo {
    int     id         = -1;
    QString phone;
    QString password;    // 库中存储的口令表示(明文哈希 或 salt$hash)
    QString nickname;
    QString avatarPath;
    double  balance     = 0.0;
    QString status      = "normal";   // normal / frozen
    QString createdAt;
};

// 管理员
struct AdminInfo {
    int     id = -1;
    QString username;
};

// 充电站(基础)
struct StationInfo {
    int     id = -1;
    QString name;
    QString address;
    double  longitude = 0.0;
    double  latitude  = 0.0;
    QString createdAt;
};

// 充电站(带桩数聚合, 供列表/附近查询)
struct StationBrief {
    int     id = -1;
    QString name;
    QString address;
    double  longitude = 0.0;
    double  latitude  = 0.0;
    int     totalPiles = 0;
    int     freePiles  = 0;
    int     onlinePiles= 0;
    double  distanceM  = -1.0;    // 米, -1 表示未参与距离计算
};

// 充电桩
struct ChargerInfo {
    QString chargerCode;
    int     stationId    = -1;
    QString type         = "slow";  // fast / slow
    double  powerKw      = 0.0;
    QString status       = "idle";  // idle / charging / fault / offline
    int     chargeCount  = 0;
    int     totalDuration= 0;
};

// 充电订单
struct OrderInfo {
    QString orderNo;
    int     userId       = -1;
    QString chargerCode;
    QString status       = "charging"; // charging / unpaid / paid / cancelled
    QString startTime;
    QString endTime;
    int     durationMinutes = 0;
    double  energyKwh    = 0.0;
    double  unitPrice    = 0.0;
    double  amount       = 0.0;
    QString createdAt;
    // 联表补充(仅管理员订单页)
    QString userPhone;
};

// 钱包流水
struct WalletRecord {
    int     id = -1;
    int     userId = -1;
    QString type;          // recharge / charge
    double  amount = 0.0;
    double  balanceAfter = 0.0;
    QString orderNo;
    QString createdAt;
};

// 电桩状态分布(需求矩阵 #33)
struct StatusCount {
    QString status;
    int     count = 0;
};

// 营收汇总(需求矩阵 #31)
struct RevenueSummary {
    double today = 0.0;
    double month = 0.0;
    double total = 0.0;
};

// 登录校验结果
struct LoginResult {
    int     code   = -1;   // 0 成功; 非 0 为协议错误码
    int     userId = -1;
    QString nickname;
    double  balance = 0.0;
    QString status;
};

#endif // DBTYPES_H
