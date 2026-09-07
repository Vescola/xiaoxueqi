#include "db/ServerDb.h"
#include "common/PasswordUtil.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <QDate>
#include <QMap>

// ---------------------------------------------------------------------------
// 内部小工具
// ---------------------------------------------------------------------------

namespace {

const double kEarthRadiusM = 6371000.0;
const double kPi = 3.14159265358979323846;

double toRad(double deg)
{
    return deg * kPi / 180.0;
}

// Haversine 球面距离(米)
double haversineM(double lat1, double lon1, double lat2, double lon2)
{
    const double dLat = toRad(lat2 - lat1);
    const double dLon = toRad(lon2 - lon1);
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2)
                   + std::cos(toRad(lat1)) * std::cos(toRad(lat2))
                   * std::sin(dLon / 2) * std::sin(dLon / 2);
    const double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return kEarthRadiusM * c;
}

} // namespace

// ---------------------------------------------------------------------------
// 单例
// ---------------------------------------------------------------------------

ServerDb::ServerDb()
{
}

ServerDb& ServerDb::instance()
{
    static ServerDb db;
    return db;
}

// ---------------------------------------------------------------------------
// 每线程独立连接。Qt 的 QSqlDatabase 连接不能跨线程共享,
// 而本数据层会同时被 GUI 线程与 ServerCore 子线程调用。
// ---------------------------------------------------------------------------

QSqlDatabase ServerDb::database()
{
    const QString connName = QStringLiteral("evsrv_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);

    if (QSqlDatabase::contains(connName)) {
        return QSqlDatabase::database(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning() << "[ServerDb] 打开数据库失败:" << db.lastError().text();
    }
    return db;
}

bool ServerDb::exec(const QString &sql)
{
    QSqlQuery q(database());
    if (!q.exec(sql)) {
        qWarning() << "[ServerDb] SQL 失败:" << q.lastError().text() << "\n  SQL:" << sql;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 打开 / 建表 / 种子
// ---------------------------------------------------------------------------

bool ServerDb::open(const QString &dbPath, bool autoSeed)
{
    m_dbPath = dbPath;

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return false;
    }

    exec("PRAGMA foreign_keys = ON");

    if (autoSeed) {
        ensureSchemaAndSeed();
    }
    return true;
}

bool ServerDb::isOpen()
{
    return database().isOpen();
}

bool ServerDb::ensureSchemaAndSeed()
{
    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return false;
    }

    // ---- 建表: 与 database.sql 完全一致(仅把 IF NOT EXISTS 语义保留) ----
    exec("CREATE TABLE IF NOT EXISTS users ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " phone TEXT NOT NULL UNIQUE,"
         " password TEXT NOT NULL,"
         " nickname TEXT NOT NULL,"
         " avatar_path TEXT,"
         " balance REAL NOT NULL DEFAULT 0.0,"
         " status TEXT NOT NULL DEFAULT 'normal' CHECK(status IN ('normal','frozen')),"
         " created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP)");

    exec("CREATE TABLE IF NOT EXISTS admins ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " username TEXT NOT NULL UNIQUE,"
         " password TEXT NOT NULL,"
         " created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP)");

    exec("CREATE TABLE IF NOT EXISTS stations ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " name TEXT NOT NULL,"
         " address TEXT NOT NULL,"
         " longitude REAL NOT NULL,"
         " latitude REAL NOT NULL,"
         " created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP)");

    exec("CREATE TABLE IF NOT EXISTS chargers ("
         " charger_code TEXT PRIMARY KEY,"
         " station_id INTEGER NOT NULL,"
         " type TEXT NOT NULL CHECK(type IN ('fast','slow')),"
         " power_kw REAL NOT NULL,"
         " status TEXT NOT NULL DEFAULT 'idle'"
         "   CHECK(status IN ('idle','charging','fault','offline')),"
         " charge_count INTEGER NOT NULL DEFAULT 0,"
         " total_duration INTEGER NOT NULL DEFAULT 0,"
         " FOREIGN KEY(station_id) REFERENCES stations(id)"
         "   ON UPDATE CASCADE ON DELETE RESTRICT)");

    exec("CREATE TABLE IF NOT EXISTS orders ("
         " order_no TEXT PRIMARY KEY,"
         " user_id INTEGER NOT NULL,"
         " charger_code TEXT NOT NULL,"
         " status TEXT NOT NULL"
         "   CHECK(status IN ('charging','unpaid','paid','cancelled')),"
         " start_time DATETIME,"
         " end_time DATETIME,"
         " duration_minutes INTEGER NOT NULL DEFAULT 0,"
         " energy_kwh REAL NOT NULL DEFAULT 0.0,"
         " unit_price REAL NOT NULL DEFAULT 0.0,"
         " amount REAL NOT NULL DEFAULT 0.0,"
         " created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
         " FOREIGN KEY(user_id) REFERENCES users(id)"
         "   ON UPDATE CASCADE ON DELETE RESTRICT,"
         " FOREIGN KEY(charger_code) REFERENCES chargers(charger_code)"
         "   ON UPDATE CASCADE ON DELETE RESTRICT)");

    exec("CREATE TABLE IF NOT EXISTS wallet_records ("
         " id INTEGER PRIMARY KEY AUTOINCREMENT,"
         " user_id INTEGER NOT NULL,"
         " type TEXT NOT NULL CHECK(type IN ('recharge','charge')),"
         " amount REAL NOT NULL,"
         " balance_after REAL NOT NULL,"
         " order_no TEXT,"
         " created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
         " FOREIGN KEY(user_id) REFERENCES users(id)"
         "   ON UPDATE CASCADE ON DELETE RESTRICT,"
         " FOREIGN KEY(order_no) REFERENCES orders(order_no)"
         "   ON UPDATE CASCADE ON DELETE RESTRICT)");

    // ---- 默认管理员 ----
    exec("INSERT OR IGNORE INTO admins (username, password) VALUES ('admin', '123456')");

    // 便于和用户端联调: 演示用户统一使用 Demo@123 的客户端哈希。
    {
        QSqlQuery patchPassword(database());
        patchPassword.prepare("UPDATE users SET password = ?"
                              " WHERE phone IN ('13800138000', '13912345678', '13700001111')"
                              " AND (password IS NULL OR password = '')");
        patchPassword.addBindValue(PasswordUtil::clientHash(QStringLiteral("Demo@123")));
        patchPassword.exec();
    }

    // ---- 演示种子数据: 仅当库为空时灌入 ----
    {
        QSqlQuery cnt(database());
        cnt.exec("SELECT COUNT(*) FROM stations");
        int stationCount = 0;
        if (cnt.next()) {
            stationCount = cnt.value(0).toInt();
        }
        if (stationCount == 0) {
            seedDemoData();
        }
    }
    return true;
}

namespace {
// 受限于篇幅, 种子数据直接内联 SQL 语句执行
void seedChargers(QSqlDatabase &db, int stationId, const QString &codePrefix,
                  const QString &type, double power, int count,
                  const QString &statusPattern)
{
    for (int i = 1; i <= count; ++i) {
        const QString code = codePrefix + QString("-%1").arg(i, 2, 10, QChar('0'));
        // statusPattern: "idle/charging/fault/offline" 循环分布
        QString status;
        const QStringList all = QStringList() << "idle" << "idle" << "idle"
                                              << "charging" << "fault" << "offline";
        status = all[(i - 1) % all.size()];

        QSqlQuery q(db);
        q.prepare("INSERT OR IGNORE INTO chargers"
                  " (charger_code, station_id, type, power_kw, status)"
                  " VALUES (?, ?, ?, ?, ?)");
        q.addBindValue(code);
        q.addBindValue(stationId);
        q.addBindValue(type);
        q.addBindValue(power);
        q.addBindValue(status);
        q.exec();
        Q_UNUSED(statusPattern);
    }
}
} // namespace

bool ServerDb::seedDemoData()
{
    QSqlDatabase db = database();

    // 三个演示站点(北京)
    struct SeedStation { const char *name; const char *addr; double lon; double lat; };
    const SeedStation sts[] = {
        { "朝阳公园充电站", "朝阳区公园路1号", 116.4821, 39.9333 },
        { "中关村软件园站", "海淀区软件园西二路", 116.2896, 40.0452 },
        { "望京SOHO充电站", "朝阳区阜通东大街6号", 116.4810, 39.9962 },
    };
    QList<int> stationIds;
    for (const auto &s : sts) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO stations (name, address, longitude, latitude)"
                  " VALUES (?, ?, ?, ?)");
        q.addBindValue(QString::fromUtf8(s.name));
        q.addBindValue(QString::fromUtf8(s.addr));
        q.addBindValue(s.lon);
        q.addBindValue(s.lat);
        if (q.exec()) {
            stationIds << q.lastInsertId().toInt();
        }
    }

    if (stationIds.size() >= 3) {
        seedChargers(db, stationIds[0], "SZ001", "fast", 60.0, 6, "");
        seedChargers(db, stationIds[0], "SZ001-S", "slow", 7.0, 4, "");
        seedChargers(db, stationIds[1], "SZ002", "fast", 120.0, 8, "");
        seedChargers(db, stationIds[1], "SZ002-S", "slow", 7.0, 6, "");
        seedChargers(db, stationIds[2], "SZ003", "fast", 60.0, 5, "");
        seedChargers(db, stationIds[2], "SZ003-S", "slow", 3.5, 5, "");
    }

    // 演示用户
    QSqlQuery u(db);
    const QString demoPassword = PasswordUtil::clientHash(QStringLiteral("Demo@123"));
    const char *userSql =
        "INSERT OR IGNORE INTO users (phone, password, nickname, balance)"
        " VALUES (?, ?, ?, ?)";
    u.prepare(userSql);
    u.addBindValue("13800138000"); u.addBindValue(demoPassword); u.addBindValue("张伟"); u.addBindValue(120.5);
    u.exec();
    u.prepare(userSql);
    u.addBindValue("13912345678"); u.addBindValue(demoPassword); u.addBindValue("李娜"); u.addBindValue(66.0);
    u.exec();
    u.prepare(userSql);
    u.addBindValue("13700001111"); u.addBindValue(demoPassword); u.addBindValue("王强"); u.addBindValue(0.0);
    u.exec();

    QSqlQuery patchPassword(db);
    patchPassword.prepare("UPDATE users SET password = ?"
                          " WHERE phone IN ('13800138000', '13912345678', '13700001111')"
                          " AND (password IS NULL OR password = '')");
    patchPassword.addBindValue(demoPassword);
    patchPassword.exec();

    const QList<int> userIds = { 1, 2, 3 };

    // 近 30 天已支付订单(供营收折线图)
    QDateTime now = QDateTime::currentDateTime();
    const QStringList chargerCodes = {
        "SZ001-01", "SZ001-02", "SZ002-01", "SZ002-02", "SZ003-01"
    };
    const QString fmt = "yyyy-MM-dd HH:mm:ss";
    int orderSeq = 1;

    for (int d = 29; d >= 0; --d) {
        const QDateTime day = now.addDays(-d);
        const int perDay = 1 + (d % 3);
        for (int i = 0; i < perDay; ++i) {
            const int uid = userIds[(d + i) % userIds.size()];
            const QString code = chargerCodes[(d + i) % chargerCodes.size()];
            const double energy = 8.0 + (d % 5) * 3.0;
            const double price = ((d + i) % 2 == 0) ? 1.80 : 0.90;
            const double amount = energy * price;
            const QString startT = day.addSecs(i * 3600 + 600).toString(fmt);
            const QString endT   = day.addSecs(i * 3600 + 3600 + 600).toString(fmt);

            QSqlQuery o(db);
            o.prepare("INSERT OR IGNORE INTO orders"
                      " (order_no, user_id, charger_code, status,"
                      "  start_time, end_time, duration_minutes,"
                      "  energy_kwh, unit_price, amount, created_at)"
                      " VALUES (?, ?, ?, 'paid', ?, ?, ?, ?, ?, ?, ?)");
            const QString orderNo = QString("C%1%2")
                .arg(day.toString("yyyyMMdd"))
                .arg(orderSeq++, 4, 10, QChar('0'));
            o.addBindValue(orderNo);
            o.addBindValue(uid);
            o.addBindValue(code);
            o.addBindValue(startT);
            o.addBindValue(endT);
            o.addBindValue(60);
            o.addBindValue(energy);
            o.addBindValue(price);
            o.addBindValue(amount);
            o.addBindValue(startT);
            o.exec();
        }
    }

    qDebug() << "[ServerDb] 演示数据播种完成";
    return true;
}

// ---------------------------------------------------------------------------
// 管理员
// ---------------------------------------------------------------------------

bool ServerDb::adminLogin(const QString &username, const QString &password)
{
    QSqlQuery q(database());
    q.prepare("SELECT id FROM admins WHERE username = ? AND password = ?");
    q.addBindValue(username);
    q.addBindValue(password);
    if (!q.exec()) {
        qWarning() << "[ServerDb] adminLogin 失败:" << q.lastError().text();
        return false;
    }
    return q.next();
}

// ---------------------------------------------------------------------------
// 用户
// ---------------------------------------------------------------------------

int ServerDb::registerUser(const QString &phone, const QString &pwdForStorage,
                           const QString &nickname)
{
    if (phone.isEmpty() || nickname.isEmpty()) {
        return -2;
    }
    // 手机号唯一检查
    QSqlQuery c(database());
    c.prepare("SELECT id FROM users WHERE phone = ?");
    c.addBindValue(phone);
    if (!c.exec()) {
        return -2;
    }
    if (c.next()) {
        return -1;   // 已存在
    }

    QSqlQuery q(database());
    q.prepare("INSERT INTO users (phone, password, nickname)"
              " VALUES (?, ?, ?)");
    q.addBindValue(phone);
    q.addBindValue(pwdForStorage);
    q.addBindValue(nickname);
    if (!q.exec()) {
        qWarning() << "[ServerDb] registerUser 失败:" << q.lastError().text();
        return -2;
    }
    return q.lastInsertId().toInt();
}

bool ServerDb::userExistsByPhone(const QString &phone)
{
    QSqlQuery q(database());
    q.prepare("SELECT id FROM users WHERE phone = ?");
    q.addBindValue(phone);
    return q.exec() && q.next();
}

bool ServerDb::getUserByPhone(const QString &phone, UserInfo &out)
{
    QSqlQuery q(database());
    q.prepare("SELECT id, phone, password, nickname, avatar_path, balance,"
              " status, created_at FROM users WHERE phone = ?");
    q.addBindValue(phone);
    if (!q.exec() || !q.next()) {
        return false;
    }
    out.id         = q.value("id").toInt();
    out.phone      = q.value("phone").toString();
    out.password   = q.value("password").toString();
    out.nickname   = q.value("nickname").toString();
    out.avatarPath = q.value("avatar_path").toString();
    out.balance    = q.value("balance").toDouble();
    out.status     = q.value("status").toString();
    out.createdAt  = q.value("created_at").toString();
    return true;
}

bool ServerDb::getUserById(int userId, UserInfo &out)
{
    QSqlQuery q(database());
    q.prepare("SELECT id, phone, password, nickname, avatar_path, balance,"
              " status, created_at FROM users WHERE id = ?");
    q.addBindValue(userId);
    if (!q.exec() || !q.next()) {
        return false;
    }
    out.id         = q.value("id").toInt();
    out.phone      = q.value("phone").toString();
    out.password   = q.value("password").toString();
    out.nickname   = q.value("nickname").toString();
    out.avatarPath = q.value("avatar_path").toString();
    out.balance    = q.value("balance").toDouble();
    out.status     = q.value("status").toString();
    out.createdAt  = q.value("created_at").toString();
    return true;
}

bool ServerDb::updateNickname(int userId, const QString &nickname)
{
    if (nickname.trimmed().isEmpty()) {
        return false;
    }
    QSqlQuery q(database());
    q.prepare("UPDATE users SET nickname = ? WHERE id = ?");
    q.addBindValue(nickname);
    q.addBindValue(userId);
    return q.exec() && q.numRowsAffected() == 1;
}

bool ServerDb::updateAvatar(int userId, const QString &avatarPath)
{
    if (avatarPath.isEmpty()) {
        return false;
    }
    QSqlQuery q(database());
    q.prepare("UPDATE users SET avatar_path = ? WHERE id = ?");
    q.addBindValue(avatarPath);
    q.addBindValue(userId);
    return q.exec() && q.numRowsAffected() == 1;
}

bool ServerDb::recharge(int userId, double amount, double &newBalance)
{
    if (amount <= 0) {
        return false;
    }
    QSqlDatabase db = database();

    double oldBalance = 0.0;
    {
        QSqlQuery q(db);
        q.prepare("SELECT balance FROM users WHERE id = ?");
        q.addBindValue(userId);
        if (!q.exec() || !q.next()) {
            return false;
        }
        oldBalance = q.value(0).toDouble();
    }
    newBalance = oldBalance + amount;

    if (!db.transaction()) {
        return false;
    }

    QSqlQuery u(db);
    u.prepare("UPDATE users SET balance = ? WHERE id = ?");
    u.addBindValue(newBalance);
    u.addBindValue(userId);
    if (!u.exec() || u.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    QSqlQuery w(db);
    w.prepare("INSERT INTO wallet_records (user_id, type, amount, balance_after, order_no)"
              " VALUES (?, 'recharge', ?, ?, NULL)");
    w.addBindValue(userId);
    w.addBindValue(amount);
    w.addBindValue(newBalance);
    if (!w.exec()) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool ServerDb::changePassword(int userId, const QString &newPwdForStorage)
{
    QSqlQuery q(database());
    q.prepare("UPDATE users SET password = ? WHERE id = ?");
    q.addBindValue(newPwdForStorage);
    q.addBindValue(userId);
    return q.exec() && q.numRowsAffected() == 1;
}

bool ServerDb::resetPasswordByPhone(const QString &phone, const QString &newPwdForStorage)
{
    QSqlQuery q(database());
    q.prepare("UPDATE users SET password = ? WHERE phone = ?");
    q.addBindValue(newPwdForStorage);
    q.addBindValue(phone);
    return q.exec() && q.numRowsAffected() == 1;
}

QList<UserInfo> ServerDb::searchUsers(const QString &keyword, int page, int pageSize, int &total)
{
    QList<UserInfo> list;
    QSqlDatabase db = database();

    const QString like = "%" + keyword.trimmed() + "%";
    const bool hasKeyword = !keyword.trimmed().isEmpty();

    {
        QSqlQuery c(db);
        if (hasKeyword) {
            c.prepare("SELECT COUNT(*) FROM users WHERE phone LIKE ? OR nickname LIKE ?");
            c.addBindValue(like);
            c.addBindValue(like);
        } else {
            c.prepare("SELECT COUNT(*) FROM users");
        }
        if (c.exec() && c.next()) {
            total = c.value(0).toInt();
        }
    }

    QSqlQuery q(db);
    QString sql = "SELECT id, phone, nickname, avatar_path, balance, status, created_at"
                  " FROM users";
    if (hasKeyword) {
        sql += " WHERE phone LIKE ? OR nickname LIKE ?";
    }
    sql += " ORDER BY id ASC LIMIT ? OFFSET ?";
    q.prepare(sql);
    if (hasKeyword) {
        q.addBindValue(like);
        q.addBindValue(like);
    }
    q.addBindValue(pageSize);
    q.addBindValue(page * pageSize);

    if (!q.exec()) {
        return list;
    }
    while (q.next()) {
        UserInfo u;
        u.id         = q.value("id").toInt();
        u.phone      = q.value("phone").toString();
        u.nickname   = q.value("nickname").toString();
        u.avatarPath = q.value("avatar_path").toString();
        u.balance    = q.value("balance").toDouble();
        u.status     = q.value("status").toString();
        u.createdAt  = q.value("created_at").toString();
        list.append(u);
    }
    return list;
}

// ---------------------------------------------------------------------------
// 钱包流水
// ---------------------------------------------------------------------------

QList<WalletRecord> ServerDb::getWalletRecords(int userId, const QString &from,
                                               const QString &to, int page,
                                               int pageSize, int &total)
{
    QList<WalletRecord> list;
    QSqlDatabase db = database();

    QString where = " user_id = ?";
    QStringList binds;
    binds << QString::number(userId);
    if (!from.isEmpty()) {
        where += " AND created_at >= ?";
        binds << from;
    }
    if (!to.isEmpty()) {
        where += " AND created_at <= ?";
        binds << to;
    }

    {
        QSqlQuery c(db);
        c.prepare("SELECT COUNT(*) FROM wallet_records WHERE" + where);
        for (const QString &b : binds) c.addBindValue(b);
        if (c.exec() && c.next()) total = c.value(0).toInt();
    }

    QSqlQuery q(db);
    q.prepare("SELECT id, user_id, type, amount, balance_after, order_no, created_at"
              " FROM wallet_records WHERE" + where +
              " ORDER BY created_at DESC LIMIT ? OFFSET ?");
    for (const QString &b : binds) q.addBindValue(b);
    q.addBindValue(pageSize);
    q.addBindValue(page * pageSize);
    if (!q.exec()) {
        return list;
    }
    while (q.next()) {
        WalletRecord r;
        r.id           = q.value("id").toInt();
        r.userId       = q.value("user_id").toInt();
        r.type         = q.value("type").toString();
        r.amount       = q.value("amount").toDouble();
        r.balanceAfter = q.value("balance_after").toDouble();
        r.orderNo      = q.value("order_no").toString();
        r.createdAt    = q.value("created_at").toString();
        list.append(r);
    }
    return list;
}

// ---------------------------------------------------------------------------
// 充电站
// ---------------------------------------------------------------------------

QList<StationInfo> ServerDb::getAllStations()
{
    QList<StationInfo> list;
    QSqlQuery q(database());
    q.exec("SELECT id, name, address, longitude, latitude, created_at"
           " FROM stations ORDER BY id");
    while (q.next()) {
        StationInfo s;
        s.id        = q.value("id").toInt();
        s.name      = q.value("name").toString();
        s.address   = q.value("address").toString();
        s.longitude = q.value("longitude").toDouble();
        s.latitude  = q.value("latitude").toDouble();
        s.createdAt = q.value("created_at").toString();
        list.append(s);
    }
    return list;
}

QList<StationBrief> ServerDb::getStationBriefs()
{
    QList<StationBrief> list;
    QSqlQuery q(database());
    q.exec("SELECT s.id, s.name, s.address, s.longitude, s.latitude,"
           "       COUNT(c.charger_code) AS total_piles,"
           "       SUM(CASE WHEN c.status='idle' THEN 1 ELSE 0 END) AS free_piles,"
           "       SUM(CASE WHEN c.status!='offline' THEN 1 ELSE 0 END) AS online_piles"
           " FROM stations s LEFT JOIN chargers c ON c.station_id = s.id"
           " GROUP BY s.id ORDER BY s.id");
    while (q.next()) {
        StationBrief b;
        b.id         = q.value("id").toInt();
        b.name       = q.value("name").toString();
        b.address    = q.value("address").toString();
        b.longitude  = q.value("longitude").toDouble();
        b.latitude   = q.value("latitude").toDouble();
        b.totalPiles = q.value("total_piles").toInt();
        b.freePiles  = q.value("free_piles").toInt();
        b.onlinePiles= q.value("online_piles").toInt();
        list.append(b);
    }
    return list;
}

QList<StationBrief> ServerDb::getNearbyStations(double lat, double lon,
                                                double radiusM, int limit, int offset,
                                                int &total)
{
    QList<StationBrief> all = getStationBriefs();
    QList<StationBrief> filtered;

    for (StationBrief &b : all) {
        const double d = haversineM(lat, lon, b.latitude, b.longitude);
        b.distanceM = d;
        if (radiusM <= 0 || d <= radiusM) {
            filtered.append(b);
        }
    }

    // 按距离由近及远
    std::sort(filtered.begin(), filtered.end(),
              [](const StationBrief &a, const StationBrief &b) {
                  return a.distanceM < b.distanceM;
              });

    total = filtered.size();

    QList<StationBrief> page;
    for (int i = offset; i < filtered.size() && i < offset + limit; ++i) {
        page.append(filtered.at(i));
    }
    return page;
}

StationInfo ServerDb::getStationById(int stationId)
{
    StationInfo s;
    QSqlQuery q(database());
    q.prepare("SELECT id, name, address, longitude, latitude, created_at"
              " FROM stations WHERE id = ?");
    q.addBindValue(stationId);
    if (q.exec() && q.next()) {
        s.id        = q.value("id").toInt();
        s.name      = q.value("name").toString();
        s.address   = q.value("address").toString();
        s.longitude = q.value("longitude").toDouble();
        s.latitude  = q.value("latitude").toDouble();
        s.createdAt = q.value("created_at").toString();
    }
    return s;
}

// ---------------------------------------------------------------------------
// 充电桩
// ---------------------------------------------------------------------------

QList<ChargerInfo> ServerDb::getChargersByStation(int stationId)
{
    QList<ChargerInfo> list;
    QSqlQuery q(database());
    q.prepare("SELECT charger_code, station_id, type, power_kw, status,"
              " charge_count, total_duration FROM chargers"
              " WHERE station_id = ? ORDER BY charger_code");
    q.addBindValue(stationId);
    if (!q.exec()) {
        return list;
    }
    while (q.next()) {
        ChargerInfo c;
        c.chargerCode  = q.value("charger_code").toString();
        c.stationId    = q.value("station_id").toInt();
        c.type         = q.value("type").toString();
        c.powerKw      = q.value("power_kw").toDouble();
        c.status       = q.value("status").toString();
        c.chargeCount  = q.value("charge_count").toInt();
        c.totalDuration= q.value("total_duration").toInt();
        list.append(c);
    }
    return list;
}

ChargerInfo ServerDb::getChargerByCode(const QString &chargerCode, bool &found)
{
    ChargerInfo c;
    found = false;
    QSqlQuery q(database());
    q.prepare("SELECT charger_code, station_id, type, power_kw, status,"
              " charge_count, total_duration FROM chargers WHERE charger_code = ?");
    q.addBindValue(chargerCode);
    if (q.exec() && q.next()) {
        found = true;
        c.chargerCode  = q.value("charger_code").toString();
        c.stationId    = q.value("station_id").toInt();
        c.type         = q.value("type").toString();
        c.powerKw      = q.value("power_kw").toDouble();
        c.status       = q.value("status").toString();
        c.chargeCount  = q.value("charge_count").toInt();
        c.totalDuration= q.value("total_duration").toInt();
    }
    return c;
}

QList<StatusCount> ServerDb::getChargerStatusDistribution()
{
    QList<StatusCount> list;
    QSqlQuery q(database());
    q.exec("SELECT status, COUNT(*) FROM chargers GROUP BY status");
    while (q.next()) {
        StatusCount sc;
        sc.status = q.value(0).toString();
        sc.count  = q.value(1).toInt();
        list.append(sc);
    }
    return list;
}

QList<ChargerInfo> ServerDb::listChargers(int page, int pageSize, int &total,
                                          const QString &stationFilter)
{
    QList<ChargerInfo> list;
    QSqlDatabase db = database();

    {
        QSqlQuery c(db);
        if (!stationFilter.isEmpty()) {
            c.prepare("SELECT COUNT(*) FROM chargers WHERE station_id = ?");
            c.addBindValue(stationFilter.toInt());
        } else {
            c.exec("SELECT COUNT(*) FROM chargers");
        }
        if (c.next()) {
            total = c.value(0).toInt();
        }
    }

    QSqlQuery q(db);
    QString sql = "SELECT charger_code, station_id, type, power_kw, status,"
                  " charge_count, total_duration FROM chargers";
    if (!stationFilter.isEmpty()) {
        sql += " WHERE station_id = ?";
    }
    sql += " ORDER BY charger_code LIMIT ? OFFSET ?";
    q.prepare(sql);
    if (!stationFilter.isEmpty()) {
        q.addBindValue(stationFilter.toInt());
    }
    q.addBindValue(pageSize);
    q.addBindValue(page * pageSize);
    if (!q.exec()) {
        return list;
    }
    while (q.next()) {
        ChargerInfo c;
        c.chargerCode  = q.value("charger_code").toString();
        c.stationId    = q.value("station_id").toInt();
        c.type         = q.value("type").toString();
        c.powerKw      = q.value("power_kw").toDouble();
        c.status       = q.value("status").toString();
        c.chargeCount  = q.value("charge_count").toInt();
        c.totalDuration= q.value("total_duration").toInt();
        list.append(c);
    }
    return list;
}

// ---------------------------------------------------------------------------
// 订单
// ---------------------------------------------------------------------------

bool ServerDb::createOrder(const QString &orderNo, int userId,
                           const QString &chargerCode, double unitPrice)
{
    QSqlDatabase db = database();

    // 1. 桩必须存在且 idle
    {
        QSqlQuery q(db);
        q.prepare("SELECT status FROM chargers WHERE charger_code = ?");
        q.addBindValue(chargerCode);
        if (!q.exec() || !q.next()) {
            return false;
        }
        if (q.value("status").toString() != "idle") {
            return false;
        }
    }

    // 2. 用户不能有未完成订单
    {
        QSqlQuery q(db);
        q.prepare("SELECT order_no FROM orders WHERE user_id = ?"
                  " AND status IN ('charging','unpaid') LIMIT 1");
        q.addBindValue(userId);
        if (q.exec() && q.next()) {
            return false;
        }
    }

    if (!db.transaction()) {
        return false;
    }

    QSqlQuery o(db);
    o.prepare("INSERT INTO orders (order_no, user_id, charger_code, status,"
              " start_time, unit_price)"
              " VALUES (?, ?, ?, 'charging', CURRENT_TIMESTAMP, ?)");
    o.addBindValue(orderNo);
    o.addBindValue(userId);
    o.addBindValue(chargerCode);
    o.addBindValue(unitPrice);
    if (!o.exec()) {
        db.rollback();
        return false;
    }

    QSqlQuery c(db);
    c.prepare("UPDATE chargers SET status='charging'"
              " WHERE charger_code = ? AND status='idle'");
    c.addBindValue(chargerCode);
    if (!c.exec() || c.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool ServerDb::finishOrder(const QString &orderNo, double energyKwh,
                           double &amount, QString &endTime, int &durationMinutes)
{
    if (energyKwh < 0) {
        return false;
    }
    QSqlDatabase db = database();

    QDateTime startTime;
    double unitPrice = 0.0;
    QString chargerCode;
    {
        QSqlQuery q(db);
        q.prepare("SELECT charger_code, status, start_time, unit_price"
                  " FROM orders WHERE order_no = ?");
        q.addBindValue(orderNo);
        if (!q.exec() || !q.next()) {
            return false;
        }
        if (q.value("status").toString() != "charging") {
            return false;
        }
        chargerCode = q.value("charger_code").toString();
        startTime   = q.value("start_time").toDateTime();
        unitPrice   = q.value("unit_price").toDouble();
    }

    const QDateTime now = QDateTime::currentDateTime();
    qint64 secs = startTime.secsTo(now);
    durationMinutes = static_cast<int>(secs / 60);
    if (durationMinutes < 0) {
        durationMinutes = 0;
    }
    amount = energyKwh * unitPrice;
    endTime = now.toString("yyyy-MM-dd HH:mm:ss");

    if (!db.transaction()) {
        return false;
    }

    QSqlQuery o(db);
    o.prepare("UPDATE orders SET status='unpaid', end_time=?,"
              " duration_minutes=?, energy_kwh=?, amount=?"
              " WHERE order_no = ? AND status='charging'");
    o.addBindValue(endTime);
    o.addBindValue(durationMinutes);
    o.addBindValue(energyKwh);
    o.addBindValue(amount);
    o.addBindValue(orderNo);
    if (!o.exec() || o.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    QSqlQuery c(db);
    c.prepare("UPDATE chargers SET status='idle', charge_count=charge_count+1,"
              " total_duration=total_duration+?"
              " WHERE charger_code = ? AND status='charging'");
    c.addBindValue(durationMinutes);
    c.addBindValue(chargerCode);
    if (!c.exec() || c.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool ServerDb::payOrder(const QString &orderNo, double &newBalance)
{
    QSqlDatabase db = database();

    int userId = -1;
    double amount = 0.0;
    {
        QSqlQuery q(db);
        q.prepare("SELECT user_id, status, amount FROM orders WHERE order_no = ?");
        q.addBindValue(orderNo);
        if (!q.exec() || !q.next()) {
            return false;
        }
        if (q.value("status").toString() != "unpaid") {
            return false;
        }
        userId = q.value("user_id").toInt();
        amount = q.value("amount").toDouble();
    }

    double balance = 0.0;
    {
        QSqlQuery q(db);
        q.prepare("SELECT balance FROM users WHERE id = ?");
        q.addBindValue(userId);
        if (!q.exec() || !q.next()) {
            return false;
        }
        balance = q.value("balance").toDouble();
    }
    if (balance < amount) {
        return false;
    }
    newBalance = balance - amount;

    if (!db.transaction()) {
        return false;
    }

    QSqlQuery u(db);
    u.prepare("UPDATE users SET balance = ? WHERE id = ?");
    u.addBindValue(newBalance);
    u.addBindValue(userId);
    if (!u.exec() || u.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    QSqlQuery o(db);
    o.prepare("UPDATE orders SET status='paid' WHERE order_no = ? AND status='unpaid'");
    o.addBindValue(orderNo);
    if (!o.exec() || o.numRowsAffected() != 1) {
        db.rollback();
        return false;
    }

    QSqlQuery w(db);
    w.prepare("INSERT INTO wallet_records (user_id, type, amount, balance_after, order_no)"
              " VALUES (?, 'charge', ?, ?, ?)");
    w.addBindValue(userId);
    w.addBindValue(-amount);
    w.addBindValue(newBalance);
    w.addBindValue(orderNo);
    if (!w.exec()) {
        db.rollback();
        return false;
    }

    return db.commit();
}

OrderInfo ServerDb::getOrderByNo(const QString &orderNo, bool &found)
{
    OrderInfo o;
    found = false;
    QSqlQuery q(database());
    q.prepare("SELECT order_no, user_id, charger_code, status, start_time, end_time,"
              " duration_minutes, energy_kwh, unit_price, amount, created_at"
              " FROM orders WHERE order_no = ?");
    q.addBindValue(orderNo);
    if (q.exec() && q.next()) {
        found = true;
        o.orderNo         = q.value("order_no").toString();
        o.userId          = q.value("user_id").toInt();
        o.chargerCode     = q.value("charger_code").toString();
        o.status          = q.value("status").toString();
        o.startTime       = q.value("start_time").toString();
        o.endTime         = q.value("end_time").toString();
        o.durationMinutes = q.value("duration_minutes").toInt();
        o.energyKwh       = q.value("energy_kwh").toDouble();
        o.unitPrice       = q.value("unit_price").toDouble();
        o.amount          = q.value("amount").toDouble();
        o.createdAt       = q.value("created_at").toString();
    }
    return o;
}

OrderInfo ServerDb::getUnfinishedOrder(int userId, bool &found)
{
    OrderInfo o;
    found = false;
    QSqlQuery q(database());
    q.prepare("SELECT order_no, user_id, charger_code, status, start_time, end_time,"
              " duration_minutes, energy_kwh, unit_price, amount, created_at"
              " FROM orders WHERE user_id = ? AND status IN ('charging','unpaid')"
              " ORDER BY start_time DESC LIMIT 1");
    q.addBindValue(userId);
    if (q.exec() && q.next()) {
        found = true;
        o.orderNo         = q.value("order_no").toString();
        o.userId          = q.value("user_id").toInt();
        o.chargerCode     = q.value("charger_code").toString();
        o.status          = q.value("status").toString();
        o.startTime       = q.value("start_time").toString();
        o.endTime         = q.value("end_time").toString();
        o.durationMinutes = q.value("duration_minutes").toInt();
        o.energyKwh       = q.value("energy_kwh").toDouble();
        o.unitPrice       = q.value("unit_price").toDouble();
        o.amount          = q.value("amount").toDouble();
        o.createdAt       = q.value("created_at").toString();
    }
    return o;
}

QList<OrderInfo> ServerDb::listOrders(int page, int pageSize, int &total,
                                      const QString &statusFilter,
                                      const QString &phoneFilter)
{
    QList<OrderInfo> list;
    QSqlDatabase db = database();

    QString where;
    QStringList binds;
    if (!statusFilter.isEmpty()) {
        where += " o.status = ?";
        binds << statusFilter;
    }
    if (!phoneFilter.isEmpty()) {
        if (!where.isEmpty()) where += " AND";
        where += " u.phone LIKE ?";
        binds << "%" + phoneFilter.trimmed() + "%";
    }
    const QString whereClause = where.isEmpty() ? QString() : (" WHERE" + where);

    {
        QSqlQuery c(db);
        c.prepare("SELECT COUNT(*) FROM orders o JOIN users u ON u.id = o.user_id"
                  + whereClause);
        for (const QString &b : binds) c.addBindValue(b);
        if (c.exec() && c.next()) total = c.value(0).toInt();
    }

    QSqlQuery q(db);
    q.prepare("SELECT o.order_no, o.user_id, o.charger_code, o.status, o.start_time,"
              " o.end_time, o.duration_minutes, o.energy_kwh, o.unit_price, o.amount,"
              " o.created_at, u.phone"
              " FROM orders o JOIN users u ON u.id = o.user_id"
              + whereClause + " ORDER BY o.created_at DESC LIMIT ? OFFSET ?");
    for (const QString &b : binds) q.addBindValue(b);
    q.addBindValue(pageSize);
    q.addBindValue(page * pageSize);
    if (!q.exec()) {
        return list;
    }
    while (q.next()) {
        OrderInfo o;
        o.orderNo         = q.value("order_no").toString();
        o.userId          = q.value("user_id").toInt();
        o.chargerCode     = q.value("charger_code").toString();
        o.status          = q.value("status").toString();
        o.startTime       = q.value("start_time").toString();
        o.endTime         = q.value("end_time").toString();
        o.durationMinutes = q.value("duration_minutes").toInt();
        o.energyKwh       = q.value("energy_kwh").toDouble();
        o.unitPrice       = q.value("unit_price").toDouble();
        o.amount          = q.value("amount").toDouble();
        o.createdAt       = q.value("created_at").toString();
        o.userPhone       = q.value("phone").toString();
        list.append(o);
    }
    return list;
}

// ---------------------------------------------------------------------------
// 营收统计
// ---------------------------------------------------------------------------

RevenueSummary ServerDb::getRevenueSummary()
{
    RevenueSummary r;
    QSqlQuery q(database());
    // 只统计已支付订单
    q.exec("SELECT"
           " SUM(CASE WHEN date(created_at)=date('now') THEN amount ELSE 0 END),"
           " SUM(CASE WHEN strftime('%Y-%m', created_at)=strftime('%Y-%m','now')"
           "      THEN amount ELSE 0 END),"
           " SUM(amount)"
           " FROM orders WHERE status='paid'");
    if (q.next()) {
        r.today = q.value(0).toDouble();
        r.month = q.value(1).toDouble();
        r.total = q.value(2).toDouble();
    }
    return r;
}

QList<QPair<QString, double>> ServerDb::getRevenueTrend(int days)
{
    QList<QPair<QString, double>> result;
    QSqlDatabase db = database();

    // 先查聚合结果
    QMap<QString, double> byDay;
    {
        QSqlQuery q(db);
        q.prepare("SELECT date(created_at) d, SUM(amount)"
                  " FROM orders WHERE status='paid'"
                  " AND created_at >= date('now', ?)"
                  " GROUP BY d");
        q.addBindValue(QString("-%1 day").arg(days));
        if (q.exec()) {
            while (q.next()) {
                byDay.insert(q.value(0).toString(), q.value(1).toDouble());
            }
        }
    }

    // 补全缺失日期为 0
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        const QString key = d.toString("yyyy-MM-dd");
        result.append(qMakePair(key, byDay.value(key, 0.0)));
    }
    return result;
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------

QString ServerDb::nextOrderNo()
{
    const QString date = QDate::currentDate().toString("yyyyMMdd");
    QSqlQuery q(database());
    q.prepare("SELECT COUNT(*) FROM orders WHERE order_no LIKE ?");
    q.addBindValue("C" + date + "%");
    int n = 0;
    if (q.exec() && q.next()) {
        n = q.value(0).toInt();
    }
    return QString("C%1%2").arg(date).arg(n + 1, 4, 10, QChar('0'));
}
