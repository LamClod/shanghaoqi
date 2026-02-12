#pragma once
#include "adapters/inbound/inbound_adapter.h"

class GeminiAdapter : public IInboundAdapter {
public:
    GeminiAdapter() = default;

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
    static QList<Segment> parseParts(const QJsonArray& parts);
    static QJsonArray serializeParts(const QList<Segment>& segments);
    static QString finishReasonFromCause(StopCause cause);
};
