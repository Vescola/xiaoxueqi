#include "ui/mainwindow.h"
#include "ui_mainwindow.h"

#include "ui/pages/salespage.h"
#include "ui/pages/stationpage.h"
#include "ui/pages/pilepage.h"
#include "ui/pages/userpage.h"
#include "ui/pages/orderpage.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow)
{
    // 独立顶层窗口(即使有父对象也作为单独窗口弹出, 而不是嵌进主界面重叠显示);
    // 父对象仅用于生命周期管理, 关闭主界面时随之一并销毁。
    setWindowFlag(Qt::Window, true);

    ui->setupUi(this);

    // ---- 五个管理页面(静态界面分别在各自 .ui 中定义) ----
    SalesPage   *sales   = new SalesPage(this);
    StationPage *station = new StationPage(this);
    PilePage    *pile    = new PilePage(this);
    UserPage    *user    = new UserPage(this);
    OrderPage   *order   = new OrderPage(this);

    ui->stack->addWidget(sales);
    ui->stack->addWidget(station);
    ui->stack->addWidget(pile);
    ui->stack->addWidget(user);
    ui->stack->addWidget(order);

    // ---- 侧栏导航 ----
    ui->navList->addItem(QStringLiteral("销售业绩"));
    ui->navList->addItem(QStringLiteral("充电站管理"));
    ui->navList->addItem(QStringLiteral("充电桩管理"));
    ui->navList->addItem(QStringLiteral("用户管理"));
    ui->navList->addItem(QStringLiteral("订单管理"));
    ui->navList->setCurrentRow(0);

    connect(ui->navList, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavChanged);
    connect(ui->logoutButton, &QPushButton::clicked,
            this, &MainWindow::onLogout);

    onNavChanged(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onNavChanged(int index)
{
    ui->stack->setCurrentIndex(index);
    static const char *titles[] = {
        "销售业绩", "充电站管理", "充电桩管理", "用户管理", "订单管理"
    };
    if (index >= 0 && index < 5) {
        ui->titleLabel->setText(QString::fromUtf8(titles[index]));
    }
}

void MainWindow::onLogout()
{
    // 关闭本窗口 = 退出登录(主界面不关闭, 后台服务持续运行)
    close();
}
