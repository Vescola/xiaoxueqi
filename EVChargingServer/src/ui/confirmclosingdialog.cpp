#include "confirmclosingdialog.h"
#include "ui_confirmclosingdialog.h"

ConfirmClosingDialog::ConfirmClosingDialog(QString text, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfirmClosingDialog)
{
    ui->setupUi(this);
    ui->label->setText(text);
    ui->label->setStyleSheet("font-size: 17px;");

}

ConfirmClosingDialog::~ConfirmClosingDialog()
{
    delete ui;
}
