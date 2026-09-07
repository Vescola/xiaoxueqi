#include "ui/pages/stationeditdialog.h"
#include "ui_stationeditdialog.h"

StationEditDialog::StationEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StationEditDialog)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

StationEditDialog::~StationEditDialog()
{
    delete ui;
}

void StationEditDialog::setStation(int id, const QString &name,
                                   const QString &address,
                                   double longitude, double latitude)
{
    m_stationId = id;
    ui->nameEdit->setText(name);
    ui->addressEdit->setText(address);
    ui->longitudeSpin->setValue(longitude);
    ui->latitudeSpin->setValue(latitude);
}

QString StationEditDialog::name() const
{
    return ui->nameEdit->text().trimmed();
}

QString StationEditDialog::address() const
{
    return ui->addressEdit->text().trimmed();
}

double StationEditDialog::longitude() const
{
    return ui->longitudeSpin->value();
}

double StationEditDialog::latitude() const
{
    return ui->latitudeSpin->value();
}
