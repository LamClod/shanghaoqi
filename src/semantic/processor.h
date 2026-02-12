#pragma once
#include "ports.h"
#include "policy.h"
#include "stream_session.h"
#include "features/stream_aggregator.h"
#include "features/stream_splitter.h"
#include <QObject>

class Processor : public QObject {
    Q_OBJECT
public:
    explicit Processor(QObject* parent = nullptr);

    void setOutbound(IOutboundAdapter* outbound) { m_outbound = outbound; }
    void setExecutor(IExecutor* executor) { m_executor = executor; }
    void setCapabilities(ICapabilityResolver* cap) { m_capabilities = cap; }
    void setPolicy(Policy* policy) { m_policy = policy; }

    IOutboundAdapter*    outbound = nullptr;
    IExecutor*           executor = nullptr;
    ICapabilityResolver* capabilities = nullptr;
    Policy*              policy = nullptr;

    // Non-streaming (synchronous, uses QEventLoop internally if needed)
    Result<SemanticResponse> process(SemanticRequest request);

    // Streaming (async, returns signal source)
    Result<StreamSession*> processStream(SemanticRequest request);

private:
    IOutboundAdapter*    m_outbound = nullptr;
    IExecutor*           m_executor = nullptr;
    ICapabilityResolver* m_capabilities = nullptr;
    Policy*              m_policy = nullptr;

    Result<SemanticResponse> processOnce(const SemanticRequest& request);
    Result<StreamSession*>   processStreamOnce(const SemanticRequest& request);

    struct AttemptRouting {
        QStringList baseUrls;
        int current = 0;
        void advance() {
            if (baseUrls.isEmpty()) return;
            current = (current + 1) % baseUrls.size();
        }
        QString currentUrl() const { return baseUrls.value(current); }
    };

    AttemptRouting buildRouting(const QMap<QString, QString>& metadata) const;
    SemanticRequest withRouting(const SemanticRequest& req,
                                const AttemptRouting& routing, int attempt) const;

    // Helper to resolve the effective outbound/executor/capabilities/policy.
    // Supports both the public raw pointers (legacy) and the setter-based
    // private pointers so callers can use either style.
    IOutboundAdapter*    effectiveOutbound() const;
    IExecutor*           effectiveExecutor() const;
    ICapabilityResolver* effectiveCapabilities() const;
    Policy*              effectivePolicy() const;
};
