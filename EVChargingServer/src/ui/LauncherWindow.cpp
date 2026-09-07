#include "ui/launcherwindow.h"
#include "ui_launcherwindow.h"

#include "ui/logindialog.h"
#include "ui/mainwindow.h"
#include "net/servercore.h"
#include "common/appconfig.h"

#include <QDateTime>

LauncherWindow::LauncherWindow(ServerCore *core, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LauncherWindow)
    , m_core(core)
{
    ui->setupUi(this);

    // 服务状态(用配置端口, 避免与子线程启动时序竞争)
    ui->statusLabel->setText(
        QStringLiteral("服务状态：运行中 · 监听端口 %1")
            .arg(AppConfig::instance().listenPort()));

    // 跨线程信号(GUI 线程 <-> 业务子线程)
    connect(core, &ServerCore::logMessage,
            this, &LauncherWindow::onLogMessage);
    connect(core, &ServerCore::connectionCountChanged,
            this, &LauncherWindow::onConnectionCountChanged);

    connect(ui->loginButton, &QPushButton::clicked,
            this, &LauncherWindow::onLoginClicked);
    connect(ui->quitButton, &QPushButton::clicked,
            this, &LauncherWindow::onQuitClicked);

    onLogMessage(QStringLiteral("服务器端已就绪 (QPainter 图表 / SQLite / Socket)"));
}

LauncherWindow::~LauncherWindow()
{
    delete ui;
}

void LauncherWindow::onLoginClicked()
{
    // 模态登录框
    LoginDialog dlg(m_core, this);
    if (dlg.exec() == QDialog::Accepted) {
        // 登录成功 -> 打开独立的管理后台窗口(非模态)
        MainWindow *console = new MainWindow(this);
        console->setAttribute(Qt::WA_DeleteOnClose);
        console->show();       // 关闭该窗口 = 退出登录
        onLogMessage(QStringLiteral("管理员已登录管理后台"));
    }
}

void LauncherWindow::onQuitClicked()
{
    close();
}

void LauncherWindow::onLogMessage(const QString &msg)
{
    ui->logView->appendPlainText(
        QStringLiteral("[%1] %2")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg));
}

void LauncherWindow::onConnectionCountChanged(int count)
{
    ui->connLabel->setText(QStringLiteral("在线连接：%1").arg(count));
}
