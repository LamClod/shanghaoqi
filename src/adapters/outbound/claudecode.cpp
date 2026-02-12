#include "claudecode.h"

ClaudeCodeOutbound::ClaudeCodeOutbound(IOutboundAdapter* anthropicDelegate)
    : m_delegate(anthropicDelegate)
{
}

QString ClaudeCodeOutbound::adapterId() const
{
    return QStringLiteral("claudecode");
}

Result<ProviderRequest> ClaudeCodeOutbound::buildRequest(const SemanticRequest& request)
{
    return m_delegate->buildRequest(request);
}

Result<SemanticResponse> ClaudeCodeOutbound::parseResponse(const ProviderResponse& response)
{
    return m_delegate->parseResponse(response);
}

Result<StreamFrame> ClaudeCodeOutbound::parseChunk(const ProviderChunk& chunk)
{
    return m_delegate->parseChunk(chunk);
}

DomainFailure ClaudeCodeOutbound::mapFailure(int httpStatus, const QByteArray& body)
{
    return m_delegate->mapFailure(httpStatus, body);
}
