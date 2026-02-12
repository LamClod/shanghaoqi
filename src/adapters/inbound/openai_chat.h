#pragma once
#include "adapters/inbound/inbound_adapter.h"
#include <QUuid>
#include <QDateTime>

class OpenAIChatAdapter : public IInboundAdapter {
public:
    OpenAIChatAdapter() = default;

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

    static QList<Segment> parseContentField(const QJsonValue& content);
    static QList<ActionCall> parseToolCalls(const QJsonArray& toolCalls);
    static QJsonArray serializeToolCalls(const QList<ActionCall>& calls);
    static QString stopCauseToFinishReason(StopCause cause);
    static QString generateChatId();
};
