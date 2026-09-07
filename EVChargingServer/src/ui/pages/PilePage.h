#ifndef PILEPAGE_H
#define PILEPAGE_H

#include <QWidget>

namespace Ui {
class PilePage;
}

// ---------------------------------------------------------------------------
// 充电桩管理页(需求矩阵 #33)
//   状态分布卡: 在用 / 闲置 / 故障 数量与占比
//   列表: 电桩编号/所属电站/类型/功率/状态/累计次数/累计时长
//   远程重启: 按决策"先只做查询, 写操作搁置", 按钮保留但禁用
// ---------------------------------------------------------------------------

class PilePage : public QWidget
{
    Q_OBJECT
public:
    explicit PilePage(QWidget *parent = nullptr);
    ~PilePage() override;

    void refresh();

private slots:
    void onFilterChanged();

private:
    Ui::PilePage *ui;
};

#endif // PILEPAGE_H
