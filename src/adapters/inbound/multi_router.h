#pragma once
#include "adapters/inbound/inbound_adapter.h"
#include <QMap>
#include <QMutex>
#include <QString>
#include <memory>
#include <vector>

class InboundMultiRouter : public IInboundAdapter {
public:
    InboundMultiRouter() = default;

    void registerAdapter(std::unique_ptr<IInboundAdapter> adapter);

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
    IInboundAdapter* findAdapter(const QString& name) const;
    IInboundAdapter* findAdapterFromResponse(const SemanticResponse& response) const;
    IInboundAdapter* findAdapterFromFrame(const StreamFrame& frame) const;
    IInboundAdapter* findAdapterFromFailure(const DomainFailure& failure) const;

    QMap<QString, IInboundAdapter*> m_adapters;
    std::vector<std::unique_ptr<IInboundAdapter>> m_owned;
    mutable QMutex m_activeProtocolMutex;
    QString m_activeProtocol;
};
