#include "connection_pool.h"
#include "core/log_manager.h"
#include <QMutexLocker>

ConnectionPool::ConnectionPool(int maxSize)
    : m_maxSize(maxSize)
{
}

ConnectionPool::~ConnectionPool()
{
    QMutexLocker locker(&m_mutex);
    for (QNetworkAccessManager* nam : m_idle) {
        delete nam;
    }
    m_idle.clear();

    for (QNetworkAccessManager* nam : m_active) {
        delete nam;
    }
    m_active.clear();
}

QNetworkAccessManager* ConnectionPool::acquire()
{
    QMutexLocker locker(&m_mutex);

    if (!m_enabled) {
        auto* nam = new QNetworkAccessManager;
        m_active.insert(nam);
        return nam;
    }

    // Prefer returning an idle connection to avoid creating new TCP/TLS sessions
    if (!m_idle.isEmpty()) {
        QNetworkAccessManager* nam = m_idle.dequeue();
        m_active.insert(nam);
        LOG_DEBUG(QStringLiteral("ConnectionPool: reused idle connection (active=%1, idle=%2)")
                      .arg(m_active.size())
                      .arg(m_idle.size()));
        return nam;
    }

    // No idle connections: create a new one
    if (m_active.size() >= m_maxSize) {
        LOG_WARNING(QStringLiteral("ConnectionPool: max pool size %1 exceeded, "
                                   "creating overflow connection (active=%2)")
                        .arg(m_maxSize)
                        .arg(m_active.size()));
    }

    auto* nam = new QNetworkAccessManager;
    m_active.insert(nam);
    LOG_DEBUG(QStringLiteral("ConnectionPool: created new connection (active=%1, idle=%2)")
                  .arg(m_active.size())
                  .arg(m_idle.size()));
    return nam;
}

void ConnectionPool::release(QNetworkAccessManager* nam)
{
    QMutexLocker locker(&m_mutex);
    if (!nam) {
        return;
    }

    if (!m_active.remove(nam)) {
        LOG_WARNING(QStringLiteral("ConnectionPool: release called on untracked NAM, deleting"));
        delete nam;
        return;
    }

    if (!m_enabled) {
        delete nam;
        return;
    }

    // If we are over capacity, destroy instead of returning to idle pool
    int totalAfterReturn = m_idle.size() + m_active.size() + 1;
    if (totalAfterReturn > m_maxSize) {
        LOG_DEBUG(QStringLiteral("ConnectionPool: discarding overflow connection "
                                 "(total would be %1, max=%2)")
                      .arg(totalAfterReturn)
                      .arg(m_maxSize));
        delete nam;
    } else {
        m_idle.enqueue(nam);
        LOG_DEBUG(QStringLiteral("ConnectionPool: returned connection to idle pool "
                                 "(active=%1, idle=%2)")
                      .arg(m_active.size())
                      .arg(m_idle.size()));
    }
}

void ConnectionPool::clear()
{
    QMutexLocker locker(&m_mutex);
    for (QNetworkAccessManager* nam : m_idle) {
        delete nam;
    }
    m_idle.clear();

    for (QNetworkAccessManager* nam : m_active) {
        delete nam;
    }
    m_active.clear();

    LOG_DEBUG(QStringLiteral("ConnectionPool: all connections cleared"));
}

void ConnectionPool::resize(int maxSize)
{
    QMutexLocker locker(&m_mutex);
    m_maxSize = maxSize;

    // Shrink idle pool if new max is smaller
    while (m_idle.size() + m_active.size() > m_maxSize && !m_idle.isEmpty()) {
        delete m_idle.dequeue();
    }
    LOG_DEBUG(QStringLiteral("ConnectionPool: resized to max=%1 (active=%2, idle=%3)")
                  .arg(m_maxSize)
                  .arg(m_active.size())
                  .arg(m_idle.size()));
}

void ConnectionPool::setEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enabled;
    if (!m_enabled) {
        while (!m_idle.isEmpty()) {
            delete m_idle.dequeue();
        }
    }
}

bool ConnectionPool::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

int ConnectionPool::activeCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_active.size();
}

int ConnectionPool::idleCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_idle.size();
}
