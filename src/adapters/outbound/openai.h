#pragma once
#include "outbound_adapter.h"

class OpenAIOutbound : public IOutboundAdapter {
public:
    OpenAIOutbound() = default;
    ~OpenAIOutbound() override = default;

    QString adapterId() const override;

    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
    Result<SemanticResponse> parseResponse(const ProviderResponse& response) override;
    Result<StreamFrame> parseChunk(const ProviderChunk& chunk) override;
    DomainFailure mapFailure(int httpStatus, const QByteArray& body) override;

protected:
    QJsonArray buildMessages(const QList<InteractionItem>& items) const;
    QJsonArray buildToolDefs(const QList<ActionSpec>& tools) const;
    void buildConstraints(QJsonObject& body, const ConstraintSet& constraints) const;
    Candidate parseChoice(const QJsonObject& choice) const;
    ActionCall parseToolCall(const QJsonObject& tc) const;
    StreamFrame parseDeltaChunk(const QJsonObject& delta, int index) const;
    ErrorKind mapHttpStatusToKind(int httpStatus) const;
};
