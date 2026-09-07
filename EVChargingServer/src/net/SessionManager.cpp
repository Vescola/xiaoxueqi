#include "net/sessionmanager.h"

#include "common/appconfig.h"

#include <QUuid>

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
}

QString SessionManager::createToken(int userId)
{
    // 用 UUID 去横杠作为 token, 足够随机
    QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);

    Session s;
    s.userId = userId;
    s.expire = QDateTime::currentDateTime()
                   .addDays(AppConfig::instance().tokenValidDays());
    m_sessions.insert(token, s);
    return token;
}

int SessionManager::validateToken(const QString &token) const
{
    if (token.isEmpty()) {
        return -1;
    }
    auto it = m_sessions.constFind(token);
    if (it == m_sessions.constEnd()) {
        return -1;
    }
    if (it->expire < QDateTime::currentDateTime()) {
        return -1;
    }
    return it->userId;
}

void SessionManager::revokeToken(const QString &token)
{
    m_sessions.remove(token);
}

void SessionManager::revokeAllForUser(int userId)
{
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if (it->userId == userId) {
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

int SessionManager::purgeExpired()
{
    const QDateTime now = QDateTime::currentDateTime();
    int count = 0;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if (it->expire < now) {
            it = m_sessions.erase(it);
            ++count;
        } else {
            ++it;
        }
    }
    return count;
}
