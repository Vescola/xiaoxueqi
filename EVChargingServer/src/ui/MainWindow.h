#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

namespace Ui {
class MainWindow;
}

// ---------------------------------------------------------------------------
// 管理后台独立窗口
//
// 登录成功后由 LauncherWindow 弹出; 关闭本窗口 = 退出登录。
// 五个管理页面均在 C++ 中实例化并挂到 stack, 各自的静态界面在 .ui 中定义。
// ---------------------------------------------------------------------------

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNavChanged(int index);
    void onLogout();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
