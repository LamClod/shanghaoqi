#pragma once
#include "semantic/ports.h"
#include "proxy/connection_pool.h"
#include <QSslConfiguration>

class QtExecutor : public IExecutor {
public:
    explicit QtExecutor(ConnectionPool& pool, const QSslConfiguration& sslConfig);

    Result<ProviderResponse> execute(const ProviderRequest& request) override;
    Result<QNetworkReply*> connectStream(const ProviderRequest& request) override;

    void setRequestTimeout(int ms) { m_requestTimeout = ms; }
    void setConnectionTimeout(int ms) { m_connectionTimeout = ms; }

private:
    ConnectionPool& m_pool;
    QSslConfiguration m_sslConfig;
    int m_requestTimeout = 120000;
    int m_connectionTimeout = 30000;

    QMap<QNetworkReply*, QNetworkAccessManager*> m_streamManagers;

    QNetworkRequest buildQtRequest(const ProviderRequest& request) const;
    std::optional<DomainFailure> checkConnectionError(QNetworkReply* reply) const;
};
