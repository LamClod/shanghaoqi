#include "pipeline.h"
#include "semantic/processor.h"
#include "semantic/stream_session.h"

// ========== PipelineStreamSession ==========

PipelineStreamSession::PipelineStreamSession(
        StreamSession* upstream,
        IInboundAdapter* inbound,
        const QString& inboundProtocol,
        const QString& inboundDelegate,
        const QList<IPipelineMiddleware*>& middlewares,
        QObject* parent)
    : QObject(parent)
    , m_upstream(upstream)
    , m_inbound(inbound)
    , m_inboundProtocol(inboundProtocol)
    , m_inboundDelegate(inboundDelegate)
    , m_middlewares(middlewares)
{
    connect(m_upstream, &StreamSession::frameReady,
            this, &PipelineStreamSession::onUpstreamFrame);
    connect(m_upstream, &StreamSession::finished,
            this, &PipelineStreamSession::onUpstreamFinished);
    connect(m_upstream, &StreamSession::error,
            this, &PipelineStreamSession::onUpstreamError);
}

void PipelineStreamSession::abort() {
    if (m_upstream) m_upstream->abort();
}

void PipelineStreamSession::onUpstreamFrame(const StreamFrame& frame) {
    StreamFrame f = frame;
    if (!m_inboundProtocol.isEmpty()) {
        f.extensions.set(QStringLiteral("inbound_protocol"), m_inboundProtocol);
    }
    if (m_inboundProtocol == QStringLiteral("codex") && !m_inboundDelegate.isEmpty()) {
        f.extensions.set(QStringLiteral("codex_delegate"), m_inboundDelegate);
    } else if (m_inboundProtocol == QStringLiteral("antigravity") && !m_inboundDelegate.isEmpty()) {
        f.extensions.set(QStringLiteral("antigravity_delegate"), m_inboundDelegate);
    }
    for (auto* mw : m_middlewares) {
        auto r = mw->onFrame(std::move(f));
        if (r) {
            f = *r;
        } else {
            emit error(r.error());
            return;
        }
    }
    auto encoded = m_inbound->encodeStreamFrame(f);
    if (encoded)
        emit encodedFrameReady(*encoded);
    else
        emit error(encoded.error());
}

void PipelineStreamSession::onUpstreamFinished() {
    emit finished();
}

void PipelineStreamSession::onUpstreamError(const DomainFailure& failure) {
    emit error(failure);
}

// ========== Pipeline ==========

Pipeline::Pipeline(IInboundAdapter* inbound,
                   IOutboundAdapter* outbound,
                   IExecutor* executor,
                   ICapabilityResolver* capabilities,
                   QObject* parent)
    : QObject(parent)
    , m_inbound(inbound)
    , m_processor(new Processor(this))
{
    m_processor->outbound = outbound;
    m_processor->executor = executor;
    m_processor->capabilities = capabilities;
}

void Pipeline::addMiddleware(std::unique_ptr<IPipelineMiddleware> mw) {
    m_middlewares.push_back(std::move(mw));
}

void Pipeline::setPolicy(Policy* policy)
{
    if (!m_processor) {
        return;
    }
    m_processor->setPolicy(policy);
}

Result<QByteArray> Pipeline::process(const QByteArray& requestBody,
                                     const QMap<QString, QString>& metadata) {
    auto decoded = m_inbound->decodeRequest(requestBody, metadata);
    if (!decoded) return std::unexpected(decoded.error());

    SemanticRequest req = *decoded;

    // Forward through middlewares in order
    for (auto& mw : m_middlewares) {
        auto r = mw->onRequest(std::move(req));
        if (!r) return std::unexpected(r.error());
        req = *r;
    }

    const QString inboundProtocol =
        req.metadata.value(QStringLiteral("_inbound_protocol"),
                           metadata.value(QStringLiteral("inbound.format")));
    const QString inboundDelegate =
        inboundProtocol == QStringLiteral("codex")
            ? req.metadata.value(QStringLiteral("_codex_delegate"))
            : (inboundProtocol == QStringLiteral("antigravity")
                   ? req.metadata.value(QStringLiteral("_antigravity_delegate"))
                   : QString());

    auto resp = m_processor->process(std::move(req));
    if (!resp) return std::unexpected(resp.error());

    // Reverse through middlewares
    SemanticResponse response = *resp;
    if (!inboundProtocol.isEmpty()) {
        response.extensions.set(QStringLiteral("inbound_protocol"), inboundProtocol);
    }
    if (inboundProtocol == QStringLiteral("codex") && !inboundDelegate.isEmpty()) {
        response.extensions.set(QStringLiteral("codex_delegate"), inboundDelegate);
    } else if (inboundProtocol == QStringLiteral("antigravity") && !inboundDelegate.isEmpty()) {
        response.extensions.set(QStringLiteral("antigravity_delegate"), inboundDelegate);
    }
    auto reversed = reversedMiddlewares();
    for (auto* mw : reversed) {
        auto r = mw->onResponse(std::move(response));
        if (!r) return std::unexpected(r.error());
        response = *r;
    }

    return m_inbound->encodeResponse(response);
}

Result<PipelineStreamSession*> Pipeline::processStream(
        const QByteArray& requestBody,
        const QMap<QString, QString>& metadata) {
    auto decoded = m_inbound->decodeRequest(requestBody, metadata);
    if (!decoded) return std::unexpected(decoded.error());

    SemanticRequest req = *decoded;

    // Forward through middlewares in order
    for (auto& mw : m_middlewares) {
        auto r = mw->onRequest(std::move(req));
        if (!r) return std::unexpected(r.error());
        req = *r;
    }

    const QString inboundProtocol =
        req.metadata.value(QStringLiteral("_inbound_protocol"),
                           metadata.value(QStringLiteral("inbound.format")));
    const QString inboundDelegate =
        inboundProtocol == QStringLiteral("codex")
            ? req.metadata.value(QStringLiteral("_codex_delegate"))
            : (inboundProtocol == QStringLiteral("antigravity")
                   ? req.metadata.value(QStringLiteral("_antigravity_delegate"))
                   : QString());

    auto session = m_processor->processStream(std::move(req));
    if (!session) return std::unexpected(session.error());

    auto reversed = reversedMiddlewares();
    auto* pipeSession = new PipelineStreamSession(
        *session, m_inbound, inboundProtocol, inboundDelegate, reversed, this);
    return pipeSession;
}

QList<IPipelineMiddleware*> Pipeline::reversedMiddlewares() const {
    QList<IPipelineMiddleware*> list;
    list.reserve(m_middlewares.size());
    for (int i = m_middlewares.size() - 1; i >= 0; --i)
        list.append(m_middlewares[i].get());
    return list;
}
