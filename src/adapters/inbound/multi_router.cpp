#include "adapters/inbound/multi_router.h"
#include <QMutexLocker>

void InboundMultiRouter::registerAdapter(std::unique_ptr<IInboundAdapter> adapter)
{
    if (!adapter) return;
    const QString name = adapter->protocol().trimmed().toLower();
    if (name.isEmpty()) return;
    m_adapters.insert(name, adapter.get());
    m_owned.push_back(std::move(adapter));
}

QString InboundMultiRouter::protocol() const
{
    return QStringLiteral("multi");
}

IInboundAdapter* InboundMultiRouter::findAdapter(const QString& name) const
{
    auto it = m_adapters.find(name.trimmed().toLower());
    if (it != m_adapters.end())
        return it.value();
    return nullptr;
}

IInboundAdapter* InboundMultiRouter::findAdapterFromResponse(const SemanticResponse& response) const
{
    const QString protocol = response.extensions.get(QStringLiteral("inbound_protocol")).toString();
    if (!protocol.isEmpty()) {
        if (IInboundAdapter* adapter = findAdapter(protocol)) {
            return adapter;
        }
    }
    QString activeProtocol;
    {
        QMutexLocker locker(&m_activeProtocolMutex);
        activeProtocol = m_activeProtocol;
    }
    return findAdapter(activeProtocol);
}

IInboundAdapter* InboundMultiRouter::findAdapterFromFrame(const StreamFrame& frame) const
{
    const QString protocol = frame.extensions.get(QStringLiteral("inbound_protocol")).toString();
    if (!protocol.isEmpty()) {
        if (IInboundAdapter* adapter = findAdapter(protocol)) {
            return adapter;
        }
    }
    QString activeProtocol;
    {
        QMutexLocker locker(&m_activeProtocolMutex);
        activeProtocol = m_activeProtocol;
    }
    return findAdapter(activeProtocol);
}

IInboundAdapter* InboundMultiRouter::findAdapterFromFailure(const DomainFailure& failure) const
{
    Q_UNUSED(failure);
    QString activeProtocol;
    {
        QMutexLocker locker(&m_activeProtocolMutex);
        activeProtocol = m_activeProtocol;
    }
    return findAdapter(activeProtocol);
}

Result<SemanticRequest> InboundMultiRouter::decodeRequest(
    const QByteArray& body,
    const QMap<QString, QString>& metadata)
{
    const QString format = metadata.value(QStringLiteral("inbound.format"));
    if (format.isEmpty()) {
        return std::unexpected(DomainFailure::invalidInput(
            QStringLiteral("missing_format"),
            QStringLiteral("metadata[\"inbound.format\"] is required")));
    }

    const QString normalizedFormat = format.trimmed().toLower();
    IInboundAdapter* adapter = findAdapter(normalizedFormat);
    if (!adapter) {
        return std::unexpected(DomainFailure::invalidInput(
            QStringLiteral("unknown_format"),
            QStringLiteral("No adapter registered for format: %1").arg(format)));
    }

    {
        QMutexLocker locker(&m_activeProtocolMutex);
        m_activeProtocol = normalizedFormat;
    }

    QMap<QString, QString> enrichedMeta = metadata;
    enrichedMeta.insert(QStringLiteral("_inbound_protocol"), normalizedFormat);

    auto result = adapter->decodeRequest(body, enrichedMeta);
    if (result.has_value()) {
        result->metadata.insert(QStringLiteral("_inbound_protocol"), normalizedFormat);
    }
    return result;
}

Result<QByteArray> InboundMultiRouter::encodeResponse(
    const SemanticResponse& response)
{
    IInboundAdapter* adapter = findAdapterFromResponse(response);
    if (!adapter) {
        QString activeProtocol;
        {
            QMutexLocker locker(&m_activeProtocolMutex);
            activeProtocol = m_activeProtocol;
        }
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("No active adapter for protocol: %1").arg(activeProtocol)));
    }
    return adapter->encodeResponse(response);
}

Result<QByteArray> InboundMultiRouter::encodeStreamFrame(
    const StreamFrame& frame)
{
    IInboundAdapter* adapter = findAdapterFromFrame(frame);
    if (!adapter) {
        QString activeProtocol;
        {
            QMutexLocker locker(&m_activeProtocolMutex);
            activeProtocol = m_activeProtocol;
        }
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("No active adapter for protocol: %1").arg(activeProtocol)));
    }
    return adapter->encodeStreamFrame(frame);
}

Result<QByteArray> InboundMultiRouter::encodeFailure(
    const DomainFailure& failure)
{
    IInboundAdapter* adapter = findAdapterFromFailure(failure);
    if (!adapter) {
        QString activeProtocol;
        {
            QMutexLocker locker(&m_activeProtocolMutex);
            activeProtocol = m_activeProtocol;
        }
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("No active adapter for protocol: %1").arg(activeProtocol)));
    }
    return adapter->encodeFailure(failure);
}
