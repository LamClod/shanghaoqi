#pragma once
#include "adapters/inbound/inbound_adapter.h"
#include "adapters/inbound/openai_chat.h"

class AiSdkAdapter : public IInboundAdapter {
public:
    AiSdkAdapter() = default;

    QString protocol() const override;
    Result<SemanticRequest> decodeRequest(
        const QByteArray& body,
        const QMap<QString, QString>& metadata) override;
    Result<QByteArray> encodeResponse(
        const SemanticResponse& response) override;
    Result<QByteArray> encodeStreamFrame(
        const StreamFrame& frame) override;
    Result<QByteArray> encodeFailure(
        const DomainFailure& failure) override;

private:
    OpenAIChatAdapter m_openaiHelper;
};
