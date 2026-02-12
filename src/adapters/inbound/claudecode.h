#pragma once
#include "adapters/inbound/inbound_adapter.h"

class ClaudeCodeAdapter : public IInboundAdapter {
public:
    explicit ClaudeCodeAdapter(IInboundAdapter* anthropicDelegate);

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
    IInboundAdapter* m_delegate;
};
