#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QList>
#include <QPair>

// ===========================================================================
// 数据库端数据访问层(完整版)
//
// 职责: 作为整个平台的唯一数据访问入口, 直接读写 SQLite 数据库文件中的
//       六张业务表(users / admins / stations / chargers / orders /
//       wallet_records), 对上层的服务器端/用户端提供统一接口。
//
// 线程模型: 本类会被多个线程(GUI 线程、ServerCore 子线程)同时调用,
//           而 Qt 的 QSqlDatabase 连接是"每线程一条", 故内部按线程懒创建
//           独立连接, 调用方无需关心线程问题。
//
// 说明:
//   - 第 1~13 个接口为《数据库端与服务器端调用接口文档 V1.0》已定义的
//     接口, 签名保持不变;
//   - 其余接口为补充接口, 用于覆盖需求矩阵/通信协议要求的全部数据操作。
// ===========================================================================

// ---- 用户 ----
struct User
{
    int     id         = -1;
    QString phone;
    QString password;      // 注意: getUserInfo 不填此字段, 仅 getUserByPhone 填
    QString nickname;
    QString avatarPath;
    double  balance     = 0.0;
    QString status      = "normal";   // normal / frozen
    QString createdAt;
};

// ---- 管理员 ----
struct Admin
{
    int     id = -1;
    QString username;
};

// ---- 充电站 ----
struct Station
{
    int     id = -1;
    QString name;
    QString address;
    double  longitude = 0.0;
    double  latitude  = 0.0;
    QString createdAt;
};

// ---- 充电站(带桩数聚合, 供列表/附近查询) ----
struct StationBrief
{
    int     id = -1;
    QString name;
    QString address;
    double  longitude = 0.0;
    double  latitude  = 0.0;
    int     totalPiles  = 0;
    int     freePiles   = 0;
    int     onlinePiles = 0;
    double  distanceM   = -1.0;    // 米, -1 表示未参与距离计算
};

// ---- 充电桩 ----
struct Charger
{
    QString chargerCode;
    int     stationId     = -1;
    QString type          = "slow";   // fast / slow
    double  powerKw       = 0.0;
    QString status        = "idle";   // idle / charging / fault / offline
    int     chargeCount   = 0;
    int     totalDuration = 0;
};

// ---- 充电订单 ----
struct Order
{
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

// ---- 钱包流水 ----
struct WalletRecord
{
    int     id = -1;
    int     userId = -1;
    QString type;          // recharge / charge
    double  amount = 0.0;
    double  balanceAfter = 0.0;
    QString orderNo;
    QString createdAt;
};

// ---- 电桩状态分布 ----
struct ChargerStatusCount
{
    QString status;
    int     count = 0;
};

// ---- 营收汇总 ----
struct RevenueSummary
{
    double today = 0.0;
    double month = 0.0;
    double total = 0.0;
};

class DatabaseManager
{
public:
    // 获取唯一实例
    static DatabaseManager& instance();

    // ------------------------------------------------------------------
    // 接口文档 V1.0 已定义接口(签名保持不变)
    // ------------------------------------------------------------------

    // 初始化数据库(打开连接 + 启用外键; 不建表, 建表见 ensureSchemaAndSeed)
    bool initDatabase(const QString &dbPath);

    // 管理员登录
    bool adminLogin(const QString &username,
                    const QString &password);

    // 用户注册, 成功返回新用户ID, 失败返回 -1
    int registerUser(const QString &phone,
                     const QString &password,
                     const QString &nickname);

    // 获取用户信息(不含密码)
    bool getUserInfo(int userId, User &user);

    // 修改昵称
    bool updateNickname(int userId,
                        const QString &nickname);

    // 修改头像
    bool updateAvatar(int userId,
                      const QString &avatarPath);

    // 用户登录, 成功返回 user_id, 失败返回 -1
    int userLogin(const QString &phone,
                  const QString &password);

    // 修改密码(校验原密码一致后写入新密码)
    bool changePassword(int userId,
                        const QString &oldPassword,
                        const QString &newPassword);

    // 查询全部充电站
    QList<Station> getAllStations();

    // 查询某个充电站下的所有充电桩
    QList<Charger> getChargersByStation(int stationId);

    // 创建充电订单
    bool createOrder(const QString &orderNo,
                     int userId,
                     const QString &chargerCode,
                     double unitPrice);

    // 结束充电
    bool finishOrder(const QString &orderNo,
                     double energyKwh);

    // 支付订单
    bool payOrder(const QString &orderNo);

    // 用户充值
    bool recharge(int userId, double amount);

    // ------------------------------------------------------------------
    // 补充接口(覆盖需求矩阵 / 通信协议要求的其余数据操作)
    // ------------------------------------------------------------------

    // 建表(与 database.sql 一致)并在库为空时灌入演示数据
    bool ensureSchemaAndSeed();

    // 按手机号取用户(含密码与冻结状态, 供登录校验)
    bool getUserByPhone(const QString &phone, User &user);
    bool userExistsByPhone(const QString &phone);
    bool resetPasswordByPhone(const QString &phone, const QString &newPassword);

    // 用户列表 + 手机号/昵称模糊搜索(只读)
    QList<User> searchUsers(const QString &keyword, int page, int pageSize,
                            int &total);

    // 钱包流水分页查询
    QList<WalletRecord> getWalletRecords(int userId, const QString &from,
                                         const QString &to, int page,
                                         int pageSize, int &total);

    // 站点列表(带桩数聚合)
    QList<StationBrief> getStationBriefs();
    // 附近站点(Haversine 距离过滤排序)
    QList<StationBrief> getNearbyStations(double lat, double lon,
                                          double radiusM, int limit, int offset,
                                          int &total);
    Station getStationById(int stationId);

    // 按编号取电桩
    Charger getChargerByCode(const QString &chargerCode, bool &found);
    // 电桩状态分布
    QList<ChargerStatusCount> getChargerStatusDistribution();
    // 电桩列表
    QList<Charger> listChargers(int page, int pageSize, int &total,
                                const QString &stationFilter = QString());

    // 订单查询
    Order getOrderByNo(const QString &orderNo, bool &found);
    Order getUnfinishedOrder(int userId, bool &found);
    QList<Order> listOrders(int page, int pageSize, int &total,
                            const QString &statusFilter = QString(),
                            const QString &phoneFilter = QString());

    // 营收统计
    RevenueSummary getRevenueSummary();
    QList<QPair<QString, double>> getRevenueTrend(int days);

    // 生成订单号
    QString nextOrderNo();

    // ---- 管理员写操作(冻结/解冻、增改、远程重启、惰性删除) ----

    // 冻结/解冻用户(status: 'normal' / 'frozen')
    bool setUserStatus(int userId, const QString &status);

    // 充电站新增/编辑(站点删除因外键 RESTRICT 且无标记列, 不在本层提供)
    int  createStation(const QString &name, const QString &address,
                       double longitude, double latitude);
    bool updateStation(int id, const QString &name, const QString &address,
                       double longitude, double latitude);

    // 充电桩新增/编辑
    bool createCharger(const QString &chargerCode, int stationId,
                       const QString &type, double powerKw);
    bool updateCharger(const QString &chargerCode, const QString &type,
                       double powerKw);
    // 电桩状态设置: 远程重启=idle / 惰性删除=offline / 故障=fault
    bool setChargerStatus(const QString &chargerCode, const QString &status);

    // 取消订单(惰性删除: unpaid -> cancelled)
    bool cancelOrder(const QString &orderNo);

private:
    // 构造函数私有化
    DatabaseManager();

    // 禁止拷贝
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase database();          // 按当前线程懒创建连接
    bool exec(const QString &sql);    // 执行无结果 SQL
    bool seedDemoData();              // 灌入演示数据(仅库为空时)

    QString m_dbPath;
};

#endif // DATABASEMANAGER_H
