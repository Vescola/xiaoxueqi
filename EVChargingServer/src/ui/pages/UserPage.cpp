#include "ui/pages/UserPage.h"
#include "ui_UserPage.h"

#include "db/ServerDb.h"

#include <QHeaderView>

UserPage::UserPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UserPage)
{
    ui->setupUi(this);

    ui->userTable->setColumnCount(6);
    ui->userTable->setHorizontalHeaderLabels({
        "用户ID", "手机号", "昵称", "钱包余额", "注册时间", "状态"
    });
    ui->userTable->horizontalHeader()->setStretchLastSection(true);
    ui->userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(ui->refreshButton, &QPushButton::clicked, this, &UserPage::refresh);
    connect(ui->searchButton, &QPushButton::clicked, this, &UserPage::onSearch);
    connect(ui->searchEdit, &QLineEdit::returnPressed, this, &UserPage::onSearch);

    refresh();
}

UserPage::~UserPage()
{
    delete ui;
}

void UserPage::onSearch()
{
    refresh();
}

void UserPage::refresh()
{
    int total = 0;
    const QList<UserInfo> users =
        ServerDb::instance().searchUsers(ui->searchEdit->text(), 0, 1000, total);

    ui->userTable->setRowCount(users.size());
    for (int i = 0; i < users.size(); ++i) {
        const UserInfo &u = users[i];
        ui->userTable->setItem(i, 0, new QTableWidgetItem(QString::number(u.id)));
        ui->userTable->setItem(i, 1, new QTableWidgetItem(u.phone));
        ui->userTable->setItem(i, 2, new QTableWidgetItem(u.nickname));
        ui->userTable->setItem(i, 3, new QTableWidgetItem(
            QString::number(u.balance, 'f', 2)));
        ui->userTable->setItem(i, 4, new QTableWidgetItem(u.createdAt));
        ui->userTable->setItem(i, 5, new QTableWidgetItem(
            u.status == "frozen" ? "冻结" : "正常"));
    }
}
