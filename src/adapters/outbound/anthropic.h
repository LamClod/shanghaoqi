#pragma once
#include "outbound_adapter.h"

class AnthropicOutbound : public IOutboundAdapter {
public:
    AnthropicOutbound() = default;
    ~AnthropicOutbound() override = default;

    QString adapterId() const override;

    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
    Result<SemanticResponse> parseResponse(const ProviderResponse& response) override;
    Result<StreamFrame> parseChunk(const ProviderChunk& chunk) override;
    DomainFailure mapFailure(int httpStatus, const QByteArray& body) override;

private:
    QJsonArray buildMessages(const QList<InteractionItem>& items, QString& systemOut) const;
    QJsonArray buildToolDefs(const QList<ActionSpec>& tools) const;
    QJsonArray segmentsToContentBlocks(const QList<Segment>& segments) const;
    Candidate parseCandidate(const QJsonObject& root) const;
    ActionCall parseToolUseBlock(const QJsonObject& block) const;
    ErrorKind mapHttpStatusToKind(int httpStatus) const;
};
