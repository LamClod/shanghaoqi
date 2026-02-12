#include "multi_router.h"
#include <QMutexLocker>

void OutboundMultiRouter::registerAdapter(std::unique_ptr<IOutboundAdapter> adapter)
{
    if (!adapter) {
        return;
    }

    QMutexLocker locker(&m_stateMutex);
    QString id = adapter->adapterId().trimmed().toLower();
    if (id.isEmpty()) {
        return;
    }
    m_adapters[id] = adapter.get();
    m_owned.push_back(std::move(adapter));
}

QString OutboundMultiRouter::adapterId() const
{
    return QStringLiteral("multi");
}

Result<ProviderRequest> OutboundMultiRouter::buildRequest(const SemanticRequest& request)
{
    IOutboundAdapter* adapter = resolve(request);
    if (!adapter) {
        return std::unexpected(DomainFailure::invalidInput(
            QStringLiteral("no_outbound_adapter"),
            QStringLiteral("Cannot resolve outbound adapter for request")));
    }
    {
        QMutexLocker locker(&m_stateMutex);
        m_lastResolvedAdapterId = adapter->adapterId().trimmed().toLower();
    }
    auto result = adapter->buildRequest(request);
    if (result.has_value()) {
        result->adapterHint = adapter->adapterId();
    }
    return result;
}

Result<SemanticResponse> OutboundMultiRouter::parseResponse(const ProviderResponse& response)
{
    IOutboundAdapter* adapter = resolveByAdapterHint(response.adapterHint);

    if (!adapter) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("No active outbound adapter for parseResponse")));
    }
    return adapter->parseResponse(response);
}

Result<StreamFrame> OutboundMultiRouter::parseChunk(const ProviderChunk& chunk)
{
    IOutboundAdapter* adapter = resolveByAdapterHint(chunk.adapterHint);
    if (!adapter) {
        adapter = resolveByChunkType(chunk.type);
    }
    if (!adapter) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Cannot resolve chunk adapter")));
    }
    return adapter->parseChunk(chunk);
}

DomainFailure OutboundMultiRouter::mapFailure(int httpStatus, const QByteArray& body)
{
    IOutboundAdapter* adapter = nullptr;

    {
        QMutexLocker locker(&m_stateMutex);
        if (!m_lastResolvedAdapterId.isEmpty()) {
            adapter = m_adapters.value(m_lastResolvedAdapterId, nullptr);
        }
        if (!adapter) {
            adapter = m_adapters.value(QStringLiteral("openai"), nullptr);
        }
    }

    if (adapter) {
        return adapter->mapFailure(httpStatus, body);
    }
    return DomainFailure::internal(QStringLiteral("HTTP %1").arg(httpStatus));
}

IOutboundAdapter* OutboundMultiRouter::resolveByAdapterHint(const QString& adapterHint)
{
    if (adapterHint.isEmpty()) {
        return nullptr;
    }
    QMutexLocker locker(&m_stateMutex);
    return m_adapters.value(adapterHint.trimmed().toLower(), nullptr);
}

IOutboundAdapter* OutboundMultiRouter::resolve(const SemanticRequest& request)
{
    // 1. Exact match via metadata["provider_adapter"]
    QString adapterId = request.metadata.value(QStringLiteral("provider_adapter"));
    if (!adapterId.isEmpty()) {
        QMutexLocker locker(&m_stateMutex);
        IOutboundAdapter* a = m_adapters.value(adapterId.trimmed().toLower(), nullptr);
        if (a) return a;
    }

    // 2. Provider name match via metadata["provider"]
    QString provider = request.metadata.value(QStringLiteral("provider"));
    if (!provider.isEmpty()) {
        QMutexLocker locker(&m_stateMutex);
        IOutboundAdapter* a = m_adapters.value(provider.trimmed().toLower(), nullptr);
        if (a) return a;
    }

    // 3. Model name heuristic
    QString model = request.target.logicalModel.toLower();
    if (model.startsWith(QStringLiteral("gpt"))
        || model.startsWith(QStringLiteral("o1"))
        || model.startsWith(QStringLiteral("o3"))
        || model.startsWith(QStringLiteral("o4"))) {
        QMutexLocker locker(&m_stateMutex);
        return m_adapters.value(QStringLiteral("openai"), nullptr);
    }
    if (model.startsWith(QStringLiteral("claude"))) {
        QMutexLocker locker(&m_stateMutex);
        return m_adapters.value(QStringLiteral("anthropic"), nullptr);
    }
    if (model.startsWith(QStringLiteral("gemini"))) {
        QMutexLocker locker(&m_stateMutex);
        return m_adapters.value(QStringLiteral("gemini"), nullptr);
    }
    if (model.startsWith(QStringLiteral("deepseek"))) {
        QMutexLocker locker(&m_stateMutex);
        return m_adapters.value(QStringLiteral("deepseek"), nullptr);
    }
    if (model.startsWith(QStringLiteral("glm"))
        || model.startsWith(QStringLiteral("chatglm"))) {
        QMutexLocker locker(&m_stateMutex);
        return m_adapters.value(QStringLiteral("zai"), nullptr);
    }
    if (model.startsWith(QStringLiteral("qwen"))) {
        QMutexLocker locker(&m_stateMutex);
        return m_adapters.value(QStringLiteral("bailian"), nullptr);
    }

    // 4. Default to openai
    QMutexLocker locker(&m_stateMutex);
    return m_adapters.value(QStringLiteral("openai"), nullptr);
}

IOutboundAdapter* OutboundMultiRouter::resolveByChunkType(const QString& type)
{
    // type format: "adapter:xxx|event:yyy|..."
    const QStringList parts = type.split(QLatin1Char('|'));
    for (const QString& part : parts) {
        if (part.startsWith(QStringLiteral("adapter:"))) {
            QString id = part.mid(8);
            QMutexLocker locker(&m_stateMutex);
            IOutboundAdapter* a = m_adapters.value(id.trimmed().toLower(), nullptr);
            if (a) return a;
        }
    }
    return nullptr;
}
