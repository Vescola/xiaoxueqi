#include "ui/pages/OrderPage.h"
#include "ui_OrderPage.h"

#include "db/ServerDb.h"

#include <QHeaderView>

OrderPage::OrderPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OrderPage)
{
    ui->setupUi(this);

    ui->orderTable->setColumnCount(10);
    ui->orderTable->setHorizontalHeaderLabels({
        "订单号", "手机号", "电桩编号", "状态", "开始时间", "结束时间",
        "时长(分)", "电量(kWh)", "单价(元)", "金额(元)"
    });
    ui->orderTable->horizontalHeader()->setStretchLastSection(true);
    ui->orderTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(ui->refreshButton, &QPushButton::clicked, this, &OrderPage::refresh);
    connect(ui->searchButton, &QPushButton::clicked, this, &OrderPage::onSearch);

    refresh();
}

OrderPage::~OrderPage()
{
    delete ui;
}

void OrderPage::onSearch()
{
    refresh();
}

void OrderPage::refresh()
{
    // 状态下拉 -> 数据库状态值
    const int idx = ui->statusCombo->currentIndex();
    const QStringList statusEn = { "", "charging", "unpaid", "paid", "cancelled" };
    const QString statusFilter = statusEn.value(idx);

    int total = 0;
    const QList<OrderInfo> orders = ServerDb::instance().listOrders(
        0, 1000, total, statusFilter, ui->phoneEdit->text());

    const QStringList statusCn = { "充电中", "待支付", "已支付", "已取消" };
    const QStringList statusKey = { "charging", "unpaid", "paid", "cancelled" };

    ui->orderTable->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const OrderInfo &o = orders[i];
        QString cn = o.status;
        const int si = statusKey.indexOf(o.status);
        if (si >= 0) cn = statusCn[si];

        ui->orderTable->setItem(i, 0, new QTableWidgetItem(o.orderNo));
        ui->orderTable->setItem(i, 1, new QTableWidgetItem(o.userPhone));
        ui->orderTable->setItem(i, 2, new QTableWidgetItem(o.chargerCode));
        ui->orderTable->setItem(i, 3, new QTableWidgetItem(cn));
        ui->orderTable->setItem(i, 4, new QTableWidgetItem(o.startTime));
        ui->orderTable->setItem(i, 5, new QTableWidgetItem(o.endTime));
        ui->orderTable->setItem(i, 6, new QTableWidgetItem(QString::number(o.durationMinutes)));
        ui->orderTable->setItem(i, 7, new QTableWidgetItem(QString::number(o.energyKwh, 'f', 2)));
        ui->orderTable->setItem(i, 8, new QTableWidgetItem(QString::number(o.unitPrice, 'f', 2)));
        ui->orderTable->setItem(i, 9, new QTableWidgetItem(QString::number(o.amount, 'f', 2)));
    }
}
