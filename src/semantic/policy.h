#pragma once
#include "ports.h"
#include "capability.h"
#include <QList>

struct ExecutionPlan {
    QString targetModel;
    int maxAttempts = 1;
    QList<ErrorKind> retryableKinds = {
        ErrorKind::Unavailable,
        ErrorKind::Timeout,
        ErrorKind::RateLimited
    };
};

struct RetryDecision {
    bool retry = false;
    bool switchPath = false;
    QString reason;
};

class Policy {
public:
    void setDefaultMaxAttempts(int attempts) { m_defaultMaxAttempts = qMax(1, attempts); }

    VoidResult preflight(const SemanticRequest& req,
                         const CapabilityProfile& profile);
    ExecutionPlan plan(const SemanticRequest& req,
                       const CapabilityProfile& profile);
    RetryDecision nextRetry(const ExecutionPlan& plan,
                            int attempt,
                            const DomainFailure& failure);

private:
    int m_defaultMaxAttempts = 1;
};
