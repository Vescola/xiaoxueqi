#ifndef USERPAGE_H
#define USERPAGE_H

#include <QWidget>

namespace Ui {
class UserPage;
}

// ---------------------------------------------------------------------------
// 用户管理页(项目说明书)
//   列表: 用户ID/手机号/昵称/余额/注册时间/状态
//   手机号模糊搜索
//   冻结/解冻: 按决策"先只做查询, 写操作搁置", 按钮保留但禁用
// ---------------------------------------------------------------------------

class UserPage : public QWidget
{
    Q_OBJECT
public:
    explicit UserPage(QWidget *parent = nullptr);
    ~UserPage() override;

    void refresh();

private slots:
    void onSearch();
    void onFreeze();
    void onUnfreeze();

private:
    int selectedUserId() const;   // 返回当前选中行的用户ID, 未选中返回 -1

    Ui::UserPage *ui;
};

#endif // USERPAGE_H
