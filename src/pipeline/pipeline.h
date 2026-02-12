#pragma once
#include "middleware.h"
#include "semantic/ports.h"
#include "semantic/policy.h"
#include <QObject>
#include <QList>
#include <memory>
#include <vector>

class Processor;
class StreamSession;

class PipelineStreamSession : public QObject {
    Q_OBJECT
public:
    PipelineStreamSession(StreamSession* upstream,
                          IInboundAdapter* inbound,
                          const QString& inboundProtocol,
                          const QString& inboundDelegate,
                          const QList<IPipelineMiddleware*>& middlewares,
                          QObject* parent = nullptr);
    void abort();

signals:
    void encodedFrameReady(const QByteArray& sseData);
    void finished();
    void error(const DomainFailure& failure);

private slots:
    void onUpstreamFrame(const StreamFrame& frame);
    void onUpstreamFinished();
    void onUpstreamError(const DomainFailure& failure);

private:
    StreamSession* m_upstream;
    IInboundAdapter* m_inbound;
    QString m_inboundProtocol;
    QString m_inboundDelegate;
    QList<IPipelineMiddleware*> m_middlewares;
};

class Pipeline : public QObject {
    Q_OBJECT
public:
    Pipeline(IInboundAdapter* inbound,
             IOutboundAdapter* outbound,
             IExecutor* executor,
             ICapabilityResolver* capabilities,
             QObject* parent = nullptr);

    void addMiddleware(std::unique_ptr<IPipelineMiddleware> mw);

    Result<QByteArray> process(const QByteArray& requestBody,
                               const QMap<QString, QString>& metadata);

    Result<PipelineStreamSession*> processStream(
        const QByteArray& requestBody,
        const QMap<QString, QString>& metadata);

    void setPolicy(Policy* policy);

private:
    IInboundAdapter* m_inbound;
    Processor* m_processor;
    std::vector<std::unique_ptr<IPipelineMiddleware>> m_middlewares;

    QList<IPipelineMiddleware*> reversedMiddlewares() const;
};
