#include "adapters/inbound/jina.h"

JinaAdapter::JinaAdapter(IInboundAdapter* delegate)
    : m_delegate(delegate)
{
}

QString JinaAdapter::protocol() const
{
    return QStringLiteral("jina");
}

Result<SemanticRequest> JinaAdapter::decodeRequest(
    const QByteArray& body,
    const QMap<QString, QString>& metadata)
{
    auto result = m_delegate->decodeRequest(body, metadata);
    if (result.has_value()) {
        result->metadata[QStringLiteral("_client")] = QStringLiteral("jina");
    }
    return result;
}

Result<QByteArray> JinaAdapter::encodeResponse(const SemanticResponse& response)
{
    return m_delegate->encodeResponse(response);
}

Result<QByteArray> JinaAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    return m_delegate->encodeStreamFrame(frame);
}

Result<QByteArray> JinaAdapter::encodeFailure(const DomainFailure& failure)
{
    return m_delegate->encodeFailure(failure);
}
