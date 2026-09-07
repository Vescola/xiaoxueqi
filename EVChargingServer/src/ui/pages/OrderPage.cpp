#include "ui/pages/orderpage.h"
#include "ui_orderpage.h"

#include "db/databasemanager.h"

#include <QHeaderView>
#include <QMessageBox>

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
    connect(ui->cancelButton, &QPushButton::clicked, this, &OrderPage::onCancelOrder);

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

void OrderPage::onCancelOrder()
{
    const int row = ui->orderTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个订单"));
        return;
    }
    QTableWidgetItem *noItem = ui->orderTable->item(row, 0);
    if (!noItem) {
        return;
    }
    const QString orderNo = noItem->text();

    // 惰性删除: 仅"待支付"订单可取消(charging/paid 不可取消)
    if (DatabaseManager::instance().cancelOrder(orderNo)) {
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("已取消订单 %1").arg(orderNo));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"),
                             QStringLiteral("仅「待支付」订单可取消"));
    }
}

void OrderPage::refresh()
{
    // 状态下拉 -> 数据库状态值
    const int idx = ui->statusCombo->currentIndex();
    const QStringList statusEn = { "", "charging", "unpaid", "paid", "cancelled" };
    const QString statusFilter = statusEn.value(idx);

    int total = 0;
    const QList<Order> orders = DatabaseManager::instance().listOrders(
        0, 1000, total, statusFilter, ui->phoneEdit->text());

    const QStringList statusCn = { "充电中", "待支付", "已支付", "已取消" };
    const QStringList statusKey = { "charging", "unpaid", "paid", "cancelled" };

    ui->orderTable->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const Order &o = orders[i];
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
