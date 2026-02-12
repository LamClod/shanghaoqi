#include "policy.h"

VoidResult Policy::preflight(const SemanticRequest& req,
                             const CapabilityProfile& profile) {
    if (!profile.taskSupport.value(req.kind, false)) {
        return std::unexpected(DomainFailure::notSupported(
            QStringLiteral("unsupported_task"),
            QStringLiteral("Adapter %1 does not support task kind %2")
                .arg(profile.adapterId)
                .arg(static_cast<int>(req.kind))));
    }
    return {};
}

ExecutionPlan Policy::plan(const SemanticRequest& req,
                           const CapabilityProfile& profile) {
    Q_UNUSED(profile);

    ExecutionPlan p;
    p.targetModel = req.target.logicalModel;
    const int requestedAttempts = qMax(1, req.target.fallback.maxAttempts);
    p.maxAttempts = qMax(m_defaultMaxAttempts, requestedAttempts);
    return p;
}

RetryDecision Policy::nextRetry(const ExecutionPlan& plan,
                                int attempt,
                                const DomainFailure& failure) {
    const int maxAttempts = qMax(1, plan.maxAttempts);
    if (attempt + 1 >= maxAttempts) {
        return {false, false, QStringLiteral("max retry attempts reached")};
    }

    for (auto kind : plan.retryableKinds) {
        if (failure.kind == kind) {
            return {
                true,
                true,
                QStringLiteral("retry %1/%2: %3")
                    .arg(attempt + 2)
                    .arg(maxAttempts)
                    .arg(failure.message)
            };
        }
    }

    return {false, false, QStringLiteral("non-retryable failure kind")};
}
