#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "models.h"

#include <QObject>
#include <QSqlDatabase>
#include <QVector>

#include <optional>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    bool open(QString *errorMessage = nullptr);
    QString databasePath() const;

    bool phoneExists(const QString &phone);
    std::optional<User> userById(int userId);
    std::optional<User> login(const QString &phone, const QString &password, QString *message = nullptr);
    std::optional<User> registerUser(const QString &phone,
                                     const QString &password,
                                     const QString &nickname,
                                     QString *message = nullptr);
    bool resetPasswordByPhone(const QString &phone, const QString &newPassword, QString *message = nullptr);
    bool changePassword(int userId, const QString &oldPassword, const QString &newPassword, QString *message = nullptr);
    bool updateProfile(int userId, const QString &nickname, const QString &avatarPath, QString *message = nullptr);

    bool recharge(int userId, double amount, double *newBalance = nullptr, QString *message = nullptr);
    QVector<Transaction> transactions(int userId, const QDate &from, const QDate &to);

    QVector<Station> stations(const QString &region, const QString &keyword);
    QVector<Pile> pilesByStation(const QString &stationId);
    std::optional<Pile> pileById(const QString &pileId);
    bool setPileStatus(const QString &pileId, const QString &status, QString *message = nullptr);

    std::optional<Order> unfinishedOrder(int userId);
    QString createChargingOrder(int userId, const QString &pileId, QString *message = nullptr);
    bool completeChargingOrder(const QString &orderId,
                               double kwh,
                               double cost,
                               bool abnormal,
                               QString *message = nullptr);
    bool settleOrder(const QString &orderId, double *newBalance = nullptr, QString *message = nullptr);
    QVector<Order> orders(int userId);

private:
    bool createTables(QString *message);
    bool seedDemoData(QString *message);
    bool execSql(const QString &sql, QString *message);
    User readUser(const QSqlQuery &query) const;
    Station readStation(const QSqlQuery &query) const;
    Pile readPile(const QSqlQuery &query) const;
    Order readOrder(const QSqlQuery &query) const;

    QString m_connectionName;
    QSqlDatabase m_db;
    QString m_databasePath;
};

#endif // DATABASEMANAGER_H
