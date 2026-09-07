#include "ui/pages/trendchartwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QVector>
#include <algorithm>

TrendChartWidget::TrendChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(260);
}

void TrendChartWidget::setData(const QList<QPair<QString, double>> &data)
{
    m_data = data;
    update();
}

void TrendChartWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void TrendChartWidget::setUnit(const QString &unit)
{
    m_unit = unit;
    update();
}

void TrendChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = rect().adjusted(10, 10, -10, -10);

    // 标题
    if (!m_title.isEmpty()) {
        p.setPen(QColor("#333333"));
        QFont titleFont = p.font();
        titleFont.setBold(true);
        titleFont.setPointSize(11);
        p.setFont(titleFont);
        p.drawText(QRectF(area.left(), area.top(), area.width(), 24),
                   Qt::AlignLeft | Qt::AlignVCenter, m_title);
    }

    // 绘图区
    const QRectF plot(area.left(), area.top() + 30,
                      area.width(), area.height() - 58);

    // 背景 + 边框
    p.fillRect(plot, QColor("#fafafa"));
    p.setPen(QColor("#e0e0e0"));
    p.drawRect(plot);

    if (m_data.isEmpty()) {
        p.setPen(QColor("#999999"));
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("暂无数据"));
        return;
    }

    // 值域
    double vmin = m_data.first().second;
    double vmax = m_data.first().second;
    for (const auto &kv : m_data) {
        vmin = std::min(vmin, kv.second);
        vmax = std::max(vmax, kv.second);
    }
    if (vmax == vmin) {
        vmax = vmin + 1.0;
    }
    const double span = vmax - vmin;
    const double pad = span * 0.1;
    vmin -= pad;
    vmax += pad;
    if (vmin < 0 && vmax < 0) vmax = 0;

    // 水平网格线 + Y 轴刻度(4 条)
    const int gridLines = 4;
    p.setPen(QColor("#eeeeee"));
    QFont tickFont = p.font();
    tickFont.setPointSize(8);
    p.setFont(tickFont);
    for (int i = 0; i <= gridLines; ++i) {
        const double y = plot.top() + plot.height() * i / gridLines;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        const double val = vmax - (vmax - vmin) * i / gridLines;
        p.setPen(QColor("#888888"));
        p.drawText(QRectF(plot.right() + 4, y - 8, 70, 16),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(val, 'f', 1));
        p.setPen(QColor("#eeeeee"));
    }

    // 折线顶点
    QVector<QPointF> points;
    const int n = m_data.size();
    const double stepX = plot.width() / (n > 1 ? n - 1 : 1);
    for (int i = 0; i < n; ++i) {
        const double x = plot.left() + i * stepX;
        const double ratio = (m_data[i].second - vmin) / (vmax - vmin);
        const double y = plot.bottom() - ratio * plot.height();
        points.append(QPointF(x, y));
    }

    // 面积渐变
    QLinearGradient grad(0, plot.top(), 0, plot.bottom());
    grad.setColorAt(0.0, QColor(30, 130, 220, 70));
    grad.setColorAt(1.0, QColor(30, 130, 220, 0));
    QPainterPath areaPath;
    areaPath.moveTo(points.first());
    for (const QPointF &pt : points) {
        areaPath.lineTo(pt);
    }
    areaPath.lineTo(points.last().x(), plot.bottom());
    areaPath.lineTo(points.first().x(), plot.bottom());
    areaPath.closeSubpath();
    p.fillPath(areaPath, grad);

    // 折线
    QPen linePen(QColor("#1e82dc"), 2);
    p.setPen(linePen);
    for (int i = 0; i < n - 1; ++i) {
        p.drawLine(points[i], points[i + 1]);
    }

    // 数据点
    p.setBrush(QColor("#ffffff"));
    for (const QPointF &pt : points) {
        p.drawEllipse(pt, 3.5, 3.5);
    }

    // X 轴标签(最多约 10 个, 避免重叠)
    p.setPen(QColor("#666666"));
    const int labelStep = std::max(1, n / 10);
    for (int i = 0; i < n; i += labelStep) {
        const QPointF pt = points[i];
        p.drawText(QRectF(pt.x() - 40, plot.bottom() + 4, 80, 16),
                   Qt::AlignHCenter | Qt::AlignTop, m_data[i].first);
    }
}
