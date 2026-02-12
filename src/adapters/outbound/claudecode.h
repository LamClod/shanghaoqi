#pragma once
#include "outbound_adapter.h"

class ClaudeCodeOutbound : public IOutboundAdapter {
public:
    explicit ClaudeCodeOutbound(IOutboundAdapter* anthropicDelegate);
    ~ClaudeCodeOutbound() override = default;

    QString adapterId() const override;

    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
    Result<SemanticResponse> parseResponse(const ProviderResponse& response) override;
    Result<StreamFrame> parseChunk(const ProviderChunk& chunk) override;
    DomainFailure mapFailure(int httpStatus, const QByteArray& body) override;

private:
    IOutboundAdapter* m_delegate;
};
