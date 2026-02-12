#include "proxy_server.h"
#include "sse_writer.h"
#include "core/log_manager.h"
#include "config/model_list_request_builder.h"
#include "config/provider_routing.h"

#include <QSslConfiguration>
#include <QSslKey>
#include <QSslCertificate>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTcpServer>
#include <QUrl>
#include "adapters/executor/qt_executor.h"

namespace {

QString normalizeModelId(const QJsonObject& modelObj)
{
    QString modelId = modelObj.value(QStringLiteral("id")).toString();
    if (modelId.isEmpty()) {
        modelId = modelObj.value(QStringLiteral("name")).toString();
    }
    if (modelId.startsWith(QStringLiteral("models/"))) {
        modelId = modelId.mid(7);
    }
    return modelId;
}

QByteArray normalizeModelListBody(const QByteArray& rawBody,
                                  bool preferAnthropicSchema)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(rawBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return rawBody;
    }

    const QJsonObject root = doc.object();
    QJsonArray normalizedData;
    QStringList seenIds;

    auto appendModelsFromArray = [&](const QJsonArray& source) {
        for (const QJsonValue& item : source) {
            if (item.isString()) {
                const QString id = item.toString().trimmed();
                if (id.isEmpty() || seenIds.contains(id)) {
                    continue;
                }
                seenIds.append(id);

                QJsonObject model;
                model[QStringLiteral("id")] = id;
                model[QStringLiteral("object")] = QStringLiteral("model");
                normalizedData.append(model);
                continue;
            }

            if (!item.isObject()) {
                continue;
            }

            QJsonObject model = item.toObject();
            const QString id = normalizeModelId(model);
            if (id.isEmpty() || seenIds.contains(id)) {
                continue;
            }

            seenIds.append(id);
            model[QStringLiteral("id")] = id;
            if (!model.contains(QStringLiteral("object"))) {
                model[QStringLiteral("object")] = QStringLiteral("model");
            }
            normalizedData.append(model);
        }
    };

    appendModelsFromArray(root.value(QStringLiteral("data")).toArray());

    if (normalizedData.isEmpty()) {
        appendModelsFromArray(root.value(QStringLiteral("models")).toArray());
    }

    if (normalizedData.isEmpty() && root.contains(QStringLiteral("result"))) {
        const QJsonObject resultObj = root.value(QStringLiteral("result")).toObject();
        appendModelsFromArray(resultObj.value(QStringLiteral("models")).toArray());
    }

    if (normalizedData.isEmpty()) {
        return rawBody;
    }

    if (preferAnthropicSchema) {
        QJsonArray anthropicData;

        auto supportsAnthropic = [](const QJsonObject& model) {
            const QJsonValue value = model.value(QStringLiteral("supported_endpoint_types"));
            if (!value.isArray()) {
                return true;
            }
            const QJsonArray types = value.toArray();
            if (types.isEmpty()) {
                return true;
            }
            for (const QJsonValue& item : types) {
                if (item.toString().compare(QStringLiteral("anthropic"), Qt::CaseInsensitive) == 0) {
                    return true;
                }
            }
            return false;
        };

        auto appendAnthropicModel = [&](const QJsonObject& model) {
            QJsonObject out;
            const QString id = normalizeModelId(model);
            if (id.isEmpty()) {
                return;
            }

            out[QStringLiteral("type")] = QStringLiteral("model");
            out[QStringLiteral("id")] = id;

            QString displayName = model.value(QStringLiteral("display_name")).toString();
            if (displayName.isEmpty()) {
                displayName = id;
            }
            out[QStringLiteral("display_name")] = displayName;

            QString createdAt = model.value(QStringLiteral("created_at")).toString();
            if (createdAt.isEmpty()) {
                const qint64 createdEpoch = model.value(QStringLiteral("created")).toVariant().toLongLong();
                if (createdEpoch > 0) {
                    createdAt = QDateTime::fromSecsSinceEpoch(createdEpoch, Qt::UTC)
                                    .toString(Qt::ISODate);
                }
            }
            if (!createdAt.isEmpty()) {
                out[QStringLiteral("created_at")] = createdAt;
            }

            anthropicData.append(out);
        };

        for (const QJsonValue& item : normalizedData) {
            const QJsonObject model = item.toObject();
            if (!supportsAnthropic(model)) {
                continue;
            }
            appendAnthropicModel(model);
        }

        if (anthropicData.isEmpty()) {
            for (const QJsonValue& item : normalizedData) {
                appendAnthropicModel(item.toObject());
            }
        }

        QJsonObject normalizedRoot;
        normalizedRoot[QStringLiteral("data")] = anthropicData;
        normalizedRoot[QStringLiteral("has_more")] = false;
        if (!anthropicData.isEmpty()) {
            const QString firstId = anthropicData.first().toObject().value(QStringLiteral("id")).toString();
            const QString lastId = anthropicData.last().toObject().value(QStringLiteral("id")).toString();
            if (!firstId.isEmpty()) {
                normalizedRoot[QStringLiteral("first_id")] = firstId;
            }
            if (!lastId.isEmpty()) {
                normalizedRoot[QStringLiteral("last_id")] = lastId;
            }
        }
        return QJsonDocument(normalizedRoot).toJson(QJsonDocument::Compact);
    }

    QJsonObject normalizedRoot;
    normalizedRoot[QStringLiteral("object")] = QStringLiteral("list");
    normalizedRoot[QStringLiteral("data")] = normalizedData;
    return QJsonDocument(normalizedRoot).toJson(QJsonDocument::Compact);
}

}

// ---------------------------------------------------------------------------
// Forward-declared types from the pipeline module.
// Pipeline exposes:
//   Result<QByteArray>               process(const QByteArray& body,
//                                            const QMap<QString,QString>& meta);
//   Result<PipelineStreamSession*>   processStream(const QByteArray& body,
//                                                  const QMap<QString,QString>& meta);
//
// PipelineStreamSession (QObject) exposes:
//   signals:  encodedFrameReady(const QByteArray&)
//             finished()
//             error(const DomainFailure&)
//   slots:    abort()
// ---------------------------------------------------------------------------
#include "pipeline/pipeline.h"

// ========================================================================
// Construction / destruction
// ========================================================================

ProxyServer::ProxyServer(QObject* parent)
    : QObject(parent)
{
    m_router.registerDefaults();
}

ProxyServer::~ProxyServer()
{
    stop();
}

// ========================================================================
// setPipeline
// ========================================================================

void ProxyServer::setPipeline(Pipeline* pipeline)
{
    m_pipeline = pipeline;
}

// ========================================================================
// isPortInUse
// ========================================================================

bool ProxyServer::isPortInUse(int port)
{
    QTcpServer test;
    bool available = test.listen(QHostAddress::Any, static_cast<quint16>(port));
    if (available) test.close();
    return !available;
}

// ========================================================================
// start
// ========================================================================

bool ProxyServer::start(const ProxyConfig& config)
{
    if (m_server) {
        stop();
    }

    m_config = config;
    m_connectionPool.clear();
    const bool useConnectionPool = config.runtime.enableConnectionPool;
    m_connectionPool.setEnabled(useConnectionPool);
    if (useConnectionPool) {
        m_connectionPool.resize(qMax(1, config.runtime.connectionPoolSize));
    } else {
        m_connectionPool.resize(1);
    }

    // ---- Load SSL certificate ----
    QFile certFile(config.certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QStringLiteral("ProxyServer: failed to open certificate file: %1")
                      .arg(config.certPath));
        return false;
    }

    QFile keyFile(config.keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QStringLiteral("ProxyServer: failed to open private key file: %1")
                      .arg(config.keyPath));
        return false;
    }

    QSslCertificate cert(&certFile, QSsl::Pem);
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);

    if (cert.isNull()) {
        LOG_ERROR(QStringLiteral("ProxyServer: SSL certificate is invalid or empty"));
        return false;
    }
    if (key.isNull()) {
        LOG_ERROR(QStringLiteral("ProxyServer: SSL private key is invalid or empty"));
        return false;
    }

    // ---- Configure SSL ----
    QSslConfiguration sslConfig;
    sslConfig.setLocalCertificate(cert);
    sslConfig.setPrivateKey(key);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

    // ---- Create and start server ----
    quint16 port = static_cast<quint16>(config.runtime.proxyPort);
    if (isPortInUse(port)) {
        LOG_ERROR(QStringLiteral("ProxyServer: port %1 is already in use").arg(port));
        return false;
    }

    m_server = new QSslServer(this);
    m_server->setSslConfiguration(sslConfig);

    connect(m_server, &QSslServer::pendingConnectionAvailable,
            this, &ProxyServer::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, port)) {
        LOG_ERROR(QStringLiteral("ProxyServer: failed to listen on port %1 - %2")
                      .arg(port)
                      .arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }

    LOG_INFO(QStringLiteral("ProxyServer: HTTPS proxy started on port %1").arg(port));
    emit statusChanged(true);
    return true;
}

// ========================================================================
// stop
// ========================================================================

void ProxyServer::stop()
{
    if (!m_server) {
        return;
    }

    // Abort all active streaming sessions
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        PipelineStreamSession* session = it.value();
        if (session) {
            session->abort();
        }
    }
    m_activeSessions.clear();

    // Disconnect and schedule deletion for every tracked socket
    for (auto it = m_pendingData.begin(); it != m_pendingData.end(); ++it) {
        QSslSocket* socket = it.key();
        socket->disconnectFromHost();
    }
    m_pendingData.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;

    m_connectionPool.clear();

    LOG_INFO(QStringLiteral("ProxyServer: proxy server stopped"));
    emit statusChanged(false);
}

// ========================================================================
// isRunning
// ========================================================================

bool ProxyServer::isRunning() const
{
    return m_server && m_server->isListening();
}

// ========================================================================
// onNewConnection
// ========================================================================

void ProxyServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QSslSocket* socket =
            qobject_cast<QSslSocket*>(m_server->nextPendingConnection());
        if (!socket) {
            continue;
        }

        connect(socket, &QSslSocket::readyRead,
                this, &ProxyServer::onSocketReadyRead);
        connect(socket, &QSslSocket::disconnected,
                this, &ProxyServer::onSocketDisconnected);
        connect(socket, &QSslSocket::sslErrors,
                this, [socket](const QList<QSslError>&) {
                    socket->ignoreSslErrors();
                });

        LOG_DEBUG(QStringLiteral("ProxyServer: new TLS connection from %1:%2")
                      .arg(socket->peerAddress().toString())
                      .arg(socket->peerPort()));
    }
}

// ========================================================================
// onSocketReadyRead
// ========================================================================

void ProxyServer::onSocketReadyRead()
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (!socket) {
        return;
    }

    m_pendingData[socket] += socket->readAll();
    QByteArray& buffer = m_pendingData[socket];

    while (true) {
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }

        int contentLength = 0;
        bool hasChunkedTransfer = false;
        const QString headerBlock = QString::fromUtf8(buffer.left(headerEnd));
        const QStringList headerLines = headerBlock.split(QStringLiteral("\r\n"));
        for (const QString& line : headerLines) {
            if (line.startsWith(QStringLiteral("Content-Length:"), Qt::CaseInsensitive)) {
                contentLength = line.mid(15).trimmed().toInt();
            }
            if (line.startsWith(QStringLiteral("Transfer-Encoding:"), Qt::CaseInsensitive)
                && line.contains(QStringLiteral("chunked"), Qt::CaseInsensitive)) {
                hasChunkedTransfer = true;
            }
        }

        if (hasChunkedTransfer) {
            QJsonObject errObj;
            errObj[QStringLiteral("error")] = QStringLiteral("chunked request bodies are not supported");
            sendHttpResponse(socket, 501,
                             QJsonDocument(errObj).toJson(QJsonDocument::Compact));
            buffer.clear();
            return;
        }

        const int bodyStart = headerEnd + 4;
        const int totalRequired = bodyStart + contentLength;
        if (buffer.size() < totalRequired) {
            return;
        }

        const QByteArray requestData = buffer.left(totalRequired);
        buffer.remove(0, totalRequired);

        const HttpRequest req = parseHttpRequest(requestData);
        handleRequest(socket, req);

        if (socket->state() != QAbstractSocket::ConnectedState) {
            return;
        }

        if (buffer.isEmpty()) {
            return;
        }
    }
}

// ========================================================================
// onSocketDisconnected
// ========================================================================

void ProxyServer::onSocketDisconnected()
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (!socket) {
        return;
    }

    m_pendingData.remove(socket);

    PipelineStreamSession* session = m_activeSessions.take(socket);
    if (session) {
        session->abort();
    }

    LOG_DEBUG(QStringLiteral("ProxyServer: client disconnected"));
}

// ========================================================================
// parseHttpRequest
// ========================================================================

ProxyServer::HttpRequest ProxyServer::parseHttpRequest(const QByteArray& data)
{
    HttpRequest req;

    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return req;
    }

    QString headerBlock = QString::fromUtf8(data.left(headerEnd));
    QStringList lines = headerBlock.split(QStringLiteral("\r\n"));

    // Parse the request line: "METHOD PATH HTTP/1.1"
    if (!lines.isEmpty()) {
        QStringList parts = lines[0].split(QLatin1Char(' '));
        if (parts.size() >= 3) {
            req.method      = parts[0].trimmed().toUpper();
            req.path        = parts[1];
            req.httpVersion = parts[2];
        }
    }

    // Parse headers
    for (int i = 1; i < lines.size(); ++i) {
        int colon = lines[i].indexOf(QLatin1Char(':'));
        if (colon > 0) {
            QString key   = lines[i].left(colon).trimmed().toLower();
            QString value = lines[i].mid(colon + 1).trimmed();
            req.headers[key] = value;
        }
    }

    // Extract body
    req.body = data.mid(headerEnd + 4);
    req.contentLength = req.body.size();
    req.complete = true;

    return req;
}

// ========================================================================
// handleRequest
// ========================================================================

void ProxyServer::handleRequest(QSslSocket* socket, const HttpRequest& request)
{
    LOG_INFO(QStringLiteral("ProxyServer: %1 %2").arg(request.method, request.path));

    if (request.method == QStringLiteral("GET")
        && request.path == QStringLiteral("/v1/models")) {
        if (handleModelsRequest(socket, request)) {
            return;
        }
    }

    // ---- Route lookup ----
    auto routeOpt = m_router.match(request.method, request.path);
    if (!routeOpt) {
        QJsonObject errObj;
        errObj[QStringLiteral("error")] = QStringLiteral("route not found");
        errObj[QStringLiteral("path")]  = request.path;
        sendHttpResponse(socket, 404,
                         QJsonDocument(errObj).toJson(QJsonDocument::Compact));
        return;
    }

    if (!m_pipeline) {
        QJsonObject errObj;
        errObj[QStringLiteral("error")] = QStringLiteral("pipeline not configured");
        sendHttpResponse(socket, 503,
                         QJsonDocument(errObj).toJson(QJsonDocument::Compact));
        return;
    }

    const Route& route = *routeOpt;
    QMap<QString, QString> metadata = buildMetadata(request, route);

    // ---- Detect streaming request ----
    QJsonParseError parseErr;
    QJsonDocument bodyDoc = QJsonDocument::fromJson(request.body, &parseErr);
    bool isStream = false;
    if (parseErr.error == QJsonParseError::NoError && bodyDoc.isObject()) {
        isStream = bodyDoc.object().value(QStringLiteral("stream")).toBool(false);
    }

    if (!isStream && request.path.contains(QStringLiteral("/models/"), Qt::CaseInsensitive)) {
        isStream = true;
    }

    // ---- Dispatch to pipeline ----
    if (isStream) {
        auto result = m_pipeline->processStream(request.body, metadata);
        if (!result) {
            DomainFailure failure = result.error();
            sendHttpResponse(socket, failure.httpStatus(),
                             QJsonDocument(failure.toJson()).toJson(QJsonDocument::Compact));
            return;
        }
        sendStreamResponse(socket, *result);
    } else {
        auto result = m_pipeline->process(request.body, metadata);
        if (!result) {
            DomainFailure failure = result.error();
            sendHttpResponse(socket, failure.httpStatus(),
                             QJsonDocument(failure.toJson()).toJson(QJsonDocument::Compact));
            return;
        }
        sendHttpResponse(socket, 200, *result);
    }
}

bool ProxyServer::handleModelsRequest(QSslSocket* socket, const HttpRequest& request)
{
    const ConfigGroup& group = m_config.currentGroup();
    if (group.baseUrl.isEmpty()) {
        return false;
    }

    const bool preferDownstreamAnthropic =
        request.headers.contains(QStringLiteral("anthropic-version"))
        || request.headers.contains(QStringLiteral("x-api-key"));
    model_list_request_builder::DownstreamHeaders incomingHeaders;
    incomingHeaders.authorization = request.headers.value(QStringLiteral("authorization")).trimmed();
    incomingHeaders.xApiKey = request.headers.value(QStringLiteral("x-api-key")).trimmed();
    incomingHeaders.xGoogApiKey = request.headers.value(QStringLiteral("x-goog-api-key")).trimmed();
    incomingHeaders.anthropicVersion = request.headers.value(QStringLiteral("anthropic-version")).trimmed();
    incomingHeaders.anthropicBeta = request.headers.value(QStringLiteral("anthropic-beta")).trimmed();

    const model_list_request_builder::Context requestContext =
        model_list_request_builder::buildContext(group, incomingHeaders, m_config.global.authKey);
    if (!requestContext.isValid()) {
        sendHttpResponse(socket, 400,
                         QJsonDocument(DomainFailure::invalidInput(
                                           QStringLiteral("invalid_model_list_url"),
                                           QStringLiteral("model list URL is invalid"))
                                           .toJson())
                             .toJson(QJsonDocument::Compact));
        return true;
    }

    using provider_routing::ModelListProvider;
    const ModelListProvider provider = requestContext.provider;
    const QStringList authModes = requestContext.authModes;

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(m_config.runtime.disableSslStrict
                                    ? QSslSocket::VerifyNone
                                    : QSslSocket::AutoVerifyPeer);
    sslConfig.setProtocol(m_config.runtime.enableHttp2
                              ? QSsl::TlsV1_2OrLater
                              : QSsl::TlsV1_2);
    QtExecutor executor(m_connectionPool, sslConfig);
    executor.setRequestTimeout(m_config.runtime.requestTimeout);
    executor.setConnectionTimeout(m_config.runtime.connectionTimeout);

    Result<ProviderResponse> result = std::unexpected(DomainFailure::internal(
        QStringLiteral("models request was not attempted")));

    for (int i = 0; i < authModes.size(); ++i) {
        const QString authMode = authModes.at(i);
        ProviderRequest attemptReq = model_list_request_builder::makeProviderRequest(
            requestContext, authMode);
        LOG_DEBUG(QStringLiteral("ProxyServer: /v1/models trying auth=%1 key_source=%2 url=%3")
                      .arg(authMode, requestContext.keySource, attemptReq.url));

        result = executor.execute(attemptReq);
        if (result.has_value()) {
            break;
        }

        const DomainFailure failure = result.error();
        const int failureStatus = failure.httpStatus();
        LOG_WARNING(QStringLiteral("ProxyServer: /v1/models auth=%1 failed status=%2 msg=%3")
                        .arg(authMode)
                        .arg(failureStatus)
                        .arg(failure.message));

        const bool isAuthFailure = (failureStatus == 401 || failureStatus == 403);
        const bool canRetry = (i + 1) < authModes.size();
        if (!(isAuthFailure && canRetry)) {
            sendHttpResponse(socket,
                             failure.httpStatus(),
                             QJsonDocument(failure.toJson()).toJson(QJsonDocument::Compact));
            return true;
        }
    }

    if (!result.has_value()) {
        const DomainFailure failure = result.error();
        sendHttpResponse(socket,
                         failure.httpStatus(),
                         QJsonDocument(failure.toJson()).toJson(QJsonDocument::Compact));
        return true;
    }

    const ProviderResponse response = result.value();
    int status = response.statusCode;
    if (status <= 0) {
        status = 502;
    }

    LOG_DEBUG(QStringLiteral("ProxyServer: /v1/models upstream status=%1 bytes=%2")
                  .arg(status)
                  .arg(response.body.size()));
    if (status < 200 || status >= 300) {
        const QString bodyPreview = QString::fromUtf8(response.body.left(512));
        LOG_WARNING(QStringLiteral("ProxyServer: /v1/models upstream error body: %1")
                        .arg(bodyPreview));
    }

    QByteArray responseBody = response.body;
    if (status >= 200 && status < 300) {
        const bool preferAnthropicSchema =
            (provider == ModelListProvider::Anthropic) || preferDownstreamAnthropic;
        responseBody = normalizeModelListBody(response.body, preferAnthropicSchema);
    }

    sendHttpResponse(socket, status, responseBody,
                     QStringLiteral("application/json; charset=utf-8"));
    return true;
}

// ========================================================================
// sendHttpResponse
// ========================================================================

void ProxyServer::sendHttpResponse(QSslSocket* socket, int status,
                                   const QByteArray& body,
                                   const QString& contentType)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    static const QMap<int, QString> statusTexts = {
        {200, QStringLiteral("OK")},
        {400, QStringLiteral("Bad Request")},
        {401, QStringLiteral("Unauthorized")},
        {403, QStringLiteral("Forbidden")},
        {404, QStringLiteral("Not Found")},
        {429, QStringLiteral("Too Many Requests")},
        {500, QStringLiteral("Internal Server Error")},
        {501, QStringLiteral("Not Implemented")},
        {503, QStringLiteral("Service Unavailable")},
        {504, QStringLiteral("Gateway Timeout")}
    };

    QString statusText = statusTexts.value(status, QStringLiteral("Unknown"));

    QByteArray response;
    response.append(QStringLiteral("HTTP/1.1 %1 %2\r\n")
                        .arg(status)
                        .arg(statusText)
                        .toUtf8());
    response.append(QStringLiteral("Content-Type: %1\r\n")
                        .arg(contentType)
                        .toUtf8());
    response.append(QStringLiteral("Content-Length: %1\r\n")
                        .arg(body.size())
                        .toUtf8());
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Connection: keep-alive\r\n");
    response.append("\r\n");
    response.append(body);

    socket->write(response);
    socket->flush();
}

// ========================================================================
// sendStreamResponse
// ========================================================================

void ProxyServer::sendStreamResponse(QSslSocket* socket,
                                     PipelineStreamSession* session)
{
    m_activeSessions[socket] = session;

    // Write the HTTP response header with chunked transfer encoding
    SseWriter::writeStreamHeader(socket);

    // Forward each encoded frame from the pipeline as an SSE chunk
    connect(session, &PipelineStreamSession::encodedFrameReady,
            this, [socket](const QByteArray& data) {
                SseWriter::sendChunk(socket, data);
            });

    // On normal completion, send the SSE [DONE] sentinel and terminate
    connect(session, &PipelineStreamSession::finished,
            this, [this, socket, session]() {
                SseWriter::sendDone(socket);
                SseWriter::sendTerminator(socket);
                if (m_activeSessions.value(socket) == session) {
                    m_activeSessions.remove(socket);
                }
            });

    // On error, send the failure as a final SSE event, then terminate
    connect(session, &PipelineStreamSession::error,
            this, [this, socket, session](const DomainFailure& failure) {
                QByteArray errJson =
                    QJsonDocument(failure.toJson()).toJson(QJsonDocument::Compact);
                SseWriter::sendChunk(socket, errJson);
                SseWriter::sendDone(socket);
                SseWriter::sendTerminator(socket);
                if (m_activeSessions.value(socket) == session) {
                    m_activeSessions.remove(socket);
                }
            });

    // If the client disconnects mid-stream, abort the pipeline session
    connect(socket, &QSslSocket::disconnected,
            session, &PipelineStreamSession::abort);
}

// ========================================================================
// buildMetadata
// ========================================================================

QMap<QString, QString> ProxyServer::buildMetadata(
    const HttpRequest& request,
    const Route& route) const
{
    const ConfigGroup& group = m_config.currentGroup();

    QMap<QString, QString> meta;
    meta[QStringLiteral("inbound.format")]    = route.inboundProtocol;
    meta[QStringLiteral("provider")]          = route.provider.isEmpty()
                                                    ? group.provider
                                                    : route.provider;
    meta[QStringLiteral("provider_base_url")] = group.baseUrl;
    meta[QStringLiteral("provider_api_key")]  = group.apiKey;
    meta[QStringLiteral("api_key")]           = group.apiKey;
    meta[QStringLiteral("model_id")]          = group.modelId;
    meta[QStringLiteral("middle_route")]      = group.middleRoute;
    meta[QStringLiteral("mapped_model_id")]   = m_config.global.mappedModelId;
    if (!group.baseUrlCandidates.isEmpty()) {
        meta[QStringLiteral("provider_base_url_candidates")] =
            group.baseUrlCandidates.join(QLatin1Char(','));
    }
    if (!group.outboundAdapter.isEmpty()) {
        meta[QStringLiteral("provider_adapter")] = group.outboundAdapter;
    }

    // Propagate the client's auth token so the pipeline can validate it
    // against the configured global auth key
    QString authHeader = request.headers.value(QStringLiteral("authorization"));
    if (authHeader.isEmpty()) {
        authHeader = request.headers.value(QStringLiteral("x-api-key"));
    }
    meta[QStringLiteral("auth_key")] = authHeader;

    // Carry the original request path so adapters can reconstruct URLs
    meta[QStringLiteral("request_path")] = request.path;

    // Copy custom headers from the config group
    for (auto it = group.customHeaders.cbegin();
         it != group.customHeaders.cend(); ++it) {
        meta[QStringLiteral("custom_header.%1").arg(it.key())] = it.value();
    }

    return meta;
}
