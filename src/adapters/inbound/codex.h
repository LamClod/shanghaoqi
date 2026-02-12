#pragma once
#include "adapters/inbound/inbound_adapter.h"

class CodexAdapter : public IInboundAdapter {
public:
    CodexAdapter(IInboundAdapter* chatDelegate, IInboundAdapter* responsesDelegate);

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
    static bool isResponsesFormat(const QJsonObject& root);
    IInboundAdapter* delegateFromProtocol(const QString& protocol) const;
    IInboundAdapter* delegateFromRequest(const QJsonObject& root) const;

    IInboundAdapter* m_chatDelegate;
    IInboundAdapter* m_responsesDelegate;
};
