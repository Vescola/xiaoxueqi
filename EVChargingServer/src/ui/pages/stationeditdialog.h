#ifndef STATIONEDITDIALOG_H
#define STATIONEDITDIALOG_H

#include <QDialog>

namespace Ui {
class StationEditDialog;
}

// ---------------------------------------------------------------------------
// 充电站新增/编辑对话框
// ---------------------------------------------------------------------------

class StationEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StationEditDialog(QWidget *parent = nullptr);
    ~StationEditDialog() override;

    // 编辑模式: 预填已有电站信息
    void setStation(int id, const QString &name, const QString &address,
                    double longitude, double latitude);
    // 编辑模式下的电站 id(-1 表示新增)
    int  stationId() const { return m_stationId; }

    QString name() const;
    QString address() const;
    double  longitude() const;
    double  latitude() const;

private:
    Ui::StationEditDialog *ui;
    int m_stationId = -1;
};

#endif // STATIONEDITDIALOG_H
