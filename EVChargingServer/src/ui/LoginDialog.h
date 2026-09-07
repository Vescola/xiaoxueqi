#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class ServerCore;

namespace Ui {
class LoginDialog;
}

// ---------------------------------------------------------------------------
// 管理员登录框
//
// 依《思路.docx》: "管理员登陆 GUI 发请求 -> 子线程查数据库(获取哈希+盐)
// 防止界面卡死 -> 子线程本地计算比对 -> 返回布尔结果给 GUI"。
// 故登录校验经由 ServerCore(业务子线程) 完成, 结果通过信号跨线程回传。
// ---------------------------------------------------------------------------

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(ServerCore *core, QWidget *parent = nullptr);
    ~LoginDialog() override;

private slots:
    void onLoginClicked();
    void onAdminLoginFinished(int requestId, bool ok);

private:
    Ui::LoginDialog *ui;
    ServerCore *m_core;
    int m_requestId = -1;   // 当前登录请求的编号, 用于匹配结果
};

#endif // LOGINDIALOG_H
