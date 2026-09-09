#ifndef CHARGINGWORKER_H
#define CHARGINGWORKER_H

#include <QObject>
#include <QString>

class QTimer;

class ChargingWorker : public QObject
{
    Q_OBJECT

public:
    explicit ChargingWorker(QObject *parent = nullptr);

public slots:
    void start(const QString &orderId, const QString &pileId, double targetKwh, double unitPrice);
    void stopFault();

signals:
    void progressChanged(double kwh, double cost, int percent);
    void finished(const QString &orderId, double kwh, double cost, bool abnormal, const QString &message);

private:
    void finish(bool abnormal, const QString &message);

    QTimer *m_timer = nullptr;
    QString m_orderId;
    QString m_pileId;
    double m_targetKwh = 0.0;
    double m_unitPrice = 0.0;
    double m_currentKwh = 0.0;
    bool m_finished = false;
};

#endif // CHARGINGWORKER_H
