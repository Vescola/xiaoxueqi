#include "ui/pages/pilepage.h"
#include "ui_pilepage.h"
#include "ui/pages/chargereditdialog.h"

#include "db/databasemanager.h"

#include <QHeaderView>
#include <QMessageBox>

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
    connect(ui->restartButton, &QPushButton::clicked, this, &PilePage::onRestart);
    connect(ui->addChargerButton, &QPushButton::clicked, this, &PilePage::onAddCharger);
    connect(ui->editChargerButton, &QPushButton::clicked, this, &PilePage::onEditCharger);
    connect(ui->deleteChargerButton, &QPushButton::clicked, this, &PilePage::onDeleteCharger);
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
    const QList<ChargerStatusCount> dist =
        DatabaseManager::instance().getChargerStatusDistribution();

    int total = 0, inUse = 0, idle = 0, fault = 0;
    for (const ChargerStatusCount &sc : dist) {
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
    ui->totalLabel->setText(QStringLiteral("%1").arg(total));
    // 2. 列表
    onFilterChanged();
}

void PilePage::onFilterChanged()
{
    int total = 0;
    const QList<Charger> chargers =
        DatabaseManager::instance().listChargers(0, 1000, total);

    const int idx = ui->statusCombo->currentIndex();
    const QStringList filterText = { "", "idle", "charging", "fault", "offline" };
    const QString filter = filterText.value(idx);

    const QStringList statusCn = { "闲置", "充电中", "故障", "离线" };
    const QStringList statusEn = { "idle", "charging", "fault", "offline" };

    QList<Charger> shown;
    for (const Charger &c : chargers) {
        if (filter.isEmpty() || c.status == filter) {
            shown.append(c);
        }
    }

    ui->pileTable->setRowCount(shown.size());
    for (int i = 0; i < shown.size(); ++i) {
        const Charger &c = shown[i];
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

int PilePage::selectedChargerRow() const
{
    return ui->pileTable->currentRow();
}

QString PilePage::selectedChargerCode() const
{
    const int row = selectedChargerRow();
    if (row < 0) {
        return QString();
    }
    QTableWidgetItem *codeItem = ui->pileTable->item(row, 0);
    return codeItem ? codeItem->text() : QString();
}

void PilePage::onRestart()
{
    const QString code = selectedChargerCode();
    if (code.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个电桩"));
        return;
    }
    // 远程重启 = 模拟向桩发送重启指令, 桩回到闲置
    if (DatabaseManager::instance().setChargerStatus(code, "idle")) {
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("已远程重启电桩 %1").arg(code));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("远程重启失败"));
    }
}

void PilePage::onAddCharger()
{
    ChargerEditDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新增电桩"));
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    if (DatabaseManager::instance().createCharger(
            dlg.chargerCode(), dlg.stationId(), dlg.type(), dlg.powerKw())) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("已新增电桩"));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"),
                             QStringLiteral("新增失败(编号可能重复)"));
    }
}

void PilePage::onEditCharger()
{
    const QString code = selectedChargerCode();
    if (code.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个电桩"));
        return;
    }
    bool found = false;
    const Charger c = DatabaseManager::instance().getChargerByCode(code, found);
    if (!found) {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("电桩不存在"));
        return;
    }

    ChargerEditDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("编辑电桩"));
    dlg.setCharger(c.chargerCode, c.stationId, c.type, c.powerKw);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    if (DatabaseManager::instance().updateCharger(code, dlg.type(), dlg.powerKw())) {
        QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("已更新电桩"));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("更新电桩失败"));
    }
}

void PilePage::onDeleteCharger()
{
    const QString code = selectedChargerCode();
    if (code.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个电桩"));
        return;
    }
    // 惰性删除 = 置为离线(offline), 不物理删
    if (DatabaseManager::instance().setChargerStatus(code, "offline")) {
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("已删除(停用)电桩 %1").arg(code));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("删除失败"));
    }
}
