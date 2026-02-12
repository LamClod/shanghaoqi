#pragma once
#include "adapters/inbound/inbound_adapter.h"

class AnthropicAdapter : public IInboundAdapter {
public:
    AnthropicAdapter() = default;

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
    static QList<Segment> parseContentBlocks(const QJsonValue& content);
    static QList<ActionCall> parseToolUseBlocks(const QJsonArray& blocks);
    static QJsonArray serializeContentBlocks(const QList<Segment>& segments);
    static QJsonArray serializeToolUseBlocks(const QList<ActionCall>& calls);
    static QString stopReasonFromCause(StopCause cause);
    static QString generateMessageId();
};
