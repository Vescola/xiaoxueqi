#ifndef SERVERCORE_H
#define SERVERCORE_H

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QVariantMap>
#include <QList>

class QTcpServer;
class QTcpSocket;
class ClientSession;
class SessionManager;

// ---------------------------------------------------------------------------
// 后台业务核心(子线程)
//
//   - 在 main 中启动, 常驻运行, 与管理员 GUI 是否登录没有任何因果;
//   - 单子线程 + 异步 I/O(Reactor): 一个用户端对应一个 socket,
//     所有 socket 的 readyRead 绑定到同一处理流程;
//   - 数据库操作、token 会话、业务路由都在本类完成。
//
// 本类通过 moveToThread 放到工作线程; GUI 通过信号/槽(队列连接)与它通信。
// ---------------------------------------------------------------------------

class ServerCore : public QObject
{
    Q_OBJECT
public:
    explicit ServerCore(QObject *parent = nullptr);
    ~ServerCore() override;

    bool start();
    void stop();

    quint16 port() const;
    int     connectionCount() const;

signals:
    void logMessage(const QString &msg);
    void connectionCountChanged(int count);

    // 管理员登录(跨线程): GUI 调用 requestAdminLogin, 结果由此信号回传
    void adminLoginFinished(int requestId, bool ok);

public slots:
    // 管理员登录请求(GUI 线程 -> 本线程)
    void requestAdminLogin(int requestId, const QString &username,
                           const QString &password);

private slots:
    void onNewConnection();
    void onPacketReceived(ClientSession *session, quint16 cmd,
                          const QVariantMap &params, quint32 requestId);
    void onSessionDisconnected(ClientSession *session);

private:
    // ---- 账户管理 ----
    void handleSmsCode(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleRegister(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleLogin(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleLogout(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleFindPwd(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleChangePwd(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleRecharge(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleBalanceQuery(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleTransaction(ClientSession *s, const QVariantMap &p, quint32 rid);

    // ---- 充电站/桩查询 ----
    void handleStations(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleStationDetail(ClientSession *s, const QVariantMap &p, quint32 rid);

    // ---- 充电业务 ----
    void handleCheckOrder(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleStartCharge(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleStopCharge(ClientSession *s, const QVariantMap &p, quint32 rid);
    void handleChargeStatus(ClientSession *s, const QVariantMap &p, quint32 rid);

    // ---- 心跳 ----
    void handleHeartbeat(ClientSession *s, const QVariantMap &p, quint32 rid);

    // ---- 公共 ----
    // 从 params 取 token 校验, 有效返回 userId, 无效返回 -1(并已回错误响应)
    int requireLogin(ClientSession *s, const QVariantMap &p, quint32 rid);
    void reply(ClientSession *s, quint16 cmd, const QVariantMap &p, quint32 rid);
    void replyError(ClientSession *s, quint32 rid, int code, const QString &msg);

    QString generateSmsCode(const QString &phone);
    bool verifySmsCode(const QString &phone, const QString &code);

    QTcpServer     *m_server = nullptr;
    SessionManager  *m_sessions = nullptr;
    QList<ClientSession*> m_clients;

    // 短信验证码(内存, 不落库): phone -> (code, expire)
    struct SmsEntry { QString code; QDateTime expire; };
    QHash<QString, SmsEntry> m_sms;
};

#endif // SERVERCORE_H
