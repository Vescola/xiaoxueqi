#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>

class QTcpSocket;

// ---------------------------------------------------------------------------
// 单条客户端连接
// 本类负责该 socket 的粘包缓冲与解包, 解出完整
// 命令后通过信号抛给业务核心统一路由。
// ---------------------------------------------------------------------------

class ClientSession : public QObject
{
    Q_OBJECT
public:
    explicit ClientSession(QTcpSocket *socket, QObject *parent = nullptr);
    ~ClientSession() override;

    QTcpSocket* socket() const { return m_socket; }

    // 发送一条命令响应(自动封包)
    void send(quint16 cmd, const QVariantMap &params, quint32 requestId);
    // 发送通用错误响应
    void sendError(quint32 requestId, int code, const QString &message);

    // 会话上下文
    int     userId() const        { return m_userId; }
    void    setUserId(int id)     { m_userId = id; }
    QString token() const         { return m_token; }
    void    setToken(const QString &t) { m_token = t; }

signals:
    // 解析出一个完整命令包
    void packetReceived(ClientSession *session, quint16 cmd,
                        const QVariantMap &params, quint32 requestId);
    // 连接断开
    void disconnected(ClientSession *session);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void processBuffer();

    QTcpSocket *m_socket;
    QByteArray  m_buffer;
    int         m_userId = -1;
    QString     m_token;
};

#endif // CLIENTSESSION_H
