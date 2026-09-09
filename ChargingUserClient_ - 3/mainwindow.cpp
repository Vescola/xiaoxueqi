#include "mainwindow.h"
#include "chargingworker.h"
#include "mousetrailwidget.h"
#include "serverapiclient.h"
#include "ui_mainwindow.h"

#include <QCloseEvent>
#include <QDate>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidgetItem>
#include <QThread>
#include <QUrl>

namespace {
QPixmap avatarPixmap(const QSize &labelSize, const QString &avatarPath)
{
    const QSize targetSize = labelSize.isValid() && !labelSize.isEmpty()
                                 ? labelSize
                                 : QSize(72, 72);

    QPixmap source;
    if (!avatarPath.isEmpty() && QFileInfo::exists(avatarPath)) {
        source.load(avatarPath);
    }
    if (source.isNull()) {
        source.load(QStringLiteral(":/resources/images/default_avatar.jpg"));
    }

    QPixmap result(targetSize);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (!source.isNull()) {
        const QPixmap scaled = source.scaled(targetSize,
                                             Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
        QPainterPath clipPath;
        clipPath.addEllipse(result.rect().adjusted(1, 1, -1, -1));
        painter.setClipPath(clipPath);
        painter.drawPixmap((targetSize.width() - scaled.width()) / 2,
                           (targetSize.height() - scaled.height()) / 2,
                           scaled);
        return result;
    }

    painter.fillRect(result.rect(), QColor(QStringLiteral("#22312a")));
    painter.setPen(QColor(QStringLiteral("#9db3a8")));
    painter.setFont(QFont(painter.font().family(), targetSize.height() / 4, QFont::Bold));
    painter.drawText(result.rect(), Qt::AlignCenter, QStringLiteral("用户"));
    return result;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(QStringLiteral(":/resources/icons/app.svg")));
    m_mouseTrail = new MouseTrailWidget(ui->centralwidget, ui->centralwidget);
    m_serverApi = new ServerApiClient(this);
    ui->appIconLabel->hide();
    ui->titleLabel->setAlignment(Qt::AlignCenter);
    ui->statusLabel->hide();
    updateHeaderSpacing();

    setupTables();
    setupConnections();

    ui->fromDateEdit->setDate(QDate::currentDate().addDays(-30));
    ui->toDateEdit->setDate(QDate::currentDate());
    ui->mapPreviewBrowser->setOpenExternalLinks(true);
    ui->faultButton->setEnabled(false);
    ui->settleButton->setEnabled(false);
    ui->beginChargeButton->setEnabled(false);
    setProfileEditorVisible(false);
    setPasswordEditorVisible(false);

    QString error;
    if (!m_database.open(&error)) {
        QMessageBox::critical(this, QStringLiteral("数据库错误"), error);
        setEnabled(false);
        return;
    }

    loadStations();
    loadRememberedUser();
    if (!m_currentUser) {
        ui->mainStack->setCurrentWidget(ui->loginPage);
    }

    QString serverMessage;
    if (m_serverApi->ping(&serverMessage)) {
        showMessage(QStringLiteral("已检测到服务端：%1").arg(m_serverApi->endpointText()));
    } else {
        showMessage(QStringLiteral("未检测到服务端，当前可使用本地演示数据"));
    }
}

MainWindow::~MainWindow()
{
    stopChargingThread();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopChargingThread();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    adjustTransactionTableColumns();
    if (m_mouseTrail) {
        m_mouseTrail->raise();
    }
}

void MainWindow::setupTables()
{
    ui->pileTable->setColumnCount(6);
    ui->pileTable->setHorizontalHeaderLabels({
        QStringLiteral("编号"),
        QStringLiteral("类型"),
        QStringLiteral("状态"),
        QStringLiteral("功率(kW)"),
        QStringLiteral("累计次数"),
        QStringLiteral("累计时长(h)")
    });
    ui->pileTable->horizontalHeader()->setStretchLastSection(true);
    ui->pileTable->verticalHeader()->setVisible(false);
    ui->pileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->transactionsTable->setColumnCount(4);
    ui->transactionsTable->setHorizontalHeaderLabels({
        QStringLiteral("业务类型"),
        QStringLiteral("金额"),
        QStringLiteral("发生时间"),
        QStringLiteral("说明")
    });
    ui->transactionsTable->horizontalHeader()->setStretchLastSection(false);
    ui->transactionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->transactionsTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->transactionsTable->setWordWrap(true);
    ui->transactionsTable->verticalHeader()->setVisible(false);
    ui->transactionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::adjustTransactionTableColumns()
{
    if (!ui || !ui->transactionsTable) {
        return;
    }

    const int totalWidth = ui->transactionsTable->viewport()->width();
    if (totalWidth < 240) {
        return;
    }

    const int typeWidth = totalWidth * 18 / 100;
    const int amountWidth = totalWidth * 18 / 100;
    const int timeWidth = totalWidth * 28 / 100;
    const int noteWidth = totalWidth - typeWidth - amountWidth - timeWidth;

    ui->transactionsTable->setColumnWidth(0, typeWidth);
    ui->transactionsTable->setColumnWidth(1, amountWidth);
    ui->transactionsTable->setColumnWidth(2, timeWidth);
    ui->transactionsTable->setColumnWidth(3, noteWidth);
}

void MainWindow::setupConnections()
{
    // 信号与槽：所有按钮动作都在这里集中连接，方便检查窗口交互流程。
    connect(ui->loginButton, &QPushButton::clicked, this, &MainWindow::handleLogin);
    connect(ui->showRegisterButton, &QPushButton::clicked, this, [this]() {
        ui->registerPhoneEdit->setText(ui->loginPhoneEdit->text());
        ui->mainStack->setCurrentWidget(ui->registerPage);
    });
    connect(ui->forgotButton, &QPushButton::clicked, this, [this]() {
        ui->resetPhoneEdit->setText(ui->loginPhoneEdit->text());
        ui->mainStack->setCurrentWidget(ui->resetPage);
    });
    connect(ui->backLoginButton, &QPushButton::clicked, this, [this]() {
        ui->mainStack->setCurrentWidget(ui->loginPage);
    });
    connect(ui->backLoginButton2, &QPushButton::clicked, this, [this]() {
        ui->mainStack->setCurrentWidget(ui->loginPage);
    });
    connect(ui->mainStack, &QStackedWidget::currentChanged, this, [this]() {
        updateHeaderSpacing();
    });
    connect(ui->sendRegisterCodeButton, &QPushButton::clicked, this, &MainWindow::handleSendRegisterCode);
    connect(ui->registerButton, &QPushButton::clicked, this, &MainWindow::handleRegister);
    connect(ui->sendResetCodeButton, &QPushButton::clicked, this, &MainWindow::handleSendResetCode);
    connect(ui->resetPasswordButton, &QPushButton::clicked, this, &MainWindow::handleResetPassword);
    connect(ui->logoutButton, &QPushButton::clicked, this, &MainWindow::handleLogout);

    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::loadStations);
    connect(ui->regionCombo, &QComboBox::currentTextChanged, this, &MainWindow::loadStations);
    connect(ui->stationList, &QListWidget::currentRowChanged, this, &MainWindow::showStation);
    connect(ui->pileTable, &QTableWidget::currentCellChanged, this, [this]() {
        handlePileSelectionChanged();
    });
    connect(ui->navigateButton, &QPushButton::clicked, this, &MainWindow::openNavigation);
    connect(ui->startChargeButton, &QPushButton::clicked, this, &MainWindow::prepareCharge);
    connect(ui->beginChargeButton, &QPushButton::clicked, this, &MainWindow::beginCharge);
    connect(ui->faultButton, &QPushButton::clicked, this, &MainWindow::simulateFault);
    connect(ui->settleButton, &QPushButton::clicked, this, &MainWindow::settleCurrentOrder);
    connect(ui->backStationsButton, &QPushButton::clicked, this, [this]() {
        ui->appTabs->setCurrentWidget(ui->stationsTab);
    });

    connect(ui->rechargeButton, &QPushButton::clicked, this, &MainWindow::handleRecharge);
    connect(ui->queryTransButton, &QPushButton::clicked, this, &MainWindow::queryTransactions);
    connect(ui->modifyProfileButton, &QPushButton::clicked, this, [this]() {
        setProfileEditorVisible(true);
    });
    connect(ui->chooseAvatarButton, &QPushButton::clicked, this, &MainWindow::chooseAvatar);
    connect(ui->saveProfileButton, &QPushButton::clicked, this, &MainWindow::saveProfile);
    connect(ui->changePasswordButton, &QPushButton::clicked, this, [this]() {
        setPasswordEditorVisible(true);
    });
    connect(ui->confirmPasswordButton, &QPushButton::clicked, this, &MainWindow::changePassword);
    connect(ui->cancelPasswordButton, &QPushButton::clicked, this, [this]() {
        setPasswordEditorVisible(false);
    });
    connect(ui->cancelProfileButton, &QPushButton::clicked, this, [this]() {
        setProfileEditorVisible(false);
    });
    connect(ui->appTabs, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget *tab = ui->appTabs->widget(index);
        if (tab == ui->walletTab) {
            refreshWallet();
            queryTransactions();
        } else if (tab == ui->profileTab) {
            refreshProfile();
        } else if (tab == ui->chargeTab && m_currentUser && m_currentOrderId.isEmpty()) {
            QString message;
            const std::optional<Order> order = usingServer()
                                                   ? m_serverApi->unfinishedOrder(&message)
                                                   : m_database.unfinishedOrder(m_currentUser->id);
            if (order) {
                showUnfinishedOrder(*order);
            }
        }
    });
}

bool MainWindow::isPhoneValid(const QString &phone) const
{
    static const QRegularExpression regex(
        QStringLiteral("^1(3[0-9]|4[5-9]|5[0-35-9]|6[2567]|7[0-8]|8[0-9]|9[0-35-9])\\d{8}$"));
    return regex.match(phone).hasMatch();
}

bool MainWindow::isPasswordValid(const QString &password) const
{
    static const QRegularExpression regex(QStringLiteral("^(?=.*[A-Z])(?=.*[^A-Za-z0-9]).{8}$"));
    return regex.match(password).hasMatch();
}

bool MainWindow::isNicknameValid(const QString &nickname) const
{
    static const QRegularExpression regex(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{2,14}$"));
    return regex.match(nickname).hasMatch();
}

QString MainWindow::generateCode() const
{
    return QString::number(QRandomGenerator::global()->bounded(1000, 10000));
}

void MainWindow::showMessage(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::showWarning(const QString &message)
{
    statusBar()->showMessage(message, 7000);
    QMessageBox::warning(this, QStringLiteral("提示"), message);
}

void MainWindow::handleSendRegisterCode()
{
    const QString phone = ui->registerPhoneEdit->text().trimmed();
    if (!isPhoneValid(phone)) {
        showWarning(QStringLiteral("请输入合法的11位大陆手机号"));
        return;
    }

    QString serverMessage;
    if (m_serverApi && m_serverApi->ping(&serverMessage)) {
        QString devCode;
        if (!m_serverApi->requestSmsCode(phone, &devCode, &serverMessage)) {
            showWarning(QStringLiteral("服务端验证码获取失败：%1").arg(serverMessage));
            return;
        }
        if (!devCode.isEmpty()) {
            m_registerCodes.insert(phone, devCode);
            QMessageBox::information(this,
                                     QStringLiteral("服务端验证码"),
                                     QStringLiteral("本次注册验证码：%1").arg(devCode));
        } else {
            QMessageBox::information(this, QStringLiteral("服务端验证码"), QStringLiteral("验证码已发送"));
        }
        showMessage(QStringLiteral("已通过服务端获取注册验证码"));
        return;
    }

    if (m_database.phoneExists(phone)) {
        showWarning(QStringLiteral("手机号已绑定账号，请直接登录"));
        return;
    }

    const QString code = generateCode();
    m_registerCodes.insert(phone, code);
    QMessageBox::information(this, QStringLiteral("演示验证码"), QStringLiteral("本次注册验证码：%1").arg(code));
}

void MainWindow::handleRegister()
{
    const QString phone = ui->registerPhoneEdit->text().trimmed();
    const QString code = ui->registerCodeEdit->text().trimmed();
    const QString password = ui->registerPasswordEdit->text();
    QString nickname = ui->registerNicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        nickname = QStringLiteral("user_%1").arg(phone.right(4));
    }

    if (!isPhoneValid(phone)) {
        showWarning(QStringLiteral("请输入合法的11位大陆手机号"));
        return;
    }
    if (m_registerCodes.value(phone) != code) {
        showWarning(QStringLiteral("验证码有误"));
        return;
    }
    if (!isNicknameValid(nickname)) {
        showWarning(QStringLiteral("用户名错误"));
        return;
    }
    if (!isPasswordValid(password)) {
        showWarning(QStringLiteral("密码违规"));
        return;
    }

    QString message;
    std::optional<User> user;
    if (m_serverApi && m_serverApi->ping(&message)) {
        user = m_serverApi->registerUser(phone, password, nickname, code, &message);
        if (user) {
            m_serverMode = true;
        }
    } else {
        user = m_database.registerUser(phone, password, nickname, &message);
        m_serverMode = false;
    }
    if (!user) {
        showWarning(message);
        return;
    }

    QMessageBox::information(this, QStringLiteral("注册成功"), QStringLiteral("注册成功！"));
    enterApplication(*user);
}

void MainWindow::handleLogin()
{
    const QString phone = ui->loginPhoneEdit->text().trimmed();
    const QString password = ui->loginPasswordEdit->text();
    if (!isPhoneValid(phone)) {
        showWarning(QStringLiteral("请输入合法的11位大陆手机号"));
        return;
    }

    QString message;
    std::optional<User> user;
    if (m_serverApi && m_serverApi->ping(&message)) {
        user = m_serverApi->login(phone, password, &message);
        if (user) {
            m_serverMode = true;
        }
    } else {
        user = m_database.login(phone, password, &message);
        m_serverMode = false;
    }
    if (!user) {
        showWarning(message);
        return;
    }

    if (ui->rememberCheck->isChecked() && !m_serverMode) {
        QSettings settings;
        settings.setValue(QStringLiteral("rememberUserId"), user->id);
        settings.setValue(QStringLiteral("rememberUntil"), QDate::currentDate().addDays(7).toString(Qt::ISODate));
    } else if (m_serverMode) {
        QSettings settings;
        settings.remove(QStringLiteral("rememberUserId"));
        settings.remove(QStringLiteral("rememberUntil"));
    }

    enterApplication(*user);
}

void MainWindow::handleSendResetCode()
{
    const QString phone = ui->resetPhoneEdit->text().trimmed();
    if (!isPhoneValid(phone)) {
        showWarning(QStringLiteral("请输入合法的11位大陆手机号"));
        return;
    }

    QString serverMessage;
    if (m_serverApi && m_serverApi->ping(&serverMessage)) {
        QString devCode;
        if (!m_serverApi->requestSmsCode(phone, &devCode, &serverMessage)) {
            showWarning(QStringLiteral("服务端验证码获取失败：%1").arg(serverMessage));
            return;
        }
        if (!devCode.isEmpty()) {
            m_resetCodes.insert(phone, devCode);
            QMessageBox::information(this,
                                     QStringLiteral("服务端验证码"),
                                     QStringLiteral("本次找回密码验证码：%1").arg(devCode));
        } else {
            QMessageBox::information(this, QStringLiteral("服务端验证码"), QStringLiteral("验证码已发送"));
        }
        showMessage(QStringLiteral("已通过服务端获取找回密码验证码"));
        return;
    }

    if (!m_database.phoneExists(phone)) {
        showWarning(QStringLiteral("手机号未绑定账户，需要先注册"));
        return;
    }

    const QString code = generateCode();
    m_resetCodes.insert(phone, code);
    QMessageBox::information(this, QStringLiteral("演示验证码"), QStringLiteral("本次找回密码验证码：%1").arg(code));
}

void MainWindow::handleResetPassword()
{
    const QString phone = ui->resetPhoneEdit->text().trimmed();
    const QString code = ui->resetCodeEdit->text().trimmed();
    const QString password = ui->resetPasswordEdit->text();
    if (m_resetCodes.value(phone) != code) {
        showWarning(QStringLiteral("验证码有误"));
        return;
    }
    if (!isPasswordValid(password)) {
        showWarning(QStringLiteral("密码违规"));
        return;
    }

    QString message;
    bool ok = false;
    if (m_serverApi && m_serverApi->ping(&message)) {
        ok = m_serverApi->resetPasswordByPhone(phone, password, code, &message);
    } else {
        ok = m_database.resetPasswordByPhone(phone, password, &message);
    }
    if (!ok) {
        showWarning(message);
        return;
    }

    QMessageBox::information(this, QStringLiteral("找回密码"), QStringLiteral("密码重置成功，请重新登录"));
    ui->mainStack->setCurrentWidget(ui->loginPage);
}

void MainWindow::loadRememberedUser()
{
    QSettings settings;
    const int userId = settings.value(QStringLiteral("rememberUserId"), -1).toInt();
    const QDate rememberUntil = QDate::fromString(settings.value(QStringLiteral("rememberUntil")).toString(), Qt::ISODate);
    if (userId <= 0 || !rememberUntil.isValid() || rememberUntil < QDate::currentDate()) {
        settings.remove(QStringLiteral("rememberUserId"));
        settings.remove(QStringLiteral("rememberUntil"));
        return;
    }

    const std::optional<User> user = m_database.userById(userId);
    if (user && user->status == QStringLiteral("正常")) {
        enterApplication(*user);
    }
}

void MainWindow::enterApplication(const User &user)
{
    m_currentUser = user;
    m_avatarPath = user.avatarPath;
    showMaximized();
    ui->mainStack->setCurrentWidget(ui->appPage);
    refreshProfile();
    refreshWallet();
    loadStations();
    queryTransactions();
    showMessage(QStringLiteral("%1登录成功").arg(connectionPrefix()));

    QString orderMessage;
    const std::optional<Order> order = usingServer()
                                           ? m_serverApi->unfinishedOrder(&orderMessage)
                                           : m_database.unfinishedOrder(user.id);
    if (order) {
        QMessageBox::information(this, QStringLiteral("未完成订单"), QStringLiteral("您有未完成的充电订单，请先结算"));
        showUnfinishedOrder(*order);
    }
}

void MainWindow::refreshCurrentUser()
{
    if (!m_currentUser) {
        return;
    }
    if (usingServer()) {
        double balance = m_currentUser->balance;
        QString message;
        if (m_serverApi->queryBalance(&balance, &message)) {
            m_currentUser->balance = balance;
            m_currentUser->status = QStringLiteral("正常");
        } else {
            showMessage(QStringLiteral("服务端余额刷新失败：%1").arg(message));
        }
        return;
    }
    if (const std::optional<User> user = m_database.userById(m_currentUser->id)) {
        m_currentUser = *user;
        m_avatarPath = user->avatarPath;
    }
}

void MainWindow::refreshProfile()
{
    refreshCurrentUser();
    if (!m_currentUser) {
        return;
    }

    ui->helloLabel->setText(QStringLiteral("%1（%2）")
                                .arg(m_currentUser->nickname, m_currentUser->phone));
    ui->headerBalanceLabel->setText(QStringLiteral("%1 元")
                                        .arg(m_currentUser->balance, 0, 'f', 2));
    ui->profilePhoneLabel->setText(QStringLiteral("手机号：%1").arg(m_currentUser->phone));
    ui->profileStatusLabel->setText(QStringLiteral("账号状态：%1 · %2")
                                        .arg(m_currentUser->status, connectionPrefix()));
    ui->nicknameEdit->setText(m_currentUser->nickname);
    updateAvatarPreview();
}

void MainWindow::refreshWallet()
{
    refreshCurrentUser();
    if (!m_currentUser) {
        return;
    }
    ui->balanceLabel->setText(QStringLiteral("%1 元").arg(m_currentUser->balance, 0, 'f', 2));
    ui->helloLabel->setText(QStringLiteral("%1（%2）")
                                .arg(m_currentUser->nickname, m_currentUser->phone));
    ui->headerBalanceLabel->setText(QStringLiteral("%1 元")
                                        .arg(m_currentUser->balance, 0, 'f', 2));
}

void MainWindow::updateHeaderSpacing()
{
    const bool isAuthPage = ui->mainStack->currentWidget() != ui->appPage;
    ui->titleLabel->setVisible(!isAuthPage);
    ui->rootLayout->setSpacing(isAuthPage ? 0 : 12);
    ui->rootLayout->setContentsMargins(isAuthPage ? 0 : 16,
                                       isAuthPage ? 0 : 18,
                                       isAuthPage ? 0 : 16,
                                       isAuthPage ? 0 : 14);
}

bool MainWindow::usingServer() const
{
    return m_serverMode && m_serverApi && m_serverApi->hasToken();
}

QString MainWindow::connectionPrefix() const
{
    return usingServer()
               ? QStringLiteral("服务端")
               : QStringLiteral("本地演示");
}

void MainWindow::setProfileEditorVisible(bool visible)
{
    ui->nicknameEdit->setVisible(visible);
    ui->chooseAvatarButton->setVisible(visible);
    ui->saveProfileButton->setVisible(visible);
    ui->cancelProfileButton->setVisible(visible);
    ui->modifyProfileButton->setVisible(!visible);

    if (m_currentUser) {
        m_avatarPath = m_currentUser->avatarPath;
        ui->nicknameEdit->setText(m_currentUser->nickname);
        updateAvatarPreview();
    }
    if (visible) {
        ui->nicknameEdit->setFocus();
    }
}

void MainWindow::setPasswordEditorVisible(bool visible)
{
    ui->oldPasswordEdit->setVisible(visible);
    ui->newPasswordEdit->setVisible(visible);
    ui->confirmPasswordEdit->setVisible(visible);
    ui->confirmPasswordButton->setVisible(visible);
    ui->cancelPasswordButton->setVisible(visible);
    ui->changePasswordButton->setVisible(!visible);

    if (!visible) {
        ui->oldPasswordEdit->clear();
        ui->newPasswordEdit->clear();
        ui->confirmPasswordEdit->clear();
    } else {
        ui->oldPasswordEdit->setFocus();
    }
}

void MainWindow::handleLogout()
{
    QSettings settings;
    settings.remove(QStringLiteral("rememberUserId"));
    settings.remove(QStringLiteral("rememberUntil"));
    if (usingServer()) {
        QString message;
        m_serverApi->logout(&message);
    }
    stopChargingThread();
    m_currentUser.reset();
    m_currentOrderId.clear();
    m_serverMode = false;
    ui->mainStack->setCurrentWidget(ui->loginPage);
    showMessage(QStringLiteral("已注销当前用户"));
}

void MainWindow::loadStations()
{
    QString message;
    if (usingServer()) {
        m_currentStations = m_serverApi->stations(ui->regionCombo->currentText(),
                                                  ui->addressEdit->text(),
                                                  &message);
        if (m_currentStations.isEmpty() && !message.isEmpty()) {
            showMessage(QStringLiteral("服务端电站查询失败：%1").arg(message));
        }
    } else {
        m_currentStations = m_database.stations(ui->regionCombo->currentText(), ui->addressEdit->text());
    }
    ui->stationList->clear();
    for (const Station &station : m_currentStations) {
        const QString text = QStringLiteral("%1\n%2 | %3 元/度 | 空闲 %4/%5\n距离 %6 km | 推荐分 %7")
                                 .arg(station.name)
                                 .arg(station.address)
                                 .arg(station.price, 0, 'f', 2)
                                 .arg(station.idlePiles)
                                 .arg(station.totalPiles)
                                 .arg(station.distance, 0, 'f', 1)
                                 .arg(station.recommendScore);
        auto *item = new QListWidgetItem(QIcon(QStringLiteral(":/resources/icons/station.svg")), text);
        item->setData(Qt::UserRole, station.id);
        ui->stationList->addItem(item);
    }

    if (!m_currentStations.isEmpty()) {
        ui->stationList->setCurrentRow(0);
    } else {
        ui->stationDetailNameLabel->setText(QStringLiteral("没有匹配的充电站"));
        ui->stationDetailInfoLabel->setText(QStringLiteral("请调整区域或地址关键字后重试。"));
        ui->pileTable->setRowCount(0);
    }
}

void MainWindow::showStation(int row)
{
    if (row < 0 || row >= m_currentStations.size()) {
        m_selectedStation.reset();
        return;
    }

    const Station station = m_currentStations.at(row);
    m_selectedStation = station;
    ui->stationDetailNameLabel->setText(station.name);
    ui->stationDetailInfoLabel->setText(QStringLiteral("%1\n电价：%2 元/度  距离：%3 km  在线率：%4%")
                                            .arg(station.address)
                                            .arg(station.price, 0, 'f', 2)
                                            .arg(station.distance, 0, 'f', 1)
                                            .arg(station.onlineRate * 100.0, 0, 'f', 0));
    ui->recommendLabel->setText(QStringLiteral("智能推荐：%1分，优先推荐低拥堵、高空闲率站点").arg(station.recommendScore));
    ui->mapPreviewBrowser->setHtml(QStringLiteral("<h3>%1</h3><p>选择出行方式后点击“路线导航”，系统会生成腾讯地图路线链接。</p>")
                                       .arg(station.name.toHtmlEscaped()));
    populatePiles(station.id);
}

void MainWindow::populatePiles(const QString &stationId)
{
    QString message;
    if (usingServer()) {
        m_currentPiles = m_serverApi->pilesByStation(stationId, &message);
        if (m_currentPiles.isEmpty() && !message.isEmpty()) {
            showMessage(QStringLiteral("服务端电桩查询失败：%1").arg(message));
        }
    } else {
        m_currentPiles = m_database.pilesByStation(stationId);
    }
    ui->pileTable->setRowCount(m_currentPiles.size());
    int firstIdleRow = -1;
    for (int row = 0; row < m_currentPiles.size(); ++row) {
        const Pile &pile = m_currentPiles.at(row);
        const QStringList values = {
            pile.id,
            pile.type,
            pile.status,
            QString::number(pile.power, 'f', 0),
            QString::number(pile.totalTimes),
            QString::number(pile.totalHours, 'f', 1)
        };

        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values.at(col));
            item->setData(Qt::UserRole, pile.id);
            if (pile.status == QStringLiteral("空闲")) {
                item->setForeground(QColor(QStringLiteral("#2dd4bf")));
            } else if (pile.status == QStringLiteral("故障")) {
                item->setForeground(QColor(QStringLiteral("#f87171")));
            }
            ui->pileTable->setItem(row, col, item);
        }
        if (firstIdleRow < 0 && pile.status == QStringLiteral("空闲")) {
            firstIdleRow = row;
        }
    }
    ui->pileTable->resizeColumnsToContents();
    if (firstIdleRow >= 0) {
        ui->pileTable->selectRow(firstIdleRow);
    }
    handlePileSelectionChanged();
}

void MainWindow::handlePileSelectionChanged()
{
    const int row = ui->pileTable->currentRow();
    if (row < 0 || row >= m_currentPiles.size()) {
        m_selectedPile.reset();
        ui->startChargeButton->setEnabled(false);
        return;
    }

    m_selectedPile = m_currentPiles.at(row);
    ui->startChargeButton->setEnabled(m_selectedPile->status == QStringLiteral("空闲"));
}

QString MainWindow::tencentMapUrl(const Station &station) const
{
    QString routeType = QStringLiteral("drive");
    if (ui->routeModeCombo->currentText() == QStringLiteral("步行")) {
        routeType = QStringLiteral("walk");
    } else if (ui->routeModeCombo->currentText() == QStringLiteral("骑行")) {
        routeType = QStringLiteral("bike");
    } else if (ui->routeModeCombo->currentText() == QStringLiteral("公共交通")) {
        routeType = QStringLiteral("bus");
    }

    const QString from = ui->addressEdit->text().trimmed().isEmpty()
                             ? QStringLiteral("当前位置")
                             : ui->addressEdit->text().trimmed();
    return QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan?type=%1&from=%2&to=%3&tocoord=%4,%5&referer=ChargingUserClient")
        .arg(routeType,
             QString::fromUtf8(QUrl::toPercentEncoding(from)),
             QString::fromUtf8(QUrl::toPercentEncoding(station.name)),
             QString::number(station.latitude, 'f', 6),
             QString::number(station.longitude, 'f', 6));
}

void MainWindow::openNavigation()
{
    if (!m_selectedStation) {
        showWarning(QStringLiteral("请先选择充电站"));
        return;
    }

    const QString url = tencentMapUrl(*m_selectedStation);
    ui->mapPreviewBrowser->setHtml(QStringLiteral("<h3>%1</h3><p>路线方式：%2</p><p><a href=\"%3\">打开腾讯地图路线规划</a></p>")
                                       .arg(m_selectedStation->name.toHtmlEscaped(),
                                            ui->routeModeCombo->currentText().toHtmlEscaped(),
                                            url.toHtmlEscaped()));
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::prepareCharge()
{
    if (!m_currentUser) {
        showWarning(QStringLiteral("请先登录"));
        return;
    }
    QString orderMessage;
    const std::optional<Order> unfinished = usingServer()
                                                ? m_serverApi->unfinishedOrder(&orderMessage)
                                                : m_database.unfinishedOrder(m_currentUser->id);
    if (unfinished) {
        QMessageBox::information(this, QStringLiteral("未完成订单"), QStringLiteral("您有未完成的充电订单，请先结算"));
        showUnfinishedOrder(*unfinished);
        return;
    }
    if (!m_selectedStation || !m_selectedPile || m_selectedPile->status != QStringLiteral("空闲")) {
        showWarning(QStringLiteral("请选择一个空闲电桩"));
        return;
    }

    m_currentOrderId.clear();
    m_currentChargeKwh = 0.0;
    m_currentChargeCost = 0.0;
    ui->chargeInfoLabel->setText(QStringLiteral("已选择：%1 / %2 / %3 / %4 kW，电价 %5 元/度")
                                     .arg(m_selectedStation->name,
                                          m_selectedPile->id,
                                          m_selectedPile->type,
                                          QString::number(m_selectedPile->power, 'f', 0),
                                          QString::number(m_selectedStation->price, 'f', 2)));
    ui->chargeProgressBar->setValue(0);
    ui->chargeKwhLabel->setText(QStringLiteral("已充电量：0.00 度"));
    ui->chargeCostLabel->setText(QStringLiteral("预估费用：0.00 元"));
    ui->beginChargeButton->setEnabled(true);
    ui->faultButton->setEnabled(false);
    ui->settleButton->setEnabled(false);
    ui->appTabs->setCurrentWidget(ui->chargeTab);
}

void MainWindow::beginCharge()
{
    if (!m_currentUser || !m_selectedStation || !m_selectedPile) {
        showWarning(QStringLiteral("请先选择空闲电桩"));
        return;
    }

    QString message;
    double estimatedCost = 0.0;
    const QString orderId = usingServer()
                                ? m_serverApi->startCharge(m_selectedPile->id,
                                                           ui->targetKwhSpin->value(),
                                                           &estimatedCost,
                                                           &message)
                                : m_database.createChargingOrder(m_currentUser->id, m_selectedPile->id, &message);
    if (orderId.isEmpty()) {
        showWarning(message);
        const std::optional<Order> order = usingServer()
                                               ? m_serverApi->unfinishedOrder(&message)
                                               : m_database.unfinishedOrder(m_currentUser->id);
        if (order) {
            showUnfinishedOrder(*order);
        }
        return;
    }

    m_currentOrderId = orderId;
    m_currentChargeKwh = 0.0;
    m_currentChargeCost = 0.0;
    stopChargingThread();

    // 多线程：充电进度放在工作线程中模拟，界面通过信号接收进度刷新。
    m_chargeThread = new QThread(this);
    m_chargeWorker = new ChargingWorker();
    m_chargeWorker->moveToThread(m_chargeThread);
    connect(m_chargeThread, &QThread::finished, m_chargeWorker, &QObject::deleteLater);
    connect(m_chargeWorker, &ChargingWorker::progressChanged, this, &MainWindow::onChargeProgress);
    connect(m_chargeWorker, &ChargingWorker::finished, this, &MainWindow::onChargeFinished);
    connect(m_chargeWorker, &ChargingWorker::finished, m_chargeThread, &QThread::quit);
    connect(m_chargeThread, &QThread::finished, this, [this]() {
        m_chargeThread->deleteLater();
        m_chargeThread = nullptr;
        m_chargeWorker = nullptr;
    });
    m_chargeThread->start();

    ui->beginChargeButton->setEnabled(false);
    ui->faultButton->setEnabled(true);
    ui->settleButton->setEnabled(false);
    showMessage(QStringLiteral("%1充电已开始").arg(connectionPrefix()));

    const double targetKwh = ui->targetKwhSpin->value();
    const double unitPrice = usingServer() && estimatedCost > 0.0 && targetKwh > 0.0
                                 ? estimatedCost / targetKwh
                                 : m_selectedStation->price;
    QMetaObject::invokeMethod(m_chargeWorker,
                              "start",
                              Qt::QueuedConnection,
                              Q_ARG(QString, orderId),
                              Q_ARG(QString, m_selectedPile->id),
                              Q_ARG(double, targetKwh),
                              Q_ARG(double, unitPrice),
                              Q_ARG(double, m_selectedPile->power));
}

void MainWindow::onChargeProgress(double kwh, double cost, int percent)
{
    m_currentChargeKwh = kwh;
    m_currentChargeCost = cost;
    ui->chargeProgressBar->setValue(percent);
    ui->chargeKwhLabel->setText(QStringLiteral("已充电量：%1 度").arg(kwh, 0, 'f', 2));
    ui->chargeCostLabel->setText(QStringLiteral("预估费用：%1 元").arg(cost, 0, 'f', 2));
}

void MainWindow::onChargeFinished(const QString &orderId, double kwh, double cost, bool abnormal, const QString &message)
{
    QString dbMessage;
    if (usingServer()) {
        double serverKwh = kwh;
        double serverCost = cost;
        double newBalance = 0.0;
        if (!m_serverApi->stopCharge(orderId, &serverKwh, &serverCost, &newBalance, &dbMessage)) {
            showWarning(dbMessage);
            return;
        }
        m_currentChargeKwh = serverKwh;
        m_currentChargeCost = serverCost;
        if (m_currentUser) {
            m_currentUser->balance = newBalance;
        }
        ui->chargeProgressBar->setValue(100);
        ui->chargeKwhLabel->setText(QStringLiteral("已充电量：%1 度").arg(serverKwh, 0, 'f', 2));
        ui->chargeCostLabel->setText(QStringLiteral("已扣费用：%1 元").arg(serverCost, 0, 'f', 2));
        ui->chargeInfoLabel->setText(QStringLiteral("%1\n服务端订单号：%2").arg(message, orderId));
        m_currentOrderId.clear();
        ui->faultButton->setEnabled(false);
        ui->settleButton->setEnabled(false);
        refreshWallet();
        queryTransactions();
        loadStations();
        showMessage(QStringLiteral("服务端已完成充电并结算"));
        Q_UNUSED(abnormal)
        return;
    }

    if (!m_database.completeChargingOrder(orderId, kwh, cost, abnormal, &dbMessage)) {
        showWarning(dbMessage);
        return;
    }

    ui->faultButton->setEnabled(false);
    ui->settleButton->setEnabled(true);
    ui->chargeInfoLabel->setText(QStringLiteral("%1\n订单号：%2").arg(message, orderId));
    loadStations();
    showMessage(message);
}

void MainWindow::simulateFault()
{
    if (!m_chargeWorker) {
        showWarning(QStringLiteral("当前没有正在进行的充电任务"));
        return;
    }
    QMetaObject::invokeMethod(m_chargeWorker, "stopFault", Qt::QueuedConnection);
}

void MainWindow::showUnfinishedOrder(const Order &order)
{
    Order displayOrder = order;
    if (!usingServer() && displayOrder.status == QStringLiteral("充电中")) {
        QString message;
        m_database.completeChargingOrder(displayOrder.id, 1.0, 1.28, false, &message);
        if (const std::optional<Order> refreshed = m_database.unfinishedOrder(displayOrder.userId)) {
            displayOrder = *refreshed;
        }
    }

    m_currentOrderId = displayOrder.id;
    m_currentChargeKwh = displayOrder.kwh;
    m_currentChargeCost = displayOrder.cost;
    ui->chargeInfoLabel->setText(QStringLiteral("未完成订单：%1，状态：%2，请先结算")
                                     .arg(displayOrder.id, displayOrder.status));
    ui->chargeProgressBar->setValue(100);
    ui->chargeKwhLabel->setText(QStringLiteral("已充电量：%1 度").arg(displayOrder.kwh, 0, 'f', 2));
    ui->chargeCostLabel->setText(QStringLiteral("待支付费用：%1 元").arg(displayOrder.cost, 0, 'f', 2));
    ui->beginChargeButton->setEnabled(false);
    ui->faultButton->setEnabled(false);
    ui->settleButton->setEnabled(true);
    ui->appTabs->setCurrentWidget(ui->chargeTab);
}

void MainWindow::settleCurrentOrder()
{
    if (m_currentOrderId.isEmpty()) {
        showWarning(QStringLiteral("当前没有可结算订单"));
        return;
    }

    QString message;
    double newBalance = 0.0;
    double chargedKwh = 0.0;
    double cost = 0.0;
    const bool ok = usingServer()
                        ? m_serverApi->stopCharge(m_currentOrderId, &chargedKwh, &cost, &newBalance, &message)
                        : m_database.settleOrder(m_currentOrderId, &newBalance, &message);
    if (!ok) {
        showWarning(message);
        if (message.contains(QStringLiteral("余额不足"))) {
            ui->appTabs->setCurrentWidget(ui->walletTab);
        }
        return;
    }

    QMessageBox::information(this,
                             QStringLiteral("结算成功"),
                             QStringLiteral("结算成功，剩余余额 %1 元").arg(newBalance, 0, 'f', 2));
    if (m_currentUser) {
        m_currentUser->balance = newBalance;
    }
    m_currentOrderId.clear();
    ui->settleButton->setEnabled(false);
    ui->beginChargeButton->setEnabled(m_selectedPile && m_selectedPile->status == QStringLiteral("空闲"));
    refreshWallet();
    queryTransactions();
    loadStations();
}

void MainWindow::handleRecharge()
{
    if (!m_currentUser) {
        showWarning(QStringLiteral("请先登录"));
        return;
    }

    QString message;
    double balance = 0.0;
    const double amount = ui->rechargeAmountSpin->value();
    const bool ok = usingServer()
                        ? m_serverApi->recharge(amount, &balance, &message)
                        : m_database.recharge(m_currentUser->id, amount, &balance, &message);
    if (!ok) {
        showWarning(message);
        return;
    }

    QMessageBox::information(this,
                             QStringLiteral("充值成功"),
                             QStringLiteral("充值成功，已到账 %1 元").arg(amount, 0, 'f', 2));
    if (m_currentUser) {
        m_currentUser->balance = balance;
    }
    refreshWallet();
    queryTransactions();
}

void MainWindow::queryTransactions()
{
    if (!m_currentUser) {
        return;
    }

    QString message;
    const QVector<Transaction> records = usingServer()
                                             ? m_serverApi->transactions(ui->fromDateEdit->date(),
                                                                        ui->toDateEdit->date(),
                                                                        &message)
                                             : m_database.transactions(m_currentUser->id,
                                                                       ui->fromDateEdit->date(),
                                                                       ui->toDateEdit->date());
    if (records.isEmpty() && usingServer() && !message.isEmpty()) {
        showMessage(QStringLiteral("服务端流水查询失败：%1").arg(message));
    }
    ui->transactionsTable->setRowCount(records.size());
    for (int row = 0; row < records.size(); ++row) {
        const Transaction &record = records.at(row);
        const QStringList values = {
            record.type,
            QStringLiteral("%1 元").arg(record.amount, 0, 'f', 2),
            record.happenedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            record.note
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values.at(col));
            if (record.amount < 0) {
                item->setForeground(QColor(QStringLiteral("#f87171")));
            } else {
                item->setForeground(QColor(QStringLiteral("#2dd4bf")));
            }
            ui->transactionsTable->setItem(row, col, item);
        }
    }
    if (records.isEmpty()) {
        showMessage(QStringLiteral("暂无流水记录"));
    }
    ui->transactionsTable->resizeRowsToContents();
    adjustTransactionTableColumns();
}

void MainWindow::chooseAvatar()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                     QStringLiteral("选择头像"),
                                                     QString(),
                                                     QStringLiteral("JPG头像 (*.jpg *.jpeg);;图片文件 (*.png *.bmp)"));
    if (path.isEmpty()) {
        return;
    }
    m_avatarPath = path;
    updateAvatarPreview();
}

void MainWindow::saveProfile()
{
    if (!m_currentUser) {
        return;
    }
    const QString nickname = ui->nicknameEdit->text().trimmed();
    if (!isNicknameValid(nickname)) {
        showWarning(QStringLiteral("用户名错误"));
        return;
    }

    QString message;
    if (!usingServer() && !m_database.updateProfile(m_currentUser->id, nickname, m_avatarPath, &message)) {
        showWarning(message);
        return;
    }
    if (usingServer()) {
        m_currentUser->nickname = nickname;
        m_currentUser->avatarPath = m_avatarPath;
    }
    refreshProfile();
    setProfileEditorVisible(false);
    showMessage(QStringLiteral("个人资料已保存"));
}

void MainWindow::changePassword()
{
    if (!m_currentUser) {
        return;
    }

    const QString oldPassword = ui->oldPasswordEdit->text();
    const QString newPassword = ui->newPasswordEdit->text();
    const QString confirmPassword = ui->confirmPasswordEdit->text();
    if (!isPasswordValid(newPassword)) {
        showWarning(QStringLiteral("密码违规"));
        return;
    }
    if (newPassword != confirmPassword) {
        showWarning(QStringLiteral("两次输入的新密码不一致"));
        return;
    }

    QString message;
    const bool ok = usingServer()
                        ? m_serverApi->changePassword(oldPassword, newPassword, &message)
                        : m_database.changePassword(m_currentUser->id, oldPassword, newPassword, &message);
    if (!ok) {
        showWarning(message);
        return;
    }
    setPasswordEditorVisible(false);
    QMessageBox::information(this, QStringLiteral("修改密码"), QStringLiteral("密码修改成功"));
}

void MainWindow::updateAvatarPreview()
{
    ui->avatarLabel->setPixmap(avatarPixmap(ui->avatarLabel->size(), m_avatarPath));
    ui->headerAvatarLabel->setPixmap(avatarPixmap(ui->headerAvatarLabel->size(), m_avatarPath));
}

void MainWindow::stopChargingThread()
{
    if (!m_chargeThread) {
        return;
    }
    m_chargeThread->quit();
    m_chargeThread->wait(1200);
    m_chargeThread = nullptr;
    m_chargeWorker = nullptr;
}
