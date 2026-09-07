#include "ui/pages/chargereditdialog.h"
#include "ui_chargereditdialog.h"

#include "db/databasemanager.h"

ChargerEditDialog::ChargerEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChargerEditDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    reloadStations();
}

ChargerEditDialog::~ChargerEditDialog()
{
    delete ui;
}

void ChargerEditDialog::reloadStations()
{
    ui->stationCombo->clear();
    const QList<Station> stations = DatabaseManager::instance().getAllStations();
    for (const Station &s : stations) {
        ui->stationCombo->addItem(
            QStringLiteral("#%1 %2").arg(s.id).arg(s.name), s.id);
    }
}

void ChargerEditDialog::setCharger(const QString &chargerCode, int stationId,
                                   const QString &type, double powerKw)
{
    ui->codeEdit->setText(chargerCode);
    ui->codeEdit->setReadOnly(true);   // 主键不可改

    const int idx = ui->stationCombo->findData(stationId);
    if (idx >= 0) {
        ui->stationCombo->setCurrentIndex(idx);
    }
    ui->typeCombo->setCurrentIndex(type == "fast" ? 0 : 1);
    ui->powerSpin->setValue(powerKw);
}

QString ChargerEditDialog::chargerCode() const
{
    return ui->codeEdit->text().trimmed();
}

int ChargerEditDialog::stationId() const
{
    return ui->stationCombo->currentData().toInt();
}

QString ChargerEditDialog::type() const
{
    return (ui->typeCombo->currentIndex() == 0) ? "fast" : "slow";
}

double ChargerEditDialog::powerKw() const
{
    return ui->powerSpin->value();
}
