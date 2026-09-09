#include "mousetrailwidget.h"

#include <QApplication>
#include <QChildEvent>
#include <QColor>
#include <QEvent>
#include <QLineF>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRadialGradient>
#include <QString>
#include <QTimer>
#include <QtGlobal>

namespace {
constexpr int TrailLifetimeMs = 520;
constexpr int MaxTrailPoints = 22;
}

MouseTrailWidget::MouseTrailWidget(QWidget *host, QWidget *parent)
    : QWidget(parent ? parent : host),
      m_host(host),
      m_fadeTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);

    m_clock.start();
    syncToHost();
    enableMouseTracking(m_host);

    // 事件过滤器用于捕获主窗口内各控件的鼠标移动，形成全局拖尾效果。
    qApp->installEventFilter(this);
    connect(m_fadeTimer, &QTimer::timeout, this, &MouseTrailWidget::removeExpiredPoints);
    m_fadeTimer->start(16);
}

MouseTrailWidget::~MouseTrailWidget()
{
    qApp->removeEventFilter(this);
}

bool MouseTrailWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_host) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == m_host && event->type() == QEvent::Resize) {
        syncToHost();
    } else if (event->type() == QEvent::ChildAdded) {
        auto *childEvent = static_cast<QChildEvent *>(event);
        if (childEvent->child() && childEvent->child()->isWidgetType()) {
            enableMouseTracking(static_cast<QWidget *>(childEvent->child()));
        }
    } else if (event->type() == QEvent::MouseMove ||
               event->type() == QEvent::MouseButtonPress ||
               event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        recordMousePosition(mouseEvent->globalPosition().toPoint());
    }

    return QWidget::eventFilter(watched, event);
}

void MouseTrailWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (m_points.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qint64 now = m_clock.elapsed();
    for (int i = 1; i < m_points.size(); ++i) {
        const TrailPoint &previous = m_points.at(i - 1);
        const TrailPoint &current = m_points.at(i);
        const double ageRatio = qBound(0.0, 1.0 - double(now - current.timestamp) / TrailLifetimeMs, 1.0);
        const double orderRatio = double(i) / qMax(1, m_points.size() - 1);

        QColor lineColor(QStringLiteral("#38bdf8"));
        lineColor.setAlphaF(0.16 + 0.46 * ageRatio * orderRatio);
        QPen pen(lineColor, 2.0 + 5.0 * ageRatio * orderRatio, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(previous.position, current.position);
    }

    for (int i = 0; i < m_points.size(); ++i) {
        const TrailPoint &point = m_points.at(i);
        const double ageRatio = qBound(0.0, 1.0 - double(now - point.timestamp) / TrailLifetimeMs, 1.0);
        const double orderRatio = double(i + 1) / qMax(1, m_points.size());
        const double radius = 7.0 + 12.0 * ageRatio * orderRatio;

        QRadialGradient glow(point.position, radius);
        QColor centerColor(QStringLiteral("#7dd3fc"));
        centerColor.setAlphaF(0.52 * ageRatio);
        QColor edgeColor(QStringLiteral("#2563eb"));
        edgeColor.setAlpha(0);
        glow.setColorAt(0.0, centerColor);
        glow.setColorAt(1.0, edgeColor);

        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(point.position, radius, radius);
    }
}

void MouseTrailWidget::syncToHost()
{
    if (!m_host) {
        return;
    }

    setParent(m_host);
    setGeometry(m_host->rect());
    raise();
    show();
}

void MouseTrailWidget::recordMousePosition(const QPoint &globalPosition)
{
    const QPoint localPosition = mapFromGlobal(globalPosition);
    if (!rect().contains(localPosition)) {
        return;
    }

    if (!m_points.isEmpty() &&
        QLineF(m_points.constLast().position, localPosition).length() < 3.0) {
        return;
    }

    m_points.append({localPosition, m_clock.elapsed()});
    while (m_points.size() > MaxTrailPoints) {
        m_points.removeFirst();
    }
    raise();
    update();
}

void MouseTrailWidget::enableMouseTracking(QWidget *widget)
{
    if (!widget) {
        return;
    }

    widget->setMouseTracking(true);
    const QList<QWidget *> children = widget->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->setMouseTracking(true);
    }
}

void MouseTrailWidget::removeExpiredPoints()
{
    const qint64 now = m_clock.elapsed();
    while (!m_points.isEmpty() && now - m_points.first().timestamp > TrailLifetimeMs) {
        m_points.removeFirst();
    }

    if (!m_points.isEmpty()) {
        update();
    }
}
