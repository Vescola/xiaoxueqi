#include "net/ClientSession.h"

#include "common/Protocol.h"

#include <QTcpSocket>

ClientSession::ClientSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);   // 会话对象接管 socket 生命周期

    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientSession::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ClientSession::onDisconnected);
}

ClientSession::~ClientSession()
{
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
    }
}

void ClientSession::onReadyRead()
{
    // 追加到缓冲
    m_buffer.append(m_socket->readAll());
    processBuffer();
}

void ClientSession::processBuffer()
{
    quint16 cmd = 0;
    QVariantMap params;
    quint32 reqId = 0;

    while (Protocol::tryParseMessage(m_buffer, cmd, params, reqId)) {
        emit packetReceived(this, cmd, params, reqId);
    }
}

void ClientSession::onDisconnected()
{
    emit disconnected(this);
}

void ClientSession::send(quint16 cmd, const QVariantMap &params, quint32 requestId)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    const QByteArray packet = Protocol::packMessage(cmd, params, requestId);
    m_socket->write(packet);
}

void ClientSession::sendError(quint32 requestId, int code, const QString &message)
{
    QVariantMap p;
    p.insert("code", code);
    p.insert("message", message);
    send(Protocol::Cmd::ERROR_RESP, p, requestId);
}
