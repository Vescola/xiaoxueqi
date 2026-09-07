#include "ui/pages/StationPage.h"
#include "ui_StationPage.h"

#include "db/ServerDb.h"

#include <QHeaderView>

StationPage::StationPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StationPage)
{
    ui->setupUi(this);

    // 站点表列
    ui->stationTable->setColumnCount(7);
    ui->stationTable->setHorizontalHeaderLabels({
        "站点ID", "站名", "详细地址", "经度", "纬度", "总电桩数", "在线率"
    });
    ui->stationTable->horizontalHeader()->setStretchLastSection(true);
    ui->stationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // 电桩明细表列
    ui->pileTable->setColumnCount(6);
    ui->pileTable->setHorizontalHeaderLabels({
        "电桩编号", "类型", "功率(kW)", "状态", "累计次数", "累计时长(分)"
    });
    ui->pileTable->horizontalHeader()->setStretchLastSection(true);
    ui->pileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(ui->refreshButton, &QPushButton::clicked, this, &StationPage::refresh);
    connect(ui->stationTable, &QTableWidget::cellClicked,
            this, &StationPage::onStationSelected);

    refresh();
}

StationPage::~StationPage()
{
    delete ui;
}

void StationPage::refresh()
{
    const QList<StationBrief> stations = ServerDb::instance().getStationBriefs();

    ui->stationTable->setRowCount(stations.size());
    for (int i = 0; i < stations.size(); ++i) {
        const StationBrief &s = stations[i];
        const double onlineRate = s.totalPiles > 0
            ? (100.0 * s.onlinePiles / s.totalPiles) : 0.0;

        ui->stationTable->setItem(i, 0, new QTableWidgetItem(QString::number(s.id)));
        ui->stationTable->setItem(i, 1, new QTableWidgetItem(s.name));
        ui->stationTable->setItem(i, 2, new QTableWidgetItem(s.address));
        ui->stationTable->setItem(i, 3, new QTableWidgetItem(QString::number(s.longitude, 'f', 4)));
        ui->stationTable->setItem(i, 4, new QTableWidgetItem(QString::number(s.latitude, 'f', 4)));
        ui->stationTable->setItem(i, 5, new QTableWidgetItem(QString::number(s.totalPiles)));
        ui->stationTable->setItem(i, 6, new QTableWidgetItem(
            QString::number(onlineRate, 'f', 1) + "%"));
    }

    // 刷新后清空明细, 等待重新选择
    ui->pileTable->setRowCount(0);
    ui->pileTitle->setText(QStringLiteral("站内电桩明细（点击上方站点行查看）"));
}

void StationPage::onStationSelected(int row, int column)
{
    Q_UNUSED(column);
    QTableWidgetItem *idItem = ui->stationTable->item(row, 0);
    QTableWidgetItem *nameItem = ui->stationTable->item(row, 1);
    if (!idItem) {
        return;
    }

    const int stationId = idItem->text().toInt();
    const QList<ChargerInfo> chargers =
        ServerDb::instance().getChargersByStation(stationId);

    ui->pileTitle->setText(QStringLiteral("站内电桩明细 —— %1").arg(
        nameItem ? nameItem->text() : QString::number(stationId)));

    const QStringList statusText = { "idle", "charging", "fault", "offline" };
    const QStringList statusCn  = { "闲置", "充电中", "故障", "离线" };

    ui->pileTable->setRowCount(chargers.size());
    for (int i = 0; i < chargers.size(); ++i) {
        const ChargerInfo &c = chargers[i];
        QString cn = c.status;
        const int idx = statusText.indexOf(c.status);
        if (idx >= 0) {
            cn = statusCn[idx];
        }
        ui->pileTable->setItem(i, 0, new QTableWidgetItem(c.chargerCode));
        ui->pileTable->setItem(i, 1, new QTableWidgetItem(c.type == "fast" ? "快充" : "慢充"));
        ui->pileTable->setItem(i, 2, new QTableWidgetItem(QString::number(c.powerKw)));
        ui->pileTable->setItem(i, 3, new QTableWidgetItem(cn));
        ui->pileTable->setItem(i, 4, new QTableWidgetItem(QString::number(c.chargeCount)));
        ui->pileTable->setItem(i, 5, new QTableWidgetItem(QString::number(c.totalDuration)));
    }
}
