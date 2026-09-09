#include "serverapiclient.h"

#include "clientprotocol.h"

#include <QCryptographicHash>
#include <QDate>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QTime>
#include <QVariantList>
#include <QtGlobal>

ServerApiClient::ServerApiClient(QObject *parent)
    : QObject(parent)
{
}

void ServerApiClient::setEndpoint(const QString &host, quint16 port)
{
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port == 0 ? 8888 : port;
}

QString ServerApiClient::endpointText() const
{
    return QStringLiteral("%1:%2").arg(m_host).arg(m_port);
}

bool ServerApiClient::hasToken() const
{
    return !m_token.isEmpty() && m_userId > 0;
}

int ServerApiClient::userId() const
{
    return m_userId;
}

bool ServerApiClient::ping(QString *message)
{
    // 服务端未实现心跳命令(0x6xxx), 发送会走 default 分支回 0x9FFF 未知命令码。
    // 这里改为纯 TCP 握手探测: 能连上即视为服务端在线, 不发任何业务包。
    QTcpSocket socket;
    socket.connectToHost(m_host, m_port);
    if (!socket.waitForConnected(1200)) {
        if (message) {
            *message = QStringLiteral("无法连接服务端 %1：%2")
                           .arg(endpointText(), socket.errorString());
        }
        return false;
    }
    socket.disconnectFromHost();
    return true;
}

bool ServerApiClient::requestSmsCode(const QString &phone, QString *devCode, QString *message)
{
    QVariantMap params;
    params.insert(QStringLiteral("phone"), phone);

    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::SMS_CODE_REQ,
                 params,
                 ClientProtocol::Cmd::SMS_CODE_RESP,
                 &reply,
                 message)) {
        return false;
    }

    if (devCode) {
        *devCode = reply.value(QStringLiteral("devCode")).toString();
    }
    return true;
}

std::optional<User> ServerApiClient::login(const QString &phone, const QString &rawPassword, QString *message)
{
    QVariantMap params;
    params.insert(QStringLiteral("phone"), phone);
    params.insert(QStringLiteral("password"), hashedPassword(rawPassword));

    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::LOGIN_REQ,
                 params,
                 ClientProtocol::Cmd::LOGIN_RESP,
                 &reply,
                 message)) {
        return std::nullopt;
    }

    User user;
    user.id = reply.value(QStringLiteral("userId")).toInt();
    user.phone = phone;
    user.password.clear();
    user.nickname = reply.value(QStringLiteral("nickname")).toString();
    user.balance = reply.value(QStringLiteral("balance")).toDouble();
    user.status = clientStatus(reply.value(QStringLiteral("status")).toString());
    user.createdAt = QDateTime::currentDateTime();

    m_userId = user.id;
    m_token = reply.value(QStringLiteral("token")).toString();
    return user;
}

std::optional<User> ServerApiClient::registerUser(const QString &phone,
                                                  const QString &rawPassword,
                                                  const QString &nickname,
                                                  const QString &verifyCode,
                                                  QString *message)
{
    QVariantMap params;
    params.insert(QStringLiteral("phone"), phone);
    params.insert(QStringLiteral("password"), hashedPassword(rawPassword));
    params.insert(QStringLiteral("nickname"), nickname);
    params.insert(QStringLiteral("verifyCode"), verifyCode);

    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::REGISTER_REQ,
                 params,
                 ClientProtocol::Cmd::REGISTER_RESP,
                 &reply,
                 message)) {
        return std::nullopt;
    }

    return login(phone, rawPassword, message);
}

bool ServerApiClient::resetPasswordByPhone(const QString &phone,
                                           const QString &newRawPassword,
                                           const QString &verifyCode,
                                           QString *message)
{
    QVariantMap params;
    params.insert(QStringLiteral("phone"), phone);
    params.insert(QStringLiteral("newPassword"), hashedPassword(newRawPassword));
    params.insert(QStringLiteral("verifyCode"), verifyCode);

    QVariantMap reply;
    return request(ClientProtocol::Cmd::FIND_PWD_REQ,
                   params,
                   ClientProtocol::Cmd::FIND_PWD_RESP,
                   &reply,
                   message);
}

bool ServerApiClient::changePassword(const QString &oldRawPassword,
                                     const QString &newRawPassword,
                                     QString *message)
{
    QVariantMap params = withToken();
    params.insert(QStringLiteral("oldPassword"), hashedPassword(oldRawPassword));
    params.insert(QStringLiteral("newPassword"), hashedPassword(newRawPassword));

    QVariantMap reply;
    return request(ClientProtocol::Cmd::CHANGE_PWD_REQ,
                   params,
                   ClientProtocol::Cmd::CHANGE_PWD_RESP,
                   &reply,
                   message);
}

bool ServerApiClient::logout(QString *message)
{
    QVariantMap params = withToken();
    QVariantMap reply;
    const bool ok = request(ClientProtocol::Cmd::LOGOUT_REQ,
                            params,
                            ClientProtocol::Cmd::LOGOUT_RESP,
                            &reply,
                            message);
    m_token.clear();
    m_userId = -1;
    return ok;
}

bool ServerApiClient::queryBalance(double *balance, QString *message)
{
    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::BALANCE_QUERY_REQ,
                 withToken(),
                 ClientProtocol::Cmd::BALANCE_QUERY_RESP,
                 &reply,
                 message)) {
        return false;
    }

    if (balance) {
        *balance = reply.value(QStringLiteral("balance")).toDouble();
    }
    return true;
}

bool ServerApiClient::recharge(double amount, double *newBalance, QString *message)
{
    QVariantMap params = withToken();
    params.insert(QStringLiteral("amount"), amount);

    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::RECHARGE_REQ,
                 params,
                 ClientProtocol::Cmd::RECHARGE_RESP,
                 &reply,
                 message)) {
        return false;
    }

    if (newBalance) {
        *newBalance = reply.value(QStringLiteral("newBalance")).toDouble();
    }
    return true;
}

QVector<Transaction> ServerApiClient::transactions(const QDate &from, const QDate &to, QString *message)
{
    QVariantMap params = withToken();
    params.insert(QStringLiteral("startDate"), from.toString(QStringLiteral("yyyy-MM-dd")));
    params.insert(QStringLiteral("endDate"), to.toString(QStringLiteral("yyyy-MM-dd")));
    params.insert(QStringLiteral("page"), 1);
    params.insert(QStringLiteral("pageSize"), 80);

    QVariantMap reply;
    QVector<Transaction> result;
    if (!request(ClientProtocol::Cmd::TRANSACTION_REQ,
                 params,
                 ClientProtocol::Cmd::TRANSACTION_RESP,
                 &reply,
                 message)) {
        return result;
    }

    const QVariantList records = reply.value(QStringLiteral("records")).toList();
    for (const QVariant &value : records) {
        const QVariantMap item = value.toMap();
        Transaction transaction;
        transaction.type = item.value(QStringLiteral("type")).toString();
        transaction.amount = item.value(QStringLiteral("amount")).toDouble();
        transaction.happenedAt = parseDateTime(item.value(QStringLiteral("time")));
        transaction.note = transaction.type;
        result.push_back(transaction);
    }
    return result;
}

QVector<Station> ServerApiClient::stations(const QString &region, const QString &keyword, QString *message)
{
    Q_UNUSED(region)

    QVariantMap params = withToken();
    params.insert(QStringLiteral("latitude"), 39.9042);
    params.insert(QStringLiteral("longitude"), 116.4074);
    // radius=0 表示不限距离: 服务端 getNearbyStations 中 radiusM<=0 时不做
    // 距离过滤, 返回全部站点。避免新建站点(或深圳 seed 站点)因离客户端硬编码
    // 的北京中心超过默认 100km 而被过滤掉, 导致客户端看不到。
    params.insert(QStringLiteral("radius"), 0);
    params.insert(QStringLiteral("limit"), 50);
    params.insert(QStringLiteral("offset"), 0);

    QVariantMap reply;
    QVector<Station> result;
    if (!request(ClientProtocol::Cmd::STATIONS_REQ,
                 params,
                 ClientProtocol::Cmd::STATIONS_RESP,
                 &reply,
                 message)) {
        return result;
    }

    const QString key = keyword.trimmed();
    const QVariantList stations = reply.value(QStringLiteral("stations")).toList();
    for (const QVariant &value : stations) {
        const QVariantMap item = value.toMap();
        Station station;
        station.id = QString::number(item.value(QStringLiteral("id")).toInt());
        station.name = item.value(QStringLiteral("name")).toString();
        station.region = QStringLiteral("服务端");
        station.address = item.value(QStringLiteral("address")).toString();
        station.latitude = item.value(QStringLiteral("latitude")).toDouble();
        station.longitude = item.value(QStringLiteral("longitude")).toDouble();
        station.price = item.value(QStringLiteral("price")).toDouble();
        station.distance = item.value(QStringLiteral("distance")).toDouble() / 1000.0;
        station.totalPiles = item.value(QStringLiteral("totalPiles")).toInt();
        station.idlePiles = item.value(QStringLiteral("freePiles")).toInt();
        station.onlineRate = station.totalPiles > 0
                                 ? double(station.idlePiles) / station.totalPiles
                                 : 0.0;
        station.recommendScore = qBound(60, 98 - int(station.distance / 2.0), 98);

        if (!key.isEmpty() &&
            !station.name.contains(key, Qt::CaseInsensitive) &&
            !station.address.contains(key, Qt::CaseInsensitive)) {
            continue;
        }
        result.push_back(station);
    }
    return result;
}

QVector<Pile> ServerApiClient::pilesByStation(const QString &stationId, QString *message)
{
    QVariantMap params;
    params.insert(QStringLiteral("stationId"), stationId.toInt());

    QVariantMap reply;
    QVector<Pile> result;
    if (!request(ClientProtocol::Cmd::STATION_DETAIL_REQ,
                 params,
                 ClientProtocol::Cmd::STATION_DETAIL_RESP,
                 &reply,
                 message)) {
        return result;
    }

    const QString stationName = reply.value(QStringLiteral("stationName")).toString();
    const QVariantList piles = reply.value(QStringLiteral("piles")).toList();
    for (const QVariant &value : piles) {
        const QVariantMap item = value.toMap();
        Pile pile;
        pile.id = item.value(QStringLiteral("pileId")).toString();
        pile.stationId = stationId;
        pile.stationName = stationName;
        pile.type = clientPileType(item.value(QStringLiteral("type")).toString());
        pile.power = item.value(QStringLiteral("power")).toDouble();
        pile.status = clientStatus(item.value(QStringLiteral("status")).toString());
        pile.totalTimes = item.value(QStringLiteral("chargeCount")).toInt();
        pile.totalHours = item.value(QStringLiteral("totalDuration")).toInt() / 60.0;   // 分钟 -> 小时
        result.push_back(pile);
    }
    return result;
}

std::optional<Order> ServerApiClient::unfinishedOrder(QString *message)
{
    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::CHECK_ORDER_REQ,
                 withToken(),
                 ClientProtocol::Cmd::CHECK_ORDER_RESP,
                 &reply,
                 message)) {
        return std::nullopt;
    }

    if (!reply.value(QStringLiteral("hasUnfinished")).toBool()) {
        return std::nullopt;
    }

    Order order;
    order.id = reply.value(QStringLiteral("orderId")).toString();
    order.userId = m_userId;
    order.pileId = reply.value(QStringLiteral("pileId")).toString();
    order.startAt = parseDateTime(reply.value(QStringLiteral("startTime")));
    order.kwh = reply.value(QStringLiteral("chargedKwh")).toDouble();
    order.cost = reply.value(QStringLiteral("cost")).toDouble();
    order.status = QStringLiteral("充电中");
    return order;
}

QString ServerApiClient::startCharge(const QString &pileId, double targetKwh, double *estimatedCost, QString *message)
{
    QVariantMap params = withToken();
    params.insert(QStringLiteral("pileId"), pileId);
    params.insert(QStringLiteral("targetKwh"), targetKwh);

    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::START_CHARGE_REQ,
                 params,
                 ClientProtocol::Cmd::START_CHARGE_RESP,
                 &reply,
                 message)) {
        return QString();
    }

    if (estimatedCost) {
        *estimatedCost = reply.value(QStringLiteral("estimatedCost")).toDouble();
    }
    return reply.value(QStringLiteral("orderId")).toString();
}

bool ServerApiClient::stopCharge(const QString &orderId,
                                 double *chargedKwh,
                                 double *cost,
                                 double *newBalance,
                                 QString *message)
{
    QVariantMap params = withToken();
    params.insert(QStringLiteral("orderId"), orderId);

    QVariantMap reply;
    if (!request(ClientProtocol::Cmd::STOP_CHARGE_REQ,
                 params,
                 ClientProtocol::Cmd::STOP_CHARGE_RESP,
                 &reply,
                 message,
                 3600)) {
        return false;
    }

    if (chargedKwh) {
        *chargedKwh = reply.value(QStringLiteral("chargedKwh")).toDouble();
    }
    if (cost) {
        *cost = reply.value(QStringLiteral("cost")).toDouble();
    }
    if (newBalance) {
        *newBalance = reply.value(QStringLiteral("newBalance")).toDouble();
    }
    return true;
}

bool ServerApiClient::request(quint16 cmd,
                              const QVariantMap &params,
                              quint16 expectedCmd,
                              QVariantMap *reply,
                              QString *message,
                              int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(m_host, m_port);
    if (!socket.waitForConnected(timeoutMs)) {
        if (message) {
            *message = QStringLiteral("无法连接服务端 %1：%2")
                           .arg(endpointText(), socket.errorString());
        }
        return false;
    }

    const quint32 requestId = m_nextRequestId++;
    const QByteArray packet = ClientProtocol::packMessage(cmd, params, requestId);
    if (socket.write(packet) != packet.size() || !socket.waitForBytesWritten(timeoutMs)) {
        if (message) {
            *message = QStringLiteral("请求发送失败：%1").arg(socket.errorString());
        }
        return false;
    }

    QByteArray buffer;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        buffer.append(socket.readAll());

        quint16 responseCmd = 0;
        quint32 responseId = 0;
        QVariantMap responseParams;
        while (ClientProtocol::tryParseMessage(buffer, responseCmd, responseParams, responseId)) {
            if (responseId != requestId) {
                continue;
            }

            if (responseCmd == ClientProtocol::Cmd::ERROR_RESP) {
                if (message) {
                    *message = messageFromReply(responseParams);
                }
                return false;
            }

            if (responseCmd != expectedCmd) {
                if (message) {
                    *message = QStringLiteral("服务端响应类型不匹配");
                }
                return false;
            }

            const int code = responseParams.value(QStringLiteral("code"), ClientProtocol::Err::OK).toInt();
            if (code != ClientProtocol::Err::OK) {
                if (message) {
                    *message = messageFromReply(responseParams);
                }
                return false;
            }

            if (reply) {
                *reply = responseParams;
            }
            return true;
        }

        const int remaining = timeoutMs - int(timer.elapsed());
        if (remaining <= 0 || !socket.waitForReadyRead(qMin(120, remaining))) {
            continue;
        }
    }

    if (message) {
        *message = QStringLiteral("等待服务端响应超时");
    }
    return false;
}

QVariantMap ServerApiClient::withToken() const
{
    QVariantMap params;
    params.insert(QStringLiteral("token"), m_token);
    return params;
}

QString ServerApiClient::hashedPassword(const QString &rawPassword) const
{
    const QByteArray hash = QCryptographicHash::hash(rawPassword.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString ServerApiClient::messageFromReply(const QVariantMap &reply) const
{
    const QString msg = reply.value(QStringLiteral("message")).toString();
    if (!msg.isEmpty()) {
        return msg;
    }
    return QStringLiteral("服务端返回错误：%1").arg(reply.value(QStringLiteral("code")).toInt());
}

QDateTime ServerApiClient::parseDateTime(const QVariant &value) const
{
    const QString text = value.toString();
    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(text, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    return dateTime.isValid() ? dateTime : QDateTime::currentDateTime();
}

QString ServerApiClient::clientStatus(const QString &serverStatus) const
{
    if (serverStatus == QStringLiteral("normal")) {
        return QStringLiteral("正常");
    }
    if (serverStatus == QStringLiteral("frozen")) {
        return QStringLiteral("冻结");
    }
    if (serverStatus == QStringLiteral("idle")) {
        return QStringLiteral("空闲");
    }
    if (serverStatus == QStringLiteral("charging")) {
        return QStringLiteral("在用");
    }
    if (serverStatus == QStringLiteral("fault")) {
        return QStringLiteral("故障");
    }
    if (serverStatus == QStringLiteral("offline")) {
        return QStringLiteral("离线");
    }
    return serverStatus;
}

QString ServerApiClient::clientPileType(const QString &serverType) const
{
    if (serverType == QStringLiteral("fast")) {
        return QStringLiteral("快充");
    }
    if (serverType == QStringLiteral("slow")) {
        return QStringLiteral("慢充");
    }
    return serverType;
}
