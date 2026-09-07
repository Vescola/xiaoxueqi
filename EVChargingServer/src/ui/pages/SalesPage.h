#ifndef SALESPAGE_H
#define SALESPAGE_H

#include <QWidget>

namespace Ui {
class SalesPage;
}

// ---------------------------------------------------------------------------
// 销售业绩页(需求矩阵 #31)
//   三大指标: 今日营收 / 本月营收 / 总营收
//   趋势折线图: 近7日 / 近30日 (TrendChartWidget, QPainter 手绘)
// ---------------------------------------------------------------------------

class SalesPage : public QWidget
{
    Q_OBJECT
public:
    explicit SalesPage(QWidget *parent = nullptr);
    ~SalesPage() override;

    void refresh();

private slots:
    void onRangeChanged();

private:
    Ui::SalesPage *ui;
};

#endif // SALESPAGE_H
