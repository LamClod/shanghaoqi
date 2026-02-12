#include "adapters/inbound/claudecode.h"

ClaudeCodeAdapter::ClaudeCodeAdapter(IInboundAdapter* anthropicDelegate)
    : m_delegate(anthropicDelegate)
{
}

QString ClaudeCodeAdapter::protocol() const
{
    return QStringLiteral("claudecode");
}

Result<SemanticRequest> ClaudeCodeAdapter::decodeRequest(
    const QByteArray& body,
    const QMap<QString, QString>& metadata)
{
    auto result = m_delegate->decodeRequest(body, metadata);
    if (result.has_value()) {
        result->metadata[QStringLiteral("_client")] = QStringLiteral("claudecode");
    }
    return result;
}

Result<QByteArray> ClaudeCodeAdapter::encodeResponse(const SemanticResponse& response)
{
    return m_delegate->encodeResponse(response);
}

Result<QByteArray> ClaudeCodeAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    return m_delegate->encodeStreamFrame(frame);
}

Result<QByteArray> ClaudeCodeAdapter::encodeFailure(const DomainFailure& failure)
{
    return m_delegate->encodeFailure(failure);
}
