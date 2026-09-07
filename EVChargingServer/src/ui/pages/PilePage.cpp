#include "ui/pages/PilePage.h"
#include "ui_PilePage.h"

#include "db/ServerDb.h"

#include <QHeaderView>

PilePage::PilePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PilePage)
{
    ui->setupUi(this);

    ui->pileTable->setColumnCount(7);
    ui->pileTable->setHorizontalHeaderLabels({
        "电桩编号", "所属电站ID", "类型", "功率(kW)", "状态",
        "累计充电次数", "累计时长(分)"
    });
    ui->pileTable->horizontalHeader()->setStretchLastSection(true);
    ui->pileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(ui->refreshButton, &QPushButton::clicked, this, &PilePage::refresh);
    connect(ui->statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PilePage::onFilterChanged);

    refresh();
}

PilePage::~PilePage()
{
    delete ui;
}

void PilePage::refresh()
{
    // 1. 状态分布
    const QList<StatusCount> dist =
        ServerDb::instance().getChargerStatusDistribution();

    int total = 0, inUse = 0, idle = 0, fault = 0;
    for (const StatusCount &sc : dist) {
        total += sc.count;
        if (sc.status == "charging") inUse = sc.count;
        else if (sc.status == "idle") idle = sc.count;
        else if (sc.status == "fault") fault = sc.count;
    }
    const int offline = total - inUse - idle - fault;

    auto pct = [&](int n) {
        return total > 0 ? (100.0 * n / total) : 0.0;
    };
    ui->inUseLabel->setText(QStringLiteral("%1（%2%）")
        .arg(inUse).arg(pct(inUse), 0, 'f', 1));
    ui->idleLabel->setText(QStringLiteral("%1（%2%）")
        .arg(idle).arg(pct(idle), 0, 'f', 1));
    ui->faultLabel->setText(QStringLiteral("%1（%2%）")
        .arg(fault + offline).arg(pct(fault + offline), 0, 'f', 1));

    // 2. 列表
    onFilterChanged();
}

void PilePage::onFilterChanged()
{
    int total = 0;
    const QList<ChargerInfo> chargers =
        ServerDb::instance().listChargers(0, 1000, total);

    const int idx = ui->statusCombo->currentIndex();
    const QStringList filterText = { "", "idle", "charging", "fault", "offline" };
    const QString filter = filterText.value(idx);

    const QStringList statusCn = { "闲置", "充电中", "故障", "离线" };
    const QStringList statusEn = { "idle", "charging", "fault", "offline" };

    QList<ChargerInfo> shown;
    for (const ChargerInfo &c : chargers) {
        if (filter.isEmpty() || c.status == filter) {
            shown.append(c);
        }
    }

    ui->pileTable->setRowCount(shown.size());
    for (int i = 0; i < shown.size(); ++i) {
        const ChargerInfo &c = shown[i];
        QString cn = c.status;
        const int si = statusEn.indexOf(c.status);
        if (si >= 0) cn = statusCn[si];

        ui->pileTable->setItem(i, 0, new QTableWidgetItem(c.chargerCode));
        ui->pileTable->setItem(i, 1, new QTableWidgetItem(QString::number(c.stationId)));
        ui->pileTable->setItem(i, 2, new QTableWidgetItem(c.type == "fast" ? "快充" : "慢充"));
        ui->pileTable->setItem(i, 3, new QTableWidgetItem(QString::number(c.powerKw)));
        ui->pileTable->setItem(i, 4, new QTableWidgetItem(cn));
        ui->pileTable->setItem(i, 5, new QTableWidgetItem(QString::number(c.chargeCount)));
        ui->pileTable->setItem(i, 6, new QTableWidgetItem(QString::number(c.totalDuration)));
    }
}
