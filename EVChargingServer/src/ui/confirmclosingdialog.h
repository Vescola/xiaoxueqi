#ifndef CONFIRMCLOSINGDIALOG_H
#define CONFIRMCLOSINGDIALOG_H

#include <QDialog>

namespace Ui {
class ConfirmClosingDialog;
}

class ConfirmClosingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfirmClosingDialog(QString text, QWidget *parent = nullptr);
    ~ConfirmClosingDialog();

private:
    Ui::ConfirmClosingDialog *ui;
};

#endif // CONFIRMCLOSINGDIALOG_H
