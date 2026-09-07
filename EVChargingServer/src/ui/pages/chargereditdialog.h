#ifndef CHARGEREDITDIALOG_H
#define CHARGEREDITDIALOG_H

#include <QDialog>

namespace Ui {
class ChargerEditDialog;
}

// ---------------------------------------------------------------------------
// 充电桩新增/编辑对话框
// ---------------------------------------------------------------------------

class ChargerEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ChargerEditDialog(QWidget *parent = nullptr);
    ~ChargerEditDialog() override;

    // 编辑模式: 预填已有电桩信息(编号只读, 因 charger_code 为主键)
    void setCharger(const QString &chargerCode, int stationId,
                    const QString &type, double powerKw);

    QString chargerCode() const;
    int     stationId() const;
    QString type() const;        // 返回 "fast" / "slow"
    double  powerKw() const;

private:
    void reloadStations();

    Ui::ChargerEditDialog *ui;
};

#endif // CHARGEREDITDIALOG_H
