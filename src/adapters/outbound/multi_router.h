#pragma once
#include "outbound_adapter.h"
#include <QMap>
#include <QMutex>
#include <memory>
#include <vector>

class OutboundMultiRouter : public IOutboundAdapter {
public:
    OutboundMultiRouter() = default;
    ~OutboundMultiRouter() override = default;

    void registerAdapter(std::unique_ptr<IOutboundAdapter> adapter);

    QString adapterId() const override;

    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
    Result<SemanticResponse> parseResponse(const ProviderResponse& response) override;
    Result<StreamFrame> parseChunk(const ProviderChunk& chunk) override;
    DomainFailure mapFailure(int httpStatus, const QByteArray& body) override;

private:
    QMap<QString, IOutboundAdapter*> m_adapters;
    std::vector<std::unique_ptr<IOutboundAdapter>> m_owned;
    mutable QMutex m_stateMutex;
    QString m_lastResolvedAdapterId;

    IOutboundAdapter* resolve(const SemanticRequest& request);
    IOutboundAdapter* resolveByChunkType(const QString& type);
    IOutboundAdapter* resolveByAdapterHint(const QString& adapterHint);
};
