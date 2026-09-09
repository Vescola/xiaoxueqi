#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "databasemanager.h"
#include "models.h"

#include <QMainWindow>
#include <QHash>
#include <QVector>

#include <optional>

class ChargingWorker;
class MouseTrailWidget;
class ServerApiClient;
class QCloseEvent;
class QResizeEvent;
class QThread;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void handleLogin();
    void handleRegister();
    void handleSendRegisterCode();
    void handleSendResetCode();
    void handleResetPassword();
    void handleLogout();

    void loadStations();
    void showStation(int row);
    void handlePileSelectionChanged();
    void openNavigation();
    void prepareCharge();
    void beginCharge();
    void simulateFault();
    void settleCurrentOrder();

    void handleRecharge();
    void queryTransactions();
    void chooseAvatar();
    void saveProfile();
    void changePassword();

    void onChargeProgress(double kwh, double cost, int percent);
    void onChargeFinished(const QString &orderId, double kwh, double cost, bool abnormal, const QString &message);

private:
    void setupTables();
    void adjustTransactionTableColumns();
    void setupConnections();
    void loadRememberedUser();
    void enterApplication(const User &user);
    void refreshCurrentUser();
    void refreshProfile();
    void refreshWallet();
    void updateHeaderSpacing();
    bool usingServer() const;
    QString connectionPrefix() const;
    void setProfileEditorVisible(bool visible);
    void setPasswordEditorVisible(bool visible);
    void updateAvatarPreview();
    void populatePiles(const QString &stationId);
    void showUnfinishedOrder(const Order &order);
    void stopChargingThread();
    void showMessage(const QString &message);
    void showWarning(const QString &message);

    bool isPhoneValid(const QString &phone) const;
    bool isPasswordValid(const QString &password) const;
    bool isNicknameValid(const QString &nickname) const;
    QString generateCode() const;
    QString tencentMapUrl(const Station &station) const;

    Ui::MainWindow *ui = nullptr;
    DatabaseManager m_database;
    ServerApiClient *m_serverApi = nullptr;
    bool m_serverMode = false;
    std::optional<User> m_currentUser;
    QVector<Station> m_currentStations;
    QVector<Pile> m_currentPiles;
    std::optional<Station> m_selectedStation;
    std::optional<Pile> m_selectedPile;
    QHash<QString, QString> m_registerCodes;
    QHash<QString, QString> m_resetCodes;
    QString m_avatarPath;

    QThread *m_chargeThread = nullptr;
    ChargingWorker *m_chargeWorker = nullptr;
    MouseTrailWidget *m_mouseTrail = nullptr;
    QString m_currentOrderId;
    double m_currentChargeKwh = 0.0;
    double m_currentChargeCost = 0.0;
};

#endif // MAINWINDOW_H
