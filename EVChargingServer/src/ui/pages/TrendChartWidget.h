#ifndef TRENDCHARTWIDGET_H
#define TRENDCHARTWIDGET_H

#include <QWidget>
#include <QList>
#include <QPair>

// ---------------------------------------------------------------------------
// 折线图控件(QPainter 手绘, 不依赖 Qt Charts)
// ---------------------------------------------------------------------------

class TrendChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TrendChartWidget(QWidget *parent = nullptr);

    // data: (横轴标签, 数值) 序列
    void setData(const QList<QPair<QString, double>> &data);
    void setTitle(const QString &title);
    void setUnit(const QString &unit);   // 数值单位, 如 "元"

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<QPair<QString, double>> m_data;
    QString m_title;
    QString m_unit;
};

#endif // TRENDCHARTWIDGET_H
