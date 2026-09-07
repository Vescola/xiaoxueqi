#ifndef STATIONPAGE_H
#define STATIONPAGE_H

#include <QWidget>

namespace Ui {
class StationPage;
}

// ---------------------------------------------------------------------------
// 充电站管理页(需求矩阵 #32 / 项目说明书)
//   列表: 站点ID/站名/地址/经纬度/总电桩数/在线率(只读查询)
//   点击站点行 -> 下方展示该站电桩实时状态明细
//   新增/编辑/删除: 按决策"先只做查询, 写操作搁置", 按钮保留但禁用
// ---------------------------------------------------------------------------

class StationPage : public QWidget
{
    Q_OBJECT
public:
    explicit StationPage(QWidget *parent = nullptr);
    ~StationPage() override;

    void refresh();

private slots:
    void onStationSelected(int row, int column);

private:
    Ui::StationPage *ui;
};

#endif // STATIONPAGE_H
