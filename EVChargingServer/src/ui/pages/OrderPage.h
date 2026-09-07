#ifndef ORDERPAGE_H
#define ORDERPAGE_H

#include <QWidget>

namespace Ui {
class OrderPage;
}

// ---------------------------------------------------------------------------
// 订单管理页(需求矩阵 #30, 只读查询)
//   列表: 订单号/手机号/电桩/状态/起止时间/时长/电量/单价/金额
//   支持按状态、手机号筛选
// ---------------------------------------------------------------------------

class OrderPage : public QWidget
{
    Q_OBJECT
public:
    explicit OrderPage(QWidget *parent = nullptr);
    ~OrderPage() override;

    void refresh();

private slots:
    void onSearch();
    void onCancelOrder();

private:
    Ui::OrderPage *ui;
};

#endif // ORDERPAGE_H
