#include "serverapiclient.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ServerApiClient api;
    QString message;

    if (!api.ping(&message)) {
        qWarning().noquote() << "PING_FAIL" << message;
        return 1;
    }

    const std::optional<User> user = api.login(QStringLiteral("13800138000"),
                                               QStringLiteral("Demo@123"),
                                               &message);
    if (!user) {
        qWarning().noquote() << "LOGIN_FAIL" << message;
        return 2;
    }

    double balance = 0.0;
    if (!api.queryBalance(&balance, &message)) {
        qWarning().noquote() << "BALANCE_FAIL" << message;
        return 3;
    }

    const QVector<Station> stations = api.stations(QString(), QString(), &message);
    if (stations.isEmpty()) {
        qWarning().noquote() << "STATIONS_FAIL" << message;
        return 4;
    }

    const QVector<Pile> piles = api.pilesByStation(stations.first().id, &message);
    if (piles.isEmpty()) {
        qWarning().noquote() << "PILES_FAIL" << message;
        return 5;
    }

    const QVector<Transaction> records = api.transactions(QDate::currentDate().addDays(-30),
                                                          QDate::currentDate(),
                                                          &message);

    qInfo().noquote() << "SOCKET_OK"
                      << "userId=" + QString::number(user->id)
                      << "nickname=" + user->nickname
                      << "balance=" + QString::number(balance, 'f', 2)
                      << "stations=" + QString::number(stations.size())
                      << "firstStation=" + stations.first().name
                      << "piles=" + QString::number(piles.size())
                      << "records=" + QString::number(records.size());

    api.logout(&message);
    return 0;
}
