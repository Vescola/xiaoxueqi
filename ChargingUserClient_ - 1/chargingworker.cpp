#include "chargingworker.h"

#include <QTimer>
#include <QtGlobal>

ChargingWorker::ChargingWorker(QObject *parent)
    : QObject(parent)
{
}

void ChargingWorker::start(const QString &orderId, const QString &pileId, double targetKwh, double unitPrice)
{
    m_orderId = orderId;
    m_pileId = pileId;
    m_targetKwh = qMax(0.1, targetKwh);
    m_unitPrice = unitPrice;
    m_currentKwh = 0.0;
    m_finished = false;

    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (m_finished) {
            return;
        }

        const double step = qMax(0.08, m_targetKwh / 40.0);
        m_currentKwh = qMin(m_targetKwh, m_currentKwh + step);
        const double cost = m_currentKwh * m_unitPrice;
        const int percent = qRound((m_currentKwh / m_targetKwh) * 100.0);
        emit progressChanged(m_currentKwh, cost, qBound(0, percent, 100));

        if (m_currentKwh >= m_targetKwh - 0.0001) {
            finish(false, QStringLiteral("充电完成，请确认结算"));
        }
    });

    emit progressChanged(0.0, 0.0, 0);
    m_timer->start(350);
}

void ChargingWorker::stopFault()
{
    if (m_finished) {
        return;
    }
    if (m_currentKwh < 0.1) {
        m_currentKwh = 0.1;
    }
    finish(true, QStringLiteral("电桩故障，已终止充电并生成异常待结算订单"));
}

void ChargingWorker::finish(bool abnormal, const QString &message)
{
    m_finished = true;
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }

    emit finished(m_orderId, m_currentKwh, m_currentKwh * m_unitPrice, abnormal, message);
}
