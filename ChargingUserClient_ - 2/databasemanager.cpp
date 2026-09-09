#include "databasemanager.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringList>
#include <QTime>
#include <QtGlobal>
#include <QUuid>
#include <QVariant>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent),
      m_connectionName(QStringLiteral("charging_client_%1").arg(reinterpret_cast<quintptr>(this)))
{
}

DatabaseManager::~DatabaseManager()
{
    const QString name = m_connectionName;
    if (m_db.isValid()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool DatabaseManager::open(QString *errorMessage)
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(baseDir);
    m_databasePath = QDir(baseDir).filePath(QStringLiteral("charging_user_client.db"));

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_databasePath);
    if (!m_db.open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("数据库打开失败：%1").arg(m_db.lastError().text());
        }
        return false;
    }

    if (!createTables(errorMessage)) {
        return false;
    }
    return seedDemoData(errorMessage);
}

QString DatabaseManager::databasePath() const
{
    return m_databasePath;
}

bool DatabaseManager::execSql(const QString &sql, QString *message)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        if (message) {
            *message = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool DatabaseManager::createTables(QString *message)
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS users ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "phone TEXT UNIQUE NOT NULL,"
                       "password TEXT NOT NULL,"
                       "nickname TEXT NOT NULL,"
                       "avatar_path TEXT,"
                       "balance REAL NOT NULL DEFAULT 0,"
                       "created_at TEXT NOT NULL,"
                       "status TEXT NOT NULL DEFAULT '正常')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS stations ("
                       "id TEXT PRIMARY KEY,"
                       "name TEXT NOT NULL,"
                       "region TEXT NOT NULL,"
                       "address TEXT NOT NULL,"
                       "latitude REAL NOT NULL,"
                       "longitude REAL NOT NULL,"
                       "price REAL NOT NULL,"
                       "distance REAL NOT NULL,"
                       "online_rate REAL NOT NULL,"
                       "recommend_score INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS piles ("
                       "id TEXT PRIMARY KEY,"
                       "station_id TEXT NOT NULL,"
                       "type TEXT NOT NULL,"
                       "power REAL NOT NULL,"
                       "status TEXT NOT NULL,"
                       "total_times INTEGER NOT NULL DEFAULT 0,"
                       "total_hours REAL NOT NULL DEFAULT 0,"
                       "FOREIGN KEY(station_id) REFERENCES stations(id))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS transactions ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "user_id INTEGER NOT NULL,"
                       "type TEXT NOT NULL,"
                       "amount REAL NOT NULL,"
                       "happened_at TEXT NOT NULL,"
                       "note TEXT,"
                       "FOREIGN KEY(user_id) REFERENCES users(id))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS orders ("
                       "id TEXT PRIMARY KEY,"
                       "user_id INTEGER NOT NULL,"
                       "pile_id TEXT NOT NULL,"
                       "start_at TEXT NOT NULL,"
                       "end_at TEXT,"
                       "kwh REAL NOT NULL DEFAULT 0,"
                       "cost REAL NOT NULL DEFAULT 0,"
                       "status TEXT NOT NULL,"
                       "FOREIGN KEY(user_id) REFERENCES users(id),"
                       "FOREIGN KEY(pile_id) REFERENCES piles(id))")
    };

    for (const QString &statement : statements) {
        if (!execSql(statement, message)) {
            return false;
        }
    }
    return true;
}

bool DatabaseManager::seedDemoData(QString *message)
{
    QSqlQuery countQuery(m_db);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM stations")) || !countQuery.next()) {
        if (message) {
            *message = countQuery.lastError().text();
        }
        return false;
    }
    if (countQuery.value(0).toInt() > 0) {
        return true;
    }

    if (!m_db.transaction()) {
        if (message) {
            *message = QStringLiteral("初始化演示数据失败：%1").arg(m_db.lastError().text());
        }
        return false;
    }

    struct DemoStation
    {
        const char *id;
        const char *name;
        const char *region;
        const char *address;
        double lat;
        double lng;
        double price;
        double distance;
        double onlineRate;
        int score;
    };

    const QVector<DemoStation> stationsData = {
        {"ST001", "南湖智慧快充站", "南湖区", "南湖区科技路88号", 30.747, 120.770, 1.28, 1.2, 0.96, 96},
        {"ST002", "万达广场地下充电站", "南湖区", "广益路万达广场B2层", 30.756, 120.804, 1.35, 2.6, 0.88, 86},
        {"ST003", "秀洲政务中心充电站", "秀洲区", "洪兴西路1765号", 30.764, 120.711, 1.18, 3.4, 0.92, 91},
        {"ST004", "高铁南站停车楼充电站", "经开区", "商务大道高铁南站停车楼", 30.681, 120.781, 1.42, 5.1, 0.81, 77},
        {"ST005", "月河慢充服务站", "南湖区", "环城北路月河历史街区", 30.775, 120.753, 1.08, 4.5, 0.73, 72}
    };

    QSqlQuery stationInsert(m_db);
    stationInsert.prepare(QStringLiteral("INSERT INTO stations "
                                         "(id,name,region,address,latitude,longitude,price,distance,online_rate,recommend_score) "
                                         "VALUES (?,?,?,?,?,?,?,?,?,?)"));
    for (const DemoStation &station : stationsData) {
        stationInsert.addBindValue(station.id);
        stationInsert.addBindValue(QString::fromUtf8(station.name));
        stationInsert.addBindValue(QString::fromUtf8(station.region));
        stationInsert.addBindValue(QString::fromUtf8(station.address));
        stationInsert.addBindValue(station.lat);
        stationInsert.addBindValue(station.lng);
        stationInsert.addBindValue(station.price);
        stationInsert.addBindValue(station.distance);
        stationInsert.addBindValue(station.onlineRate);
        stationInsert.addBindValue(station.score);
        if (!stationInsert.exec()) {
            if (message) {
                *message = stationInsert.lastError().text();
            }
            m_db.rollback();
            return false;
        }
    }

    struct DemoPile
    {
        const char *id;
        const char *stationId;
        const char *type;
        double power;
        const char *status;
        int totalTimes;
        double totalHours;
    };

    const QVector<DemoPile> pilesData = {
        {"NH-A01", "ST001", "快充", 120, "空闲", 238, 512.5},
        {"NH-A02", "ST001", "快充", 120, "空闲", 221, 498.2},
        {"NH-A03", "ST001", "慢充", 60, "在用", 145, 420.0},
        {"NH-A04", "ST001", "快充", 160, "空闲", 252, 550.8},
        {"WD-B01", "ST002", "快充", 100, "空闲", 310, 690.1},
        {"WD-B02", "ST002", "快充", 100, "在用", 287, 601.0},
        {"WD-B03", "ST002", "慢充", 45, "故障", 99, 230.4},
        {"XZ-C01", "ST003", "快充", 150, "空闲", 330, 720.2},
        {"XZ-C02", "ST003", "慢充", 60, "空闲", 204, 530.3},
        {"XZ-C03", "ST003", "快充", 150, "空闲", 318, 701.8},
        {"GT-D01", "ST004", "快充", 180, "在用", 410, 880.0},
        {"GT-D02", "ST004", "快充", 180, "空闲", 402, 859.6},
        {"YH-E01", "ST005", "慢充", 40, "空闲", 80, 350.5},
        {"YH-E02", "ST005", "慢充", 40, "故障", 73, 320.0}
    };

    QSqlQuery pileInsert(m_db);
    pileInsert.prepare(QStringLiteral("INSERT INTO piles "
                                      "(id,station_id,type,power,status,total_times,total_hours) "
                                      "VALUES (?,?,?,?,?,?,?)"));
    for (const DemoPile &pile : pilesData) {
        pileInsert.addBindValue(pile.id);
        pileInsert.addBindValue(pile.stationId);
        pileInsert.addBindValue(QString::fromUtf8(pile.type));
        pileInsert.addBindValue(pile.power);
        pileInsert.addBindValue(QString::fromUtf8(pile.status));
        pileInsert.addBindValue(pile.totalTimes);
        pileInsert.addBindValue(pile.totalHours);
        if (!pileInsert.exec()) {
            if (message) {
                *message = pileInsert.lastError().text();
            }
            m_db.rollback();
            return false;
        }
    }

    QSqlQuery userInsert(m_db);
    userInsert.prepare(QStringLiteral("INSERT OR IGNORE INTO users "
                                      "(phone,password,nickname,avatar_path,balance,created_at,status) "
                                      "VALUES (?,?,?,?,?,?,?)"));
    userInsert.addBindValue(QStringLiteral("13800138000"));
    userInsert.addBindValue(QStringLiteral("Demo@123"));
    userInsert.addBindValue(QStringLiteral("user_8000"));
    userInsert.addBindValue(QString());
    userInsert.addBindValue(200.0);
    userInsert.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    userInsert.addBindValue(QStringLiteral("正常"));
    if (!userInsert.exec()) {
        if (message) {
            *message = userInsert.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    QSqlQuery transactionInsert(m_db);
    transactionInsert.prepare(QStringLiteral("INSERT INTO transactions (user_id,type,amount,happened_at,note) "
                                             "SELECT id,?,?,?,? FROM users WHERE phone=?"));
    transactionInsert.addBindValue(QStringLiteral("充值"));
    transactionInsert.addBindValue(200.0);
    transactionInsert.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    transactionInsert.addBindValue(QStringLiteral("演示账户初始余额"));
    transactionInsert.addBindValue(QStringLiteral("13800138000"));
    if (!transactionInsert.exec()) {
        if (message) {
            *message = transactionInsert.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (message) {
            *message = QStringLiteral("初始化演示数据提交失败：%1").arg(m_db.lastError().text());
        }
        return false;
    }
    return true;
}

bool DatabaseManager::phoneExists(const QString &phone)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM users WHERE phone=?"));
    query.addBindValue(phone);
    return query.exec() && query.next() && query.value(0).toInt() > 0;
}

User DatabaseManager::readUser(const QSqlQuery &query) const
{
    const QSqlRecord record = query.record();
    User user;
    user.id = query.value(record.indexOf(QStringLiteral("id"))).toInt();
    user.phone = query.value(record.indexOf(QStringLiteral("phone"))).toString();
    user.password = query.value(record.indexOf(QStringLiteral("password"))).toString();
    user.nickname = query.value(record.indexOf(QStringLiteral("nickname"))).toString();
    user.avatarPath = query.value(record.indexOf(QStringLiteral("avatar_path"))).toString();
    user.balance = query.value(record.indexOf(QStringLiteral("balance"))).toDouble();
    user.createdAt = QDateTime::fromString(query.value(record.indexOf(QStringLiteral("created_at"))).toString(), Qt::ISODate);
    user.status = query.value(record.indexOf(QStringLiteral("status"))).toString();
    return user;
}

std::optional<User> DatabaseManager::userById(int userId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM users WHERE id=?"));
    query.addBindValue(userId);
    if (query.exec() && query.next()) {
        return readUser(query);
    }
    return std::nullopt;
}

std::optional<User> DatabaseManager::login(const QString &phone, const QString &password, QString *message)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM users WHERE phone=?"));
    query.addBindValue(phone);
    if (!query.exec()) {
        if (message) {
            *message = query.lastError().text();
        }
        return std::nullopt;
    }
    if (!query.next()) {
        if (message) {
            *message = QStringLiteral("手机号未注册，请先注册");
        }
        return std::nullopt;
    }

    const User user = readUser(query);
    if (user.status != QStringLiteral("正常")) {
        if (message) {
            *message = QStringLiteral("账号已被冻结，请联系管理员");
        }
        return std::nullopt;
    }
    if (user.password != password) {
        if (message) {
            *message = QStringLiteral("密码错误");
        }
        return std::nullopt;
    }
    return user;
}

std::optional<User> DatabaseManager::registerUser(const QString &phone,
                                                  const QString &password,
                                                  const QString &nickname,
                                                  QString *message)
{
    if (phoneExists(phone)) {
        if (message) {
            *message = QStringLiteral("手机号已绑定账号，请直接登录");
        }
        return std::nullopt;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO users "
                                 "(phone,password,nickname,avatar_path,balance,created_at,status) "
                                 "VALUES (?,?,?,?,?,?,?)"));
    query.addBindValue(phone);
    query.addBindValue(password);
    query.addBindValue(nickname);
    query.addBindValue(QString());
    query.addBindValue(0.0);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(QStringLiteral("正常"));
    if (!query.exec()) {
        if (message) {
            *message = query.lastError().text();
        }
        return std::nullopt;
    }
    return login(phone, password, message);
}

bool DatabaseManager::resetPasswordByPhone(const QString &phone, const QString &newPassword, QString *message)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE users SET password=? WHERE phone=?"));
    query.addBindValue(newPassword);
    query.addBindValue(phone);
    if (!query.exec()) {
        if (message) {
            *message = query.lastError().text();
        }
        return false;
    }
    if (query.numRowsAffected() == 0) {
        if (message) {
            *message = QStringLiteral("手机号未绑定账户，需要先注册");
        }
        return false;
    }
    return true;
}

bool DatabaseManager::changePassword(int userId, const QString &oldPassword, const QString &newPassword, QString *message)
{
    const std::optional<User> user = userById(userId);
    if (!user) {
        if (message) {
            *message = QStringLiteral("用户不存在");
        }
        return false;
    }
    if (user->password != oldPassword) {
        if (message) {
            *message = QStringLiteral("原密码错误");
        }
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE users SET password=? WHERE id=?"));
    query.addBindValue(newPassword);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (message) {
            *message = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool DatabaseManager::updateProfile(int userId, const QString &nickname, const QString &avatarPath, QString *message)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE users SET nickname=?, avatar_path=? WHERE id=?"));
    query.addBindValue(nickname);
    query.addBindValue(avatarPath);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (message) {
            *message = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool DatabaseManager::recharge(int userId, double amount, double *newBalance, QString *message)
{
    if (amount <= 0.0) {
        if (message) {
            *message = QStringLiteral("请输入有效的充值金额");
        }
        return false;
    }

    if (!m_db.transaction()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return false;
    }

    QSqlQuery update(m_db);
    update.prepare(QStringLiteral("UPDATE users SET balance=balance+? WHERE id=? AND status='正常'"));
    update.addBindValue(amount);
    update.addBindValue(userId);
    if (!update.exec() || update.numRowsAffected() == 0) {
        if (message) {
            *message = update.lastError().text().isEmpty() ? QStringLiteral("账号状态异常，无法充值") : update.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO transactions (user_id,type,amount,happened_at,note) VALUES (?,?,?,?,?)"));
    insert.addBindValue(userId);
    insert.addBindValue(QStringLiteral("充值"));
    insert.addBindValue(amount);
    insert.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    insert.addBindValue(QStringLiteral("模拟支付成功"));
    if (!insert.exec()) {
        if (message) {
            *message = insert.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return false;
    }

    const std::optional<User> user = userById(userId);
    if (user && newBalance) {
        *newBalance = user->balance;
    }
    return true;
}

QVector<Transaction> DatabaseManager::transactions(int userId, const QDate &from, const QDate &to)
{
    QVector<Transaction> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT type,amount,happened_at,note FROM transactions "
                                 "WHERE user_id=? AND happened_at>=? AND happened_at<=? "
                                 "ORDER BY happened_at DESC"));
    query.addBindValue(userId);
    query.addBindValue(QDateTime(from, QTime(0, 0, 0)).toString(Qt::ISODate));
    query.addBindValue(QDateTime(to, QTime(23, 59, 59)).toString(Qt::ISODate));
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        Transaction transaction;
        transaction.type = query.value(0).toString();
        transaction.amount = query.value(1).toDouble();
        transaction.happenedAt = QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
        transaction.note = query.value(3).toString();
        result.push_back(transaction);
    }
    return result;
}

Station DatabaseManager::readStation(const QSqlQuery &query) const
{
    const QSqlRecord record = query.record();
    Station station;
    station.id = query.value(record.indexOf(QStringLiteral("id"))).toString();
    station.name = query.value(record.indexOf(QStringLiteral("name"))).toString();
    station.region = query.value(record.indexOf(QStringLiteral("region"))).toString();
    station.address = query.value(record.indexOf(QStringLiteral("address"))).toString();
    station.latitude = query.value(record.indexOf(QStringLiteral("latitude"))).toDouble();
    station.longitude = query.value(record.indexOf(QStringLiteral("longitude"))).toDouble();
    station.price = query.value(record.indexOf(QStringLiteral("price"))).toDouble();
    station.distance = query.value(record.indexOf(QStringLiteral("distance"))).toDouble();
    station.onlineRate = query.value(record.indexOf(QStringLiteral("online_rate"))).toDouble();
    station.recommendScore = query.value(record.indexOf(QStringLiteral("recommend_score"))).toInt();
    station.totalPiles = query.value(record.indexOf(QStringLiteral("total_piles"))).toInt();
    station.idlePiles = query.value(record.indexOf(QStringLiteral("idle_piles"))).toInt();
    return station;
}

QVector<Station> DatabaseManager::stations(const QString &region, const QString &keyword)
{
    QVector<Station> result;
    QString sql = QStringLiteral("SELECT s.*, COUNT(p.id) AS total_piles, "
                                 "SUM(CASE WHEN p.status='空闲' THEN 1 ELSE 0 END) AS idle_piles "
                                 "FROM stations s LEFT JOIN piles p ON p.station_id=s.id WHERE 1=1");
    QVector<QVariant> binds;
    if (!region.isEmpty() && region != QStringLiteral("全部区域")) {
        sql += QStringLiteral(" AND s.region=?");
        binds.push_back(region);
    }
    if (!keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (s.name LIKE ? OR s.address LIKE ?)");
        const QString pattern = QStringLiteral("%") + keyword.trimmed() + QStringLiteral("%");
        binds.push_back(pattern);
        binds.push_back(pattern);
    }
    sql += QStringLiteral(" GROUP BY s.id ORDER BY recommend_score DESC, distance ASC");

    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant &bind : binds) {
        query.addBindValue(bind);
    }
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.push_back(readStation(query));
    }
    return result;
}

Pile DatabaseManager::readPile(const QSqlQuery &query) const
{
    const QSqlRecord record = query.record();
    Pile pile;
    pile.id = query.value(record.indexOf(QStringLiteral("id"))).toString();
    pile.stationId = query.value(record.indexOf(QStringLiteral("station_id"))).toString();
    pile.stationName = query.value(record.indexOf(QStringLiteral("station_name"))).toString();
    pile.type = query.value(record.indexOf(QStringLiteral("type"))).toString();
    pile.power = query.value(record.indexOf(QStringLiteral("power"))).toDouble();
    pile.status = query.value(record.indexOf(QStringLiteral("status"))).toString();
    pile.totalTimes = query.value(record.indexOf(QStringLiteral("total_times"))).toInt();
    pile.totalHours = query.value(record.indexOf(QStringLiteral("total_hours"))).toDouble();
    return pile;
}

QVector<Pile> DatabaseManager::pilesByStation(const QString &stationId)
{
    QVector<Pile> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT p.*, s.name AS station_name FROM piles p "
                                 "JOIN stations s ON s.id=p.station_id WHERE p.station_id=? "
                                 "ORDER BY p.id"));
    query.addBindValue(stationId);
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.push_back(readPile(query));
    }
    return result;
}

std::optional<Pile> DatabaseManager::pileById(const QString &pileId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT p.*, s.name AS station_name FROM piles p "
                                 "JOIN stations s ON s.id=p.station_id WHERE p.id=?"));
    query.addBindValue(pileId);
    if (query.exec() && query.next()) {
        return readPile(query);
    }
    return std::nullopt;
}

bool DatabaseManager::setPileStatus(const QString &pileId, const QString &status, QString *message)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE piles SET status=? WHERE id=?"));
    query.addBindValue(status);
    query.addBindValue(pileId);
    if (!query.exec()) {
        if (message) {
            *message = query.lastError().text();
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

Order DatabaseManager::readOrder(const QSqlQuery &query) const
{
    const QSqlRecord record = query.record();
    Order order;
    order.id = query.value(record.indexOf(QStringLiteral("id"))).toString();
    order.userId = query.value(record.indexOf(QStringLiteral("user_id"))).toInt();
    order.pileId = query.value(record.indexOf(QStringLiteral("pile_id"))).toString();
    order.startAt = QDateTime::fromString(query.value(record.indexOf(QStringLiteral("start_at"))).toString(), Qt::ISODate);
    order.endAt = QDateTime::fromString(query.value(record.indexOf(QStringLiteral("end_at"))).toString(), Qt::ISODate);
    order.kwh = query.value(record.indexOf(QStringLiteral("kwh"))).toDouble();
    order.cost = query.value(record.indexOf(QStringLiteral("cost"))).toDouble();
    order.status = query.value(record.indexOf(QStringLiteral("status"))).toString();
    return order;
}

std::optional<Order> DatabaseManager::unfinishedOrder(int userId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM orders WHERE user_id=? "
                                 "AND status IN ('充电中','待结算','异常待结算') "
                                 "ORDER BY start_at DESC LIMIT 1"));
    query.addBindValue(userId);
    if (query.exec() && query.next()) {
        return readOrder(query);
    }
    return std::nullopt;
}

QString DatabaseManager::createChargingOrder(int userId, const QString &pileId, QString *message)
{
    const std::optional<Pile> pile = pileById(pileId);
    if (!pile) {
        if (message) {
            *message = QStringLiteral("电桩不存在");
        }
        return QString();
    }
    if (pile->status != QStringLiteral("空闲")) {
        if (message) {
            *message = QStringLiteral("当前电桩不可用，请选择空闲电桩");
        }
        return QString();
    }
    if (unfinishedOrder(userId)) {
        if (message) {
            *message = QStringLiteral("您有未完成的充电订单，请先结算");
        }
        return QString();
    }

    if (!m_db.transaction()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return QString();
    }

    const QString orderId = QStringLiteral("ORD%1").arg(QDateTime::currentMSecsSinceEpoch());
    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO orders (id,user_id,pile_id,start_at,status) VALUES (?,?,?,?,?)"));
    insert.addBindValue(orderId);
    insert.addBindValue(userId);
    insert.addBindValue(pileId);
    insert.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    insert.addBindValue(QStringLiteral("充电中"));
    if (!insert.exec()) {
        if (message) {
            *message = insert.lastError().text();
        }
        m_db.rollback();
        return QString();
    }

    QSqlQuery update(m_db);
    update.prepare(QStringLiteral("UPDATE piles SET status='在用' WHERE id=?"));
    update.addBindValue(pileId);
    if (!update.exec()) {
        if (message) {
            *message = update.lastError().text();
        }
        m_db.rollback();
        return QString();
    }

    if (!m_db.commit()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return QString();
    }
    return orderId;
}

bool DatabaseManager::completeChargingOrder(const QString &orderId,
                                            double kwh,
                                            double cost,
                                            bool abnormal,
                                            QString *message)
{
    QSqlQuery find(m_db);
    find.prepare(QStringLiteral("SELECT * FROM orders WHERE id=?"));
    find.addBindValue(orderId);
    if (!find.exec() || !find.next()) {
        if (message) {
            *message = QStringLiteral("订单不存在");
        }
        return false;
    }
    const Order order = readOrder(find);
    const QString nextStatus = abnormal ? QStringLiteral("异常待结算") : QStringLiteral("待结算");
    const QString nextPileStatus = abnormal ? QStringLiteral("故障") : QStringLiteral("空闲");

    if (!m_db.transaction()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return false;
    }

    QSqlQuery updateOrder(m_db);
    updateOrder.prepare(QStringLiteral("UPDATE orders SET end_at=?, kwh=?, cost=?, status=? WHERE id=?"));
    updateOrder.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    updateOrder.addBindValue(kwh);
    updateOrder.addBindValue(cost);
    updateOrder.addBindValue(nextStatus);
    updateOrder.addBindValue(orderId);
    if (!updateOrder.exec()) {
        if (message) {
            *message = updateOrder.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    QSqlQuery updatePile(m_db);
    updatePile.prepare(QStringLiteral("UPDATE piles SET status=?, total_times=total_times+1, total_hours=total_hours+? WHERE id=?"));
    updatePile.addBindValue(nextPileStatus);
    updatePile.addBindValue(qMax(kwh / 60.0, 0.1));
    updatePile.addBindValue(order.pileId);
    if (!updatePile.exec()) {
        if (message) {
            *message = updatePile.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return false;
    }
    return true;
}

bool DatabaseManager::settleOrder(const QString &orderId, double *newBalance, QString *message)
{
    QSqlQuery find(m_db);
    find.prepare(QStringLiteral("SELECT * FROM orders WHERE id=?"));
    find.addBindValue(orderId);
    if (!find.exec() || !find.next()) {
        if (message) {
            *message = QStringLiteral("订单不存在");
        }
        return false;
    }
    const Order order = readOrder(find);
    if (order.status != QStringLiteral("待结算") && order.status != QStringLiteral("异常待结算")) {
        if (message) {
            *message = QStringLiteral("该订单当前不可结算");
        }
        return false;
    }

    const std::optional<User> user = userById(order.userId);
    if (!user || user->status != QStringLiteral("正常")) {
        if (message) {
            *message = QStringLiteral("账号状态异常，无法结算");
        }
        return false;
    }
    if (user->balance + 0.0001 < order.cost) {
        if (message) {
            *message = QStringLiteral("余额不足，请先充值");
        }
        return false;
    }

    if (!m_db.transaction()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return false;
    }

    QSqlQuery updateUser(m_db);
    updateUser.prepare(QStringLiteral("UPDATE users SET balance=balance-? WHERE id=?"));
    updateUser.addBindValue(order.cost);
    updateUser.addBindValue(order.userId);
    if (!updateUser.exec()) {
        if (message) {
            *message = updateUser.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    QSqlQuery updateOrder(m_db);
    updateOrder.prepare(QStringLiteral("UPDATE orders SET status='已结算' WHERE id=?"));
    updateOrder.addBindValue(orderId);
    if (!updateOrder.exec()) {
        if (message) {
            *message = updateOrder.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    QSqlQuery insertTrans(m_db);
    insertTrans.prepare(QStringLiteral("INSERT INTO transactions (user_id,type,amount,happened_at,note) VALUES (?,?,?,?,?)"));
    insertTrans.addBindValue(order.userId);
    insertTrans.addBindValue(QStringLiteral("充电扣费"));
    insertTrans.addBindValue(-order.cost);
    insertTrans.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    insertTrans.addBindValue(QStringLiteral("订单 %1，充电 %2 度").arg(order.id).arg(order.kwh, 0, 'f', 2));
    if (!insertTrans.exec()) {
        if (message) {
            *message = insertTrans.lastError().text();
        }
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (message) {
            *message = m_db.lastError().text();
        }
        return false;
    }

    const std::optional<User> refreshed = userById(order.userId);
    if (refreshed && newBalance) {
        *newBalance = refreshed->balance;
    }
    return true;
}

QVector<Order> DatabaseManager::orders(int userId)
{
    QVector<Order> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM orders WHERE user_id=? ORDER BY start_at DESC"));
    query.addBindValue(userId);
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.push_back(readOrder(query));
    }
    return result;
}
