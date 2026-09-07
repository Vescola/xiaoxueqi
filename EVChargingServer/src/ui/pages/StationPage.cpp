#include "ui/pages/stationpage.h"
#include "ui_stationpage.h"
#include "ui/pages/stationeditdialog.h"

#include "db/databasemanager.h"

#include <QHeaderView>
#include <QMessageBox>

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
    connect(ui->addButton, &QPushButton::clicked, this, &StationPage::onAddStation);
    connect(ui->editButton, &QPushButton::clicked, this, &StationPage::onEditStation);
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
    const QList<StationBrief> stations = DatabaseManager::instance().getStationBriefs();

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
    const QList<Charger> chargers =
        DatabaseManager::instance().getChargersByStation(stationId);

    ui->pileTitle->setText(QStringLiteral("站内电桩明细 —— %1").arg(
        nameItem ? nameItem->text() : QString::number(stationId)));

    const QStringList statusText = { "idle", "charging", "fault", "offline" };
    const QStringList statusCn  = { "闲置", "充电中", "故障", "离线" };

    ui->pileTable->setRowCount(chargers.size());
    for (int i = 0; i < chargers.size(); ++i) {
        const Charger &c = chargers[i];
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

void StationPage::onAddStation()
{
    StationEditDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增充电站"));
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const int id = DatabaseManager::instance().createStation(
        dlg.name(), dlg.address(), dlg.longitude(), dlg.latitude());
    if (id > 0) {
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("已新增电站 #%1").arg(id));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("新增电站失败"));
    }
}

void StationPage::onEditStation()
{
    const int row = ui->stationTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个电站"));
        return;
    }
    const int id = ui->stationTable->item(row, 0)->text().toInt();
    const QString name = ui->stationTable->item(row, 1)->text();
    const QString addr = ui->stationTable->item(row, 2)->text();
    const double lon = ui->stationTable->item(row, 3)->text().toDouble();
    const double lat = ui->stationTable->item(row, 4)->text().toDouble();

    StationEditDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("编辑充电站"));
    dlg.setStation(id, name, addr, lon, lat);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    if (DatabaseManager::instance().updateStation(
            id, dlg.name(), dlg.address(), dlg.longitude(), dlg.latitude())) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("已更新电站"));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("更新电站失败"));
    }
}
