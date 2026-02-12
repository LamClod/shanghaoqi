#pragma once
#include <QNetworkAccessManager>
#include <QQueue>
#include <QSet>
#include <QMutex>

class ConnectionPool {
public:
    explicit ConnectionPool(int maxSize = 10);
    ~ConnectionPool();

    QNetworkAccessManager* acquire();
    void release(QNetworkAccessManager* nam);
    void clear();
    void resize(int maxSize);
    void setEnabled(bool enabled);
    bool isEnabled() const;
    int activeCount() const;
    int idleCount() const;

private:
    int m_maxSize;
    bool m_enabled = true;
    QQueue<QNetworkAccessManager*> m_idle;
    QSet<QNetworkAccessManager*> m_active;
    mutable QMutex m_mutex;
};
