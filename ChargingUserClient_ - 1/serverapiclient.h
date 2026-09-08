#ifndef SERVERAPICLIENT_H
#define SERVERAPICLIENT_H

#include "models.h"

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QVariantMap>
#include <QVector>

#include <optional>

class ServerApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ServerApiClient(QObject *parent = nullptr);

    void setEndpoint(const QString &host, quint16 port);
    QString endpointText() const;
    bool hasToken() const;
    int userId() const;

    bool ping(QString *message = nullptr);
    bool requestSmsCode(const QString &phone, QString *devCode = nullptr, QString *message = nullptr);

    std::optional<User> login(const QString &phone, const QString &rawPassword, QString *message = nullptr);
    std::optional<User> registerUser(const QString &phone,
                                     const QString &rawPassword,
                                     const QString &nickname,
                                     const QString &verifyCode,
                                     QString *message = nullptr);
    bool resetPasswordByPhone(const QString &phone,
                              const QString &newRawPassword,
                              const QString &verifyCode,
                              QString *message = nullptr);
    bool changePassword(const QString &oldRawPassword,
                        const QString &newRawPassword,
                        QString *message = nullptr);
    bool logout(QString *message = nullptr);

    bool queryBalance(double *balance, QString *message = nullptr);
    bool recharge(double amount, double *newBalance = nullptr, QString *message = nullptr);
    QVector<Transaction> transactions(const QDate &from, const QDate &to, QString *message = nullptr);

    QVector<Station> stations(const QString &region, const QString &keyword, QString *message = nullptr);
    QVector<Pile> pilesByStation(const QString &stationId, QString *message = nullptr);
    std::optional<Order> unfinishedOrder(QString *message = nullptr);
    QString startCharge(const QString &pileId, double targetKwh, double *estimatedCost = nullptr, QString *message = nullptr);
    bool stopCharge(const QString &orderId,
                    double *chargedKwh = nullptr,
                    double *cost = nullptr,
                    double *newBalance = nullptr,
                    QString *message = nullptr);

private:
    bool request(quint16 cmd,
                 const QVariantMap &params,
                 quint16 expectedCmd,
                 QVariantMap *reply,
                 QString *message,
                 int timeoutMs = 2600);
    QVariantMap withToken() const;
    QString hashedPassword(const QString &rawPassword) const;
    QString messageFromReply(const QVariantMap &reply) const;
    QDateTime parseDateTime(const QVariant &value) const;
    QString clientStatus(const QString &serverStatus) const;
    QString clientPileType(const QString &serverType) const;

    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 8888;
    quint32 m_nextRequestId = 1;
    QString m_token;
    int m_userId = -1;
};

#endif // SERVERAPICLIENT_H
