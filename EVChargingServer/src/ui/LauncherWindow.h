#ifndef LAUNCHERWINDOW_H
#define LAUNCHERWINDOW_H

#include <QWidget>

class ServerCore;

namespace Ui {
class LauncherWindow;
}

// ---------------------------------------------------------------------------
// 服务器端主界面(常驻)
//
// 依《思路.docx》: 主界面只有一个标题和一个"登录"按钮;
// 点击登录弹出管理员登录框, 登录成功后打开独立的管理后台窗口;
// 关闭管理后台 = 退出登录, 但本主界面不关闭, 后台服务持续运行。
// ---------------------------------------------------------------------------

class LauncherWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LauncherWindow(ServerCore *core, QWidget *parent = nullptr);
    ~LauncherWindow() override;

private slots:
    void onLoginClicked();
    void onQuitClicked();
    void onLogMessage(const QString &msg);
    void onConnectionCountChanged(int count);

private:
    Ui::LauncherWindow *ui;
    ServerCore *m_core;
};

#endif // LAUNCHERWINDOW_H
