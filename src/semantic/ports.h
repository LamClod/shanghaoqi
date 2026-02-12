#pragma once
#include "request.h"
#include "response.h"
#include "frame.h"
#include "failure.h"
#include "capability.h"
#include "target.h"
#include <expected>
#include <QByteArray>
#include <QMap>
#include <QNetworkReply>

template<typename T>
using Result = std::expected<T, DomainFailure>;

using VoidResult = std::expected<void, DomainFailure>;

struct ProviderRequest {
    QString method;
    QString url;
    QMap<QString, QString> headers;
    QByteArray body;
    bool stream = false;
    QString adapterHint;
};

struct ProviderResponse {
    int statusCode = 0;
    QMap<QString, QString> headers;
    QByteArray body;
    QString adapterHint;
};

struct ProviderChunk {
    QString type;
    QByteArray data;
    QString adapterHint;
};

class IInboundAdapter {
public:
    virtual ~IInboundAdapter() = default;
    virtual QString protocol() const = 0;
    virtual Result<SemanticRequest> decodeRequest(
        const QByteArray& body,
        const QMap<QString, QString>& metadata) = 0;
    virtual Result<QByteArray> encodeResponse(
        const SemanticResponse& response) = 0;
    virtual Result<QByteArray> encodeStreamFrame(
        const StreamFrame& frame) = 0;
    virtual Result<QByteArray> encodeFailure(
        const DomainFailure& failure) = 0;
};

class IOutboundAdapter {
public:
    virtual ~IOutboundAdapter() = default;
    virtual QString adapterId() const = 0;
    virtual Result<ProviderRequest> buildRequest(
        const SemanticRequest& request) = 0;
    virtual Result<SemanticResponse> parseResponse(
        const ProviderResponse& response) = 0;
    virtual Result<StreamFrame> parseChunk(
        const ProviderChunk& chunk) = 0;
    virtual DomainFailure mapFailure(int httpStatus, const QByteArray& body) = 0;
};

class IExecutor {
public:
    virtual ~IExecutor() = default;
    virtual Result<ProviderResponse> execute(
        const ProviderRequest& request) = 0;
    virtual Result<QNetworkReply*> connectStream(
        const ProviderRequest& request) = 0;
};

class ICapabilityResolver {
public:
    virtual ~ICapabilityResolver() = default;
    virtual Result<CapabilityProfile> resolve(const TargetSpec& target) = 0;
};
