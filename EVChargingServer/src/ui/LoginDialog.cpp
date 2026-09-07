#include "ui/LoginDialog.h"
#include "ui_LoginDialog.h"

#include "net/ServerCore.h"

LoginDialog::LoginDialog(ServerCore *core, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , m_core(core)
{
    ui->setupUi(this);

    connect(ui->loginButton, &QPushButton::clicked,
            this, &LoginDialog::onLoginClicked);
    connect(ui->cancelButton, &QPushButton::clicked,
            this, &QDialog::reject);

    // 跨线程结果回传
    connect(m_core, &ServerCore::adminLoginFinished,
            this, &LoginDialog::onAdminLoginFinished);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::onLoginClicked()
{
    const QString account = ui->accountEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    if (account.isEmpty() || password.isEmpty()) {
        ui->errorLabel->setText(QStringLiteral("请输入账号和密码"));
        return;
    }

    ui->errorLabel->setText(QStringLiteral("正在校验…"));
    ui->loginButton->setEnabled(false);

    // 用递增编号标识本次请求
    static int seq = 0;
    m_requestId = ++seq;

    // 发到业务子线程校验(GUI 不阻塞)
    QMetaObject::invokeMethod(m_core, "requestAdminLogin", Qt::QueuedConnection,
                              Q_ARG(int, m_requestId),
                              Q_ARG(QString, account),
                              Q_ARG(QString, password));
}

void LoginDialog::onAdminLoginFinished(int requestId, bool ok)
{
    if (requestId != m_requestId) {
        return;   // 不是本次请求的结果
    }
    ui->loginButton->setEnabled(true);

    if (ok) {
        accept();
    } else {
        ui->errorLabel->setText(QStringLiteral("账号或密码错误"));
    }
}
