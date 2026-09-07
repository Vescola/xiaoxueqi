#include "ui/pages/userpage.h"
#include "ui_userpage.h"

#include "db/databasemanager.h"

#include <QHeaderView>
#include <QMessageBox>

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
    connect(ui->freezeButton, &QPushButton::clicked, this, &UserPage::onFreeze);
    connect(ui->unfreezeButton, &QPushButton::clicked, this, &UserPage::onUnfreeze);

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

int UserPage::selectedUserId() const
{
    const int row = ui->userTable->currentRow();
    if (row < 0) {
        return -1;
    }
    QTableWidgetItem *idItem = ui->userTable->item(row, 0);
    return idItem ? idItem->text().toInt() : -1;
}

void UserPage::onFreeze()
{
    const int userId = selectedUserId();
    if (userId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个用户"));
        return;
    }
    // 惰性删除 = 冻结: 冻结后该账号无法登录
    if (DatabaseManager::instance().setUserStatus(userId, "frozen")) {
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("已冻结用户 #%1").arg(userId));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("冻结失败"));
    }
}

void UserPage::onUnfreeze()
{
    const int userId = selectedUserId();
    if (userId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一个用户"));
        return;
    }
    if (DatabaseManager::instance().setUserStatus(userId, "normal")) {
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("已解冻用户 #%1").arg(userId));
        refresh();
    } else {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("解冻失败"));
    }
}

void UserPage::refresh()
{
    int total = 0;
    const QList<User> users =
        DatabaseManager::instance().searchUsers(ui->searchEdit->text(), 0, 1000, total);

    ui->userTable->setRowCount(users.size());
    for (int i = 0; i < users.size(); ++i) {
        const User &u = users[i];
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
