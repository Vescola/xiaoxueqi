#include <QApplication>
#include <QThread>
#include <QMessageBox>

#include "common/AppConfig.h"
#include "db/ServerDb.h"
#include "net/ServerCore.h"
#include "ui/LauncherWindow.h"

// ---------------------------------------------------------------------------
// 程序入口
//
// 启动顺序(依《思路.docx》):
//   1. 加载 ini 配置
//   2. 打开数据库(建表 + 演示数据)
//   3. 启动业务核心 ServerCore 到独立子线程(常驻, 与 GUI 登录无关)
//   4. 显示服务器端主界面(LauncherWindow, 常驻)
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EVChargingServer");
    app.setOrganizationName("Neusoft");

    // 1. 配置
    AppConfig::instance().load();

    // 2. 数据库(主线程打开并建表/播种)
    if (!ServerDb::instance().open(AppConfig::instance().dbPath(),
                                   AppConfig::instance().autoSeed())) {
        QMessageBox::critical(nullptr, QStringLiteral("数据库错误"),
            QStringLiteral("无法打开数据库文件:\n%1")
                .arg(AppConfig::instance().dbPath()));
        return 1;
    }

    // 3. 业务核心 -> 子线程
    QThread *coreThread = new QThread;
    ServerCore *core = new ServerCore;   // 无父对象, 由线程接管
    core->moveToThread(coreThread);
    QObject::connect(coreThread, &QThread::started, core, &ServerCore::start);
    QObject::connect(coreThread, &QThread::finished, core, &QObject::deleteLater);
    coreThread->setObjectName("ServerCoreThread");
    coreThread->start();

    // 4. 主界面(常驻)
    LauncherWindow w(core);
    w.show();

    const int ret = app.exec();

    // 退出时先停止业务线程
    coreThread->quit();
    coreThread->wait();

    return ret;
}
