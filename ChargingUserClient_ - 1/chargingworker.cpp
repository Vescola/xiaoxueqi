#include "chargingworker.h"

#include <QTimer>
#include <QtGlobal>

// 演示加速倍率: 充电动画按真实功率 × 该倍率快进, 使演示时长可控。
// 例如 120kW 桩: 120 × 150 / 3600 = 5 度/秒, 充 60 度约 12 秒; 7kW 慢充则明显变慢。
constexpr double kSimSpeedUp = 150.0;

ChargingWorker::ChargingWorker(QObject *parent)
    : QObject(parent)
{
}

void ChargingWorker::start(const QString &orderId, const QString &pileId, double targetKwh, double unitPrice, double powerKw)
{
    m_orderId = orderId;
    m_pileId = pileId;
    m_targetKwh = qMax(0.1, targetKwh);
    m_unitPrice = unitPrice;
    m_powerKw = qMax(0.1, powerKw);
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

        // 每 200ms 一步, 每步电量 = 功率 × 倍率 / 3600 × 0.2
        const double step = m_powerKw * kSimSpeedUp / 3600.0 * 0.2;
        m_currentKwh = qMin(m_targetKwh, m_currentKwh + step);
        const double cost = m_currentKwh * m_unitPrice;
        const int percent = qRound((m_currentKwh / m_targetKwh) * 100.0);
        emit progressChanged(m_currentKwh, cost, qBound(0, percent, 100));

        if (m_currentKwh >= m_targetKwh - 0.0001) {
            finish(false, QStringLiteral("充电完成，请确认结算"));
        }
    });

    emit progressChanged(0.0, 0.0, 0);
    m_timer->start(200);
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
