#include "qt_executor.h"
#include <QEventLoop>
#include <QTimer>
#include <QNetworkReply>
#include <QUrl>

QtExecutor::QtExecutor(ConnectionPool& pool, const QSslConfiguration& sslConfig)
    : m_pool(pool)
    , m_sslConfig(sslConfig)
{
}

QNetworkRequest QtExecutor::buildQtRequest(const ProviderRequest& request) const {
    QNetworkRequest req{QUrl{request.url}};
    req.setSslConfiguration(m_sslConfig);

    for (auto it = request.headers.constBegin(); it != request.headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    if (!req.hasRawHeader("Content-Type"))
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    req.setTransferTimeout(m_requestTimeout);
    return req;
}

std::optional<DomainFailure> QtExecutor::checkConnectionError(QNetworkReply* reply) const {
    if (!reply) return DomainFailure::internal("null reply");
    if (reply->error() == QNetworkReply::NoError) return std::nullopt;

    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401 || status == 403)
        return DomainFailure::unauthorized(reply->errorString());
    if (status == 429)
        return DomainFailure::rateLimited(reply->errorString());
    if (status >= 500)
        return DomainFailure::unavailable(reply->errorString());
    if (reply->error() == QNetworkReply::TimeoutError)
        return DomainFailure::timeout(reply->errorString());

    return DomainFailure::internal(reply->errorString());
}

Result<ProviderResponse> QtExecutor::execute(const ProviderRequest& request) {
    auto* nam = m_pool.acquire();
    QNetworkRequest req = buildQtRequest(request);

    const QString method = request.method.trimmed().toUpper();
    QNetworkReply* reply = nullptr;
    if (method == "POST")
        reply = nam->post(req, request.body);
    else if (method == "GET")
        reply = nam->get(req);
    else if (method == "PUT")
        reply = nam->put(req, request.body);
    else if (method == "DELETE")
        reply = nam->deleteResource(req);
    else
        reply = nam->sendCustomRequest(req, method.toUtf8(), request.body);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(m_requestTimeout);
    loop.exec();

    if (reply->isRunning()) {
        reply->abort();
        m_pool.release(nam);
        reply->deleteLater();
        return std::unexpected(DomainFailure::timeout("request timeout"));
    }

    auto err = checkConnectionError(reply);
    if (err) {
        m_pool.release(nam);
        reply->deleteLater();
        return std::unexpected(*err);
    }

    ProviderResponse resp;
    resp.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    resp.body = reply->readAll();
    resp.adapterHint = request.adapterHint;
    for (const auto& header : reply->rawHeaderList())
        resp.headers[QString::fromUtf8(header)] = QString::fromUtf8(reply->rawHeader(header));

    reply->deleteLater();
    m_pool.release(nam);
    return resp;
}

Result<QNetworkReply*> QtExecutor::connectStream(const ProviderRequest& request) {
    auto* nam = m_pool.acquire();
    QNetworkRequest req = buildQtRequest(request);

    QNetworkReply* reply = nam->post(req, request.body);

    QEventLoop loop;
    bool gotData = false;
    bool gotError = false;

    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
        gotData = true;
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::errorOccurred, &loop, [&]() {
        gotError = true;
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        loop.quit();
    });
    timeoutTimer.start(m_connectionTimeout);
    loop.exec();

    if (gotError) {
        auto err = checkConnectionError(reply);
        reply->abort();
        reply->deleteLater();
        m_pool.release(nam);
        return std::unexpected(err.value_or(DomainFailure::internal("connection failed")));
    }

    if (!gotData && reply->isRunning()) {
        reply->abort();
        reply->deleteLater();
        m_pool.release(nam);
        return std::unexpected(DomainFailure::timeout("connection timeout"));
    }

    m_streamManagers.insert(reply, nam);
    QObject::connect(reply, &QObject::destroyed, [this, reply]() {
        auto it = m_streamManagers.find(reply);
        if (it != m_streamManagers.end()) {
            m_pool.release(it.value());
            m_streamManagers.erase(it);
        }
    });

    return reply;
}
