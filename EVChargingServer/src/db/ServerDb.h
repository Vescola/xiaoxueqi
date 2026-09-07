#ifndef SERVERDB_H
#define SERVERDB_H

#include <QString>
#include <QList>
#include <QPair>

#include "db/DbTypes.h"

class QSqlDatabase;

// ---------------------------------------------------------------------------
// 服务器端自带数据访问层
//
// 背景: 数据库端 Demo 目前无法编译(9 处签名不匹配 + 若干未定义符号),
// 且其 13 个接口只能覆盖约 40% 需求。经项目决策:
//   - 不修改数据库端任何源码、不改动任何表结构;
//   - 服务器端自带本数据层, 直接读写 database.sql 定义的六张表;
//   - 密码当前沿用明文哈希比对, 通过 PasswordUtil 预留加盐切换。
//
// 线程模型: 本类会被 GUI 线程 与 ServerCore 子线程同时调用。
// Qt 的 QSqlDatabase 连接是"每个线程一条", 故内部按线程懒创建独立连接。
// ---------------------------------------------------------------------------

class ServerDb
{
public:
    static ServerDb& instance();

    // 打开数据库。成功返回 true。
    // autoSeed=true 时: 若库为空则自动建表(与 database.sql 一致)并灌演示数据。
    bool open(const QString &dbPath, bool autoSeed = true);

    bool isOpen();

    // ---- 建表与种子数据(供首次运行 / 演示) ----
    bool ensureSchemaAndSeed();

    // ---- 管理员 ----
    bool adminLogin(const QString &username, const QString &password);

    // ---- 用户 ----
    // 注册。返回: >0 新用户ID; -1 手机号已存在; -2 数据库错误
    int  registerUser(const QString &phone, const QString &pwdForStorage,
                      const QString &nickname);
    bool userExistsByPhone(const QString &phone);
    bool getUserByPhone(const QString &phone, UserInfo &out);
    bool getUserById(int userId, UserInfo &out);
    bool updateNickname(int userId, const QString &nickname);
    bool updateAvatar(int userId, const QString &avatarPath);
    bool recharge(int userId, double amount, double &newBalance);
    bool changePassword(int userId, const QString &newPwdForStorage);
    bool resetPasswordByPhone(const QString &phone, const QString &newPwdForStorage);
    // 管理员: 用户列表 + 手机号模糊搜索(只读)
    QList<UserInfo> searchUsers(const QString &keyword, int page, int pageSize,
                                int &total);

    // ---- 钱包流水 ----
    QList<WalletRecord> getWalletRecords(int userId, const QString &from,
                                         const QString &to, int page,
                                         int pageSize, int &total);

    // ---- 充电站 ----
    QList<StationInfo>   getAllStations();
    QList<StationBrief>  getStationBriefs();
    // 附近站点: 在 C++ 侧做 Haversine 距离过滤与排序(SQLite 无地理函数)
    QList<StationBrief>  getNearbyStations(double lat, double lon,
                                           double radiusM, int limit, int offset,
                                           int &total);
    StationInfo getStationById(int stationId);

    // ---- 充电桩 ----
    QList<ChargerInfo> getChargersByStation(int stationId);
    ChargerInfo getChargerByCode(const QString &chargerCode, bool &found);
    QList<StatusCount> getChargerStatusDistribution();
    // 管理员: 电桩列表(联表补站名)
    QList<ChargerInfo> listChargers(int page, int pageSize, int &total,
                                    const QString &stationFilter = QString());

    // ---- 订单 ----
    bool createOrder(const QString &orderNo, int userId,
                     const QString &chargerCode, double unitPrice);
    bool finishOrder(const QString &orderNo, double energyKwh,
                     double &amount, QString &endTime, int &durationMinutes);
    bool payOrder(const QString &orderNo, double &newBalance);
    OrderInfo getOrderByNo(const QString &orderNo, bool &found);
    OrderInfo getUnfinishedOrder(int userId, bool &found);
    QList<OrderInfo> listOrders(int page, int pageSize, int &total,
                                const QString &statusFilter = QString(),
                                const QString &phoneFilter = QString());

    // ---- 营收统计 ----
    RevenueSummary getRevenueSummary();
    // 近 N 日营收趋势, 返回 (日期 "yyyy-MM-dd", 金额); 缺失日期补 0
    QList<QPair<QString, double>> getRevenueTrend(int days);

    // ---- 工具 ----
    // 生成订单号: C + yyyyMMdd + 4 位序号(基于当日已有订单数)
    QString nextOrderNo();

private:
    ServerDb();
    ServerDb(const ServerDb&) = delete;
    ServerDb& operator=(const ServerDb&) = delete;

    QSqlDatabase database();   // 按当前线程懒创建连接
    bool exec(const QString &sql);   // 执行无结果 SQL
    bool seedDemoData();             // 灌入演示数据(仅库为空时)

    QString m_dbPath;
};

#endif // SERVERDB_H
