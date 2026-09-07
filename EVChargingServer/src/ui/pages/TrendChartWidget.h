#ifndef TRENDCHARTWIDGET_H
#define TRENDCHARTWIDGET_H

#include <QWidget>
#include <QList>
#include <QPair>

// ---------------------------------------------------------------------------
// 折线图控件(QPainter 手绘, 不依赖 Qt Charts)
//
// 决策依据: 项目说明书说用 QChart, 但《思路.docx》与需求矩阵 #31 均要求
// QPainter; 且 Qt 5.15 开源版 QtCharts 为 GPL。最终按"QPainter 自绘"落地。
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
