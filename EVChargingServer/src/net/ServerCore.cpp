#include "net/servercore.h"

#include "net/clientsession.h"
#include "net/sessionmanager.h"
#include "common/appconfig.h"
#include "common/protocol.h"
#include "common/passwordutil.h"
#include "db/databasemanager.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QDebug>

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------

ServerCore::ServerCore(QObject *parent)
    : QObject(parent)
    , m_sessions(new SessionManager(this))
{
}

ServerCore::~ServerCore()
{
    stop();
}

bool ServerCore::start()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &ServerCore::onNewConnection);

    const quint16 port = AppConfig::instance().listenPort();
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit logMessage(QStringLiteral("监听端口 %1 失败: %2")
                            .arg(port).arg(m_server->errorString()));
        return false;
    }

    emit logMessage(QStringLiteral("服务器已启动, 监听端口 %1").arg(port));
    return true;
}

void ServerCore::stop()
{
    if (m_server) {
        m_server->close();
    }
    qDeleteAll(m_clients);
    m_clients.clear();
}

quint16 ServerCore::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

int ServerCore::connectionCount() const
{
    return m_clients.size();
}

// ---------------------------------------------------------------------------
// 连接管理
// ---------------------------------------------------------------------------

void ServerCore::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        ClientSession *session = new ClientSession(socket, this);
        m_clients.append(session);

        connect(session, &ClientSession::packetReceived,
                this, &ServerCore::onPacketReceived);
        connect(session, &ClientSession::disconnected,
                this, &ServerCore::onSessionDisconnected);

        emit logMessage(QStringLiteral("新连接: %1:%2, 当前在线 %3")
                            .arg(socket->peerAddress().toString())
                            .arg(socket->peerPort())
                            .arg(m_clients.size()));
        emit connectionCountChanged(m_clients.size());
    }
}

void ServerCore::onSessionDisconnected(ClientSession *session)
{
    m_clients.removeAll(session);
    emit logMessage(QStringLiteral("连接断开, 当前在线 %1").arg(m_clients.size()));
    emit connectionCountChanged(m_clients.size());
    session->deleteLater();
}

// ---------------------------------------------------------------------------
// 管理员登录
// ---------------------------------------------------------------------------

void ServerCore::requestAdminLogin(int requestId, const QString &username,
                                   const QString &password)
{
    const bool ok = DatabaseManager::instance().adminLogin(username, password);
    emit logMessage(QStringLiteral("管理员登录请求: %1 -> %2")
                        .arg(username).arg(ok ? "成功" : "失败"));
    emit adminLoginFinished(requestId, ok);
}

// ---------------------------------------------------------------------------
// 命令路由
// ---------------------------------------------------------------------------

void ServerCore::onPacketReceived(ClientSession *session, quint16 cmd,
                                  const QVariantMap &params, quint32 requestId)
{
    using namespace Protocol::Cmd;

    switch (cmd) {
    case SMS_CODE_REQ:        handleSmsCode(session, params, requestId); break;
    case REGISTER_REQ:        handleRegister(session, params, requestId); break;
    case LOGIN_REQ:           handleLogin(session, params, requestId); break;
    case LOGOUT_REQ:          handleLogout(session, params, requestId); break;
    case FIND_PWD_REQ:        handleFindPwd(session, params, requestId); break;
    case CHANGE_PWD_REQ:      handleChangePwd(session, params, requestId); break;
    case RECHARGE_REQ:        handleRecharge(session, params, requestId); break;
    case BALANCE_QUERY_REQ:   handleBalanceQuery(session, params, requestId); break;
    case TRANSACTION_REQ:     handleTransaction(session, params, requestId); break;
    case STATIONS_REQ:        handleStations(session, params, requestId); break;
    case STATION_DETAIL_REQ:  handleStationDetail(session, params, requestId); break;
    case CHECK_ORDER_REQ:     handleCheckOrder(session, params, requestId); break;
    case START_CHARGE_REQ:    handleStartCharge(session, params, requestId); break;
    case STOP_CHARGE_REQ:     handleStopCharge(session, params, requestId); break;
    case CHARGE_STATUS_REQ:   handleChargeStatus(session, params, requestId); break;
    default:
        replyError(session, requestId, Protocol::Err::UNKNOWN,
                   QStringLiteral("未知命令码 0x%1").arg(cmd, 4, 16, QChar('0')));
        break;
    }
}

void ServerCore::reply(ClientSession *s, quint16 cmd, const QVariantMap &p,
                       quint32 rid)
{
    s->send(cmd, p, rid);
}

void ServerCore::replyError(ClientSession *s, quint32 rid, int code,
                            const QString &msg)
{
    s->sendError(rid, code, msg);
}

// ---------------------------------------------------------------------------
// 短信验证码(模拟)
// ---------------------------------------------------------------------------

QString ServerCore::generateSmsCode(const QString &phone)
{
    QString code = AppConfig::instance().smsDevCode();
    if (code.isEmpty()) {
        code = QString::number(QRandomGenerator::global()->bounded(1000, 10000));
    }
    SmsEntry e;
    e.code   = code;
    e.expire = QDateTime::currentDateTime()
                   .addSecs(AppConfig::instance().smsValidSec());
    m_sms.insert(phone, e);

    // 系统不接入真实短信网关: 验证码打印到日志供联调
    emit logMessage(QStringLiteral("[模拟短信] 手机号 %1 验证码: %2")
                        .arg(phone).arg(code));
    return code;
}

bool ServerCore::verifySmsCode(const QString &phone, const QString &code)
{
    auto it = m_sms.constFind(phone);
    if (it == m_sms.constEnd()) {
        return false;
    }
    if (it->expire < QDateTime::currentDateTime()) {
        m_sms.remove(phone);
        return false;
    }
    return it->code == code.trimmed();
}

void ServerCore::handleSmsCode(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    const QString phone = p.value("phone").toString().trimmed();
    if (phone.isEmpty()) {
        replyError(s, rid, Protocol::Err::PARAM_MISSING, "缺少手机号");
        return;
    }
    const QString code = generateSmsCode(phone);

    QVariantMap r;
    r.insert("code", Protocol::Err::OK);
    r.insert("message", QStringLiteral("验证码已发送(模拟)"));
    // 联调模式: 把验证码一并返回, 方便客户端免看日志
    if (!AppConfig::instance().smsDevCode().isEmpty()) {
        r.insert("devCode", code);
    }
    reply(s, Protocol::Cmd::SMS_CODE_RESP, r, rid);
}

// ---------------------------------------------------------------------------
// 账户管理
// ---------------------------------------------------------------------------

void ServerCore::handleRegister(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;

    const QString phone    = p.value("phone").toString().trimmed();
    const QString password = p.value("password").toString();   // 客户端 SHA-256
    const QString nickname = p.value("nickname").toString().trimmed();
    const QString verify   = p.value("verifyCode").toString().trimmed();

    // 手机号格式: 11 位, 1 开头
    static const QRegularExpression phoneRe("^1[0-9]{10}$");
    if (!phoneRe.match(phone).hasMatch()) {
        replyError(s, rid, Err::PARAM_FORMAT, "手机号格式错误");
        return;
    }
    // 昵称格式(需求矩阵 #8): ^[A-Za-z_][A-Za-z0-9_]{2,14}$
    static const QRegularExpression nickRe("^[A-Za-z_][A-Za-z0-9_]{2,14}$");
    if (!nickRe.match(nickname).hasMatch()) {
        replyError(s, rid, Err::NICKNAME_ERR, "昵称格式错误");
        return;
    }
    if (!verify.isEmpty() && !verifySmsCode(phone, verify)) {
        replyError(s, rid, Err::VERIFY_CODE_ERR, "验证码错误");
        return;
    }
    if (password.isEmpty()) {
        replyError(s, rid, Err::PASSWORD_ERR, "密码不能为空");
        return;
    }

    // 密码入库加工(当前明文哈希, 预留加盐)
    const QString stored = PasswordUtil::hashForStorage(password);

    const int userId = DatabaseManager::instance().registerUser(phone, stored, nickname);
    if (userId == -1) {
        replyError(s, rid, Err::PHONE_EXISTS, "手机号已存在");
        return;
    }
    if (userId == -2) {
        replyError(s, rid, Err::DB_ERROR, "注册失败");
        return;
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("message", "注册成功");
    r.insert("userId", userId);
    r.insert("nickname", nickname);
    reply(s, Cmd::REGISTER_RESP, r, rid);
}

void ServerCore::handleLogin(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    DatabaseManager &db = DatabaseManager::instance();

    const QString phone    = p.value("phone").toString().trimmed();
    const QString password = p.value("password").toString();
    const QString smsCode  = p.value("smsCode").toString().trimmed();

    if (phone.isEmpty()) {
        replyError(s, rid, Err::PARAM_MISSING, "缺少手机号");
        return;
    }

    // 两种登录方式:
    //  1) 短信验证码登录(免密): 验证码正确即可; 手机号不存在则自动注册
    //  2) 密码登录: 校验密码
    bool smsLogin = !smsCode.isEmpty();

    User user;
    bool exists = db.getUserByPhone(phone, user);

    if (smsLogin) {
        if (!verifySmsCode(phone, smsCode)) {
            replyError(s, rid, Err::VERIFY_CODE_ERR, "验证码错误");
            return;
        }
        if (!exists) {
            // 自动注册(项目说明书 1.4: 手机号不存在则自动建号)。
            // 数据库 registerUser 要求密码非空, 此处用随机占位口令(不告知用户),
            // 用户后续通过短信登录或"找回密码"重设。
            const QString nick = "用户" + phone.right(4);
            const QString placeholderPwd =
                QUuid::createUuid().toString(QUuid::WithoutBraces);
            const int uid = db.registerUser(phone, placeholderPwd, nick);
            if (uid <= 0) {
                replyError(s, rid, Err::DB_ERROR, "自动注册失败");
                return;
            }
            db.getUserByPhone(phone, user);
        }
    } else {
        if (!exists) {
            replyError(s, rid, Err::PHONE_NOT_EXIST, "手机号不存在");
            return;
        }
        if (password.isEmpty()) {
            replyError(s, rid, Err::PARAM_MISSING, "缺少密码或验证码");
            return;
        }
        if (!PasswordUtil::verify(password, user.password)) {
            replyError(s, rid, Err::WRONG_PASSWORD, "密码错误");
            return;
        }
    }

    if (user.status == "frozen") {
        replyError(s, rid, Err::ACCOUNT_FROZEN, "账号已被冻结");
        return;
    }

    // 签发 token
    const QString token = m_sessions->createToken(user.id);
    s->setUserId(user.id);
    s->setToken(token);

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("message", "登录成功");
    r.insert("userId", user.id);
    r.insert("nickname", user.nickname);
    r.insert("balance", user.balance);
    r.insert("status", user.status);
    r.insert("token", token);
    reply(s, Cmd::LOGIN_RESP, r, rid);

    emit logMessage(QStringLiteral("用户登录: %1 (id=%2)")
                        .arg(phone).arg(user.id));
}

void ServerCore::handleLogout(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    const QString token = p.value("token").toString();
    m_sessions->revokeToken(token);
    s->setToken(QString());
    s->setUserId(-1);

    QVariantMap r;
    r.insert("code", Protocol::Err::OK);
    r.insert("message", "登出成功");
    reply(s, Protocol::Cmd::LOGOUT_RESP, r, rid);
}

void ServerCore::handleFindPwd(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const QString phone    = p.value("phone").toString().trimmed();
    const QString newPwd   = p.value("newPassword").toString();
    const QString verify   = p.value("verifyCode").toString().trimmed();

    if (!DatabaseManager::instance().userExistsByPhone(phone)) {
        replyError(s, rid, Err::PHONE_UNREGISTER, "手机号未注册");
        return;
    }
    if (!verifySmsCode(phone, verify)) {
        replyError(s, rid, Err::VERIFY_CODE_ERR, "验证码错误");
        return;
    }
    const QString stored = PasswordUtil::hashForStorage(newPwd);
    if (!DatabaseManager::instance().resetPasswordByPhone(phone, stored)) {
        replyError(s, rid, Err::DB_ERROR, "重置失败");
        return;
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("message", "密码重置成功");
    reply(s, Cmd::FIND_PWD_RESP, r, rid);
}

void ServerCore::handleChangePwd(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const QString oldPwd = p.value("oldPassword").toString();
    const QString newPwd = p.value("newPassword").toString();

    // 数据库端 changePassword 内部完成"原密码比对 + 写入新密码"。
    // 当前 PasswordMode=plain: 库里存的就是客户端 SHA-256 哈希, 直接比对即可;
    // 加盐哈希由 PasswordUtil 负责, 切换模式时此处逻辑保持不变。
    if (!DatabaseManager::instance().changePassword(userId, oldPwd, newPwd)) {
        replyError(s, rid, Err::OLD_PWD_ERR, "原密码错误或修改失败");
        return;
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("message", "密码修改成功");
    reply(s, Cmd::CHANGE_PWD_RESP, r, rid);
}

void ServerCore::handleRecharge(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const double amount = p.value("amount").toDouble();
    if (amount <= 0) {
        replyError(s, rid, Err::AMOUNT_INVALID, "充值金额无效");
        return;
    }

    if (!DatabaseManager::instance().recharge(userId, amount)) {
        replyError(s, rid, Err::DB_ERROR, "充值失败");
        return;
    }
    User recharged;
    DatabaseManager::instance().getUserInfo(userId, recharged);
    const double newBalance = recharged.balance;

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("message", QStringLiteral("充值成功，已到账%1元").arg(amount, 0, 'f', 2));
    r.insert("newBalance", newBalance);
    reply(s, Cmd::RECHARGE_RESP, r, rid);
}

void ServerCore::handleBalanceQuery(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    User user;
    if (!DatabaseManager::instance().getUserInfo(userId, user)) {
        replyError(s, rid, Err::DB_ERROR, "查询失败");
        return;
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("balance", user.balance);
    reply(s, Cmd::BALANCE_QUERY_RESP, r, rid);
}

void ServerCore::handleTransaction(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const QString from = p.value("startDate").toString();
    const QString to   = p.value("endDate").toString();
    const int page     = p.value("page", 1).toInt() - 1;      // 协议从 1 起
    const int pageSize = p.value("pageSize", 20).toInt();
    if (page < 0) {
        replyError(s, rid, Err::PARAM_FORMAT, "分页参数错误");
        return;
    }

    int total = 0;
    const QList<WalletRecord> records =
        DatabaseManager::instance().getWalletRecords(userId, from, to,
                                              page, pageSize, total);

    QVariantList list;
    for (const WalletRecord &rec : records) {
        QVariantMap item;
        item.insert("type", rec.type == "recharge" ? "充值" : "充电扣费");
        item.insert("amount", rec.amount);
        item.insert("time", rec.createdAt);
        list.append(item);
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("records", list);
    r.insert("total", total);
    r.insert("page", page + 1);
    r.insert("pageSize", pageSize);
    reply(s, Cmd::TRANSACTION_RESP, r, rid);
}

// ---------------------------------------------------------------------------
// 充电站 / 桩查询
// ---------------------------------------------------------------------------

void ServerCore::handleStations(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const double lat    = p.value("latitude").toDouble();
    const double lon    = p.value("longitude").toDouble();
    const double radius = p.value("radius", 5000).toDouble();
    const int limit     = p.value("limit", 20).toInt();
    const int offset    = p.value("offset", 0).toInt();

    int total = 0;
    const QList<StationBrief> stations =
        DatabaseManager::instance().getNearbyStations(lat, lon, radius,
                                               limit, offset, total);

    QVariantList list;
    for (const StationBrief &st : stations) {
        QVariantMap item;
        item.insert("id", st.id);
        item.insert("name", st.name);
        item.insert("address", st.address);
        item.insert("distance", static_cast<qint64>(st.distanceM));
        // 价格: 决策为 ini 两档价, 站点卡展示快充价作为主价, 附慢充价
        item.insert("price", AppConfig::instance().priceFast());
        item.insert("priceFast", AppConfig::instance().priceFast());
        item.insert("priceSlow", AppConfig::instance().priceSlow());
        item.insert("totalPiles", st.totalPiles);
        item.insert("freePiles", st.freePiles);
        item.insert("latitude", st.latitude);
        item.insert("longitude", st.longitude);
        list.append(item);
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("stations", list);
    r.insert("total", total);
    reply(s, Cmd::STATIONS_RESP, r, rid);
}

void ServerCore::handleStationDetail(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int stationId = p.value("stationId").toInt();
    if (stationId <= 0) {
        replyError(s, rid, Err::PARAM_MISSING, "缺少站点ID");
        return;
    }

    DatabaseManager &db = DatabaseManager::instance();
    const Station st = db.getStationById(stationId);
    const QList<Charger> chargers = db.getChargersByStation(stationId);

    int freeCount = 0;
    QVariantList piles;
    for (const Charger &c : chargers) {
        if (c.status == "idle") {
            ++freeCount;
        }
        QVariantMap item;
        // 说明: 协议字段名 pileId 承载的是数据库的 charger_code(字符串主键)
        item.insert("pileId", c.chargerCode);
        item.insert("type", c.type == "fast" ? "快充" : "慢充");
        item.insert("power", c.powerKw);
        item.insert("status", c.status);
        piles.append(item);
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("stationName", st.name);
    r.insert("freePileCount", freeCount);
    r.insert("piles", piles);
    reply(s, Cmd::STATION_DETAIL_RESP, r, rid);
}

// ---------------------------------------------------------------------------
// 充电业务
// ---------------------------------------------------------------------------

void ServerCore::handleCheckOrder(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    bool found = false;
    const Order order = DatabaseManager::instance().getUnfinishedOrder(userId, found);

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("hasUnfinished", found);
    if (found) {
        r.insert("orderId", order.orderNo);
        r.insert("pileId", order.chargerCode);
        r.insert("startTime", order.startTime);
        r.insert("chargedKwh", order.energyKwh);
        r.insert("cost", order.amount);
    }
    reply(s, Cmd::CHECK_ORDER_RESP, r, rid);
}

void ServerCore::handleStartCharge(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const QString pileId = p.value("pileId").toString().trimmed();
    if (pileId.isEmpty()) {
        replyError(s, rid, Err::PARAM_MISSING, "缺少电桩编号");
        return;
    }

    DatabaseManager &db = DatabaseManager::instance();

    // 1. 电桩状态检查
    bool found = false;
    const Charger charger = db.getChargerByCode(pileId, found);
    if (!found) {
        replyError(s, rid, Err::PARAM_FORMAT, "电桩不存在");
        return;
    }
    if (charger.status == "charging") {
        replyError(s, rid, Err::PILE_OCCUPIED, "电桩已被占用");
        return;
    }
    if (charger.status == "fault") {
        replyError(s, rid, Err::PILE_FAULT, "电桩故障");
        return;
    }
    if (charger.status == "offline") {
        replyError(s, rid, Err::PILE_FAULT, "电桩离线");
        return;
    }

    // 2. 未完成订单检查
    bool hasUnfinished = false;
    db.getUnfinishedOrder(userId, hasUnfinished);
    if (hasUnfinished) {
        replyError(s, rid, Err::UNFINISHED_ORDER, "存在未完成订单");
        return;
    }

    // 3. 余额检查
    User user;
    db.getUserInfo(userId, user);
    if (user.balance <= 0) {
        replyError(s, rid, Err::BALANCE_LOW, "余额不足");
        return;
    }

    // 4. 单价按电桩类型取价
    const double unitPrice = AppConfig::instance().priceOf(charger.type);

    // 5. 创建订单
    const QString orderNo = db.nextOrderNo();
    if (!db.createOrder(orderNo, userId, pileId, unitPrice)) {
        replyError(s, rid, Err::DB_ERROR, "创建订单失败");
        return;
    }

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("orderId", orderNo);
    r.insert("pileId", pileId);
    r.insert("startTime", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    // 预估费用 = 目标电量(缺省按 1 小时满功率) * 单价
    const double targetKwh = p.value("targetKwh", charger.powerKw).toDouble();
    r.insert("estimatedCost", targetKwh * unitPrice);
    r.insert("status", "充电中");
    reply(s, Cmd::START_CHARGE_RESP, r, rid);

    emit logMessage(QStringLiteral("开始充电: 用户%1, 桩%2, 订单%3")
                        .arg(userId).arg(pileId).arg(orderNo));
}

void ServerCore::handleStopCharge(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const QString orderNo = p.value("orderId").toString().trimmed();
    if (orderNo.isEmpty()) {
        replyError(s, rid, Err::PARAM_MISSING, "缺少订单号");
        return;
    }

    DatabaseManager &db = DatabaseManager::instance();

    bool found = false;
    Order order = db.getOrderByNo(orderNo, found);
    if (!found || order.userId != userId) {
        replyError(s, rid, Err::PARAM_FORMAT, "订单不存在");
        return;
    }
    if (order.status != "charging") {
        replyError(s, rid, Err::PARAM_FORMAT, "订单不是充电中状态");
        return;
    }

    // 模拟计量: 根据已充电时长与桩功率估算电量
    const QDateTime start = QDateTime::fromString(order.startTime, "yyyy-MM-dd HH:mm:ss");
    const qint64 secs = qMax<qint64>(1, start.secsTo(QDateTime::currentDateTime()));
    bool cf = false;
    const Charger charger = db.getChargerByCode(order.chargerCode, cf);
    const double powerKw = cf ? charger.powerKw : 7.0;
    const double energyKwh = powerKw * (secs / 3600.0) * 150;//150 -> ChargingSpeedUp

    if (!db.finishOrder(orderNo, energyKwh)) {
        replyError(s, rid, Err::DB_ERROR, "结束充电失败");
        return;
    }
    bool doneFound = false;
    const Order done = db.getOrderByNo(orderNo, doneFound);
    const double amount = done.amount;
    const QString endTime = done.endTime;

    if (!db.payOrder(orderNo)) {
        replyError(s, rid, Err::BALANCE_LOW, "结算失败(余额不足)");
        return;
    }
    User payer;
    db.getUserInfo(userId, payer);
    const double newBalance = payer.balance;

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("orderId", orderNo);
    r.insert("chargedKwh", energyKwh);
    r.insert("cost", amount);
    r.insert("endTime", endTime);
    r.insert("newBalance", newBalance);
    reply(s, Cmd::STOP_CHARGE_RESP, r, rid);

    emit logMessage(QStringLiteral("结束充电: 订单%1, 电量%2kWh, 金额%3元, secs = %4, powerKw = %5")
                        .arg(orderNo).arg(energyKwh).arg(amount).arg(secs).arg(powerKw));
}

void ServerCore::handleChargeStatus(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    using namespace Protocol;
    const int userId = requireLogin(s, p, rid);
    if (userId < 0) {
        return;
    }

    const QString orderNo = p.value("orderId").toString().trimmed();
    bool found = false;
    const Order order = DatabaseManager::instance().getOrderByNo(orderNo, found);
    if (!found || order.userId != userId) {
        replyError(s, rid, Err::PARAM_FORMAT, "订单不存在");
        return;
    }

    QString statusText = order.status;
    if (order.status == "charging") statusText = "充电中";
    else if (order.status == "unpaid") statusText = "待支付";
    else if (order.status == "paid") statusText = "已完成";
    else if (order.status == "cancelled") statusText = "已取消";

    QVariantMap r;
    r.insert("code", Err::OK);
    r.insert("orderId", orderNo);
    r.insert("status", statusText);
    r.insert("chargedKwh", order.energyKwh);
    r.insert("startTime", order.startTime);
    reply(s, Cmd::CHARGE_STATUS_RESP, r, rid);
}

// ---------------------------------------------------------------------------
// 登录校验
// ---------------------------------------------------------------------------

int ServerCore::requireLogin(ClientSession *s, const QVariantMap &p, quint32 rid)
{
    const QString token = p.value("token").toString();
    const int userId = m_sessions->validateToken(token);
    if (userId < 0) {
        replyError(s, rid, Protocol::Err::NOT_LOGGED_IN, "未登录或登录已过期");
        return -1;
    }

    User user;
    if (DatabaseManager::instance().getUserInfo(userId, user) && user.status == "frozen") {
        replyError(s, rid, Protocol::Err::ACCOUNT_FROZEN, "账号已被冻结");
        return -1;
    }

    return userId;
}
