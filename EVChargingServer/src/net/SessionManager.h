#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QDateTime>

// ---------------------------------------------------------------------------
// 登录会话管理(协议 7.2)
//
// 登录成功后服务端返回 token, 后续业务请求(除登录/注册/验证码外)必须携带。
// token 只存在于服务器内存, 不落库(数据库端无会话表, 且决策为不改表)。
// token 带过期时间(默认 7 天), 登出时立即失效。
// ---------------------------------------------------------------------------

class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);

    // 为某用户签发 token
    QString createToken(int userId);

    // 校验 token, 有效返回 userId, 无效/过期返回 -1
    int validateToken(const QString &token) const;

    // 使 token 失效(登出)
    void revokeToken(const QString &token);

    // 使某用户的所有 token 失效
    void revokeAllForUser(int userId);

    // 清理过期会话, 返回清理数量
    int purgeExpired();

private:
    struct Session {
        int      userId;
        QDateTime expire;
    };
    QHash<QString, Session> m_sessions;
};

#endif // SESSIONMANAGER_H
