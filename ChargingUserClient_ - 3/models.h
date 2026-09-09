#ifndef MODELS_H
#define MODELS_H

#include <QDateTime>
#include <QString>

struct User
{
    int id = -1;
    QString phone;
    QString password;
    QString nickname;
    QString avatarPath;
    QString status = "正常";
    double balance = 0.0;
    QDateTime createdAt;
};

struct Station
{
    QString id;
    QString name;
    QString region;
    QString address;
    double latitude = 0.0;
    double longitude = 0.0;
    double price = 0.0;
    double distance = 0.0;
    double onlineRate = 0.0;
    int recommendScore = 0;
    int totalPiles = 0;
    int idlePiles = 0;
};

struct Pile
{
    QString id;
    QString stationId;
    QString stationName;
    QString type;
    double power = 0.0;
    QString status;
    int totalTimes = 0;
    double totalHours = 0.0;
};

struct Transaction
{
    QString type;
    double amount = 0.0;
    QDateTime happenedAt;
    QString note;
};

struct Order
{
    QString id;
    int userId = -1;
    QString pileId;
    QDateTime startAt;
    QDateTime endAt;
    double kwh = 0.0;
    double cost = 0.0;
    QString status;
};

#endif // MODELS_H
