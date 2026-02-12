#pragma once
#include "outbound_adapter.h"

class GeminiOutbound : public IOutboundAdapter {
public:
    GeminiOutbound() = default;
    ~GeminiOutbound() override = default;

    QString adapterId() const override;

    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
    Result<SemanticResponse> parseResponse(const ProviderResponse& response) override;
    Result<StreamFrame> parseChunk(const ProviderChunk& chunk) override;
    DomainFailure mapFailure(int httpStatus, const QByteArray& body) override;

private:
    QJsonArray buildContents(const QList<InteractionItem>& items, QJsonArray& systemInstructionOut) const;
    QJsonArray buildToolDeclarations(const QList<ActionSpec>& tools) const;
    QJsonObject buildGenerationConfig(const ConstraintSet& constraints) const;
    QJsonArray segmentsToParts(const QList<Segment>& segments) const;
    Candidate parseGeminiCandidate(const QJsonObject& candidate) const;
    ActionCall parseFunctionCall(const QJsonObject& part) const;
    ErrorKind mapHttpStatusToKind(int httpStatus) const;
};
