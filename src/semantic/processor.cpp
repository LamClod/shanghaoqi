#include "processor.h"
#include "validate.h"
#include "core/log_manager.h"

Processor::Processor(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Effective-pointer helpers: prefer the setter-based private pointer; fall
// back to the public raw pointer for backward compatibility.
// ---------------------------------------------------------------------------

IOutboundAdapter* Processor::effectiveOutbound() const
{
    return m_outbound ? m_outbound : outbound;
}

IExecutor* Processor::effectiveExecutor() const
{
    return m_executor ? m_executor : executor;
}

ICapabilityResolver* Processor::effectiveCapabilities() const
{
    return m_capabilities ? m_capabilities : capabilities;
}

Policy* Processor::effectivePolicy() const
{
    return m_policy ? m_policy : policy;
}

// ---------------------------------------------------------------------------
// Routing helpers
// ---------------------------------------------------------------------------

Processor::AttemptRouting Processor::buildRouting(
    const QMap<QString, QString>& metadata) const
{
    AttemptRouting routing;

    // Primary base URL always goes first
    QString primary = metadata.value(QStringLiteral("provider_base_url"));
    if (!primary.trimmed().isEmpty()) {
        routing.baseUrls.append(primary.trimmed());
    }

    // Then append any comma-separated candidate URLs
    QString candidates = metadata.value(QStringLiteral("provider_base_url_candidates"));
    if (!candidates.isEmpty()) {
        const QStringList parts = candidates.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            QString url = part.trimmed();
            if (!url.isEmpty() && !routing.baseUrls.contains(url)) {
                routing.baseUrls.append(url);
            }
        }
    }

    routing.current = 0;
    return routing;
}

SemanticRequest Processor::withRouting(const SemanticRequest& req,
                                       const AttemptRouting& routing,
                                       int attempt) const
{
    SemanticRequest copy = req;
    if (!routing.baseUrls.isEmpty()) {
        copy.metadata[QStringLiteral("provider_base_url")] = routing.currentUrl();
    }
    copy.metadata[QStringLiteral("_attempt")] = QString::number(attempt);
    return copy;
}

// ---------------------------------------------------------------------------
// Non-streaming path
// ---------------------------------------------------------------------------

Result<SemanticResponse> Processor::process(SemanticRequest request)
{
    // Step 1: Validate the request
    VoidResult valResult = Validate::request(request);
    if (!valResult.has_value()) {
        return std::unexpected(valResult.error());
    }

    // Step 2: Ensure capability resolver is available
    ICapabilityResolver* caps = effectiveCapabilities();
    if (!caps) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("capabilities resolver not set")));
    }

    // Step 3: Resolve capabilities for the target
    Result<CapabilityProfile> capResult = caps->resolve(request.target);
    if (!capResult.has_value()) {
        return std::unexpected(capResult.error());
    }
    const CapabilityProfile& profile = capResult.value();

    // Step 4: Policy preflight and plan
    Policy* pol = effectivePolicy();
    ExecutionPlan plan;

    if (pol) {
        VoidResult pf = pol->preflight(request, profile);
        if (!pf.has_value()) {
            return std::unexpected(pf.error());
        }
        plan = pol->plan(request, profile);
    }

    // Step 5: Build routing table
    AttemptRouting routing = buildRouting(request.metadata);

    // Step 6: Retry loop
    DomainFailure lastFailure = DomainFailure::internal(
        QStringLiteral("No attempts were made"));

    for (int attempt = 0; attempt < plan.maxAttempts; ++attempt) {
        SemanticRequest routed = withRouting(request, routing, attempt);

        LOG_DEBUG(QStringLiteral("Processor::process attempt %1/%2 url=%3")
                      .arg(attempt + 1)
                      .arg(plan.maxAttempts)
                      .arg(routing.currentUrl()));

        Result<SemanticResponse> result = processOnce(routed);

        if (result.has_value()) {
            return result;
        }

        lastFailure = result.error();

        // Consult retry policy
        if (pol) {
            RetryDecision decision = pol->nextRetry(plan, attempt, lastFailure);
            if (!decision.retry) {
                LOG_WARNING(QStringLiteral("Processor: not retrying after attempt %1: %2")
                                .arg(attempt + 1)
                                .arg(decision.reason));
                return std::unexpected(lastFailure);
            }
            LOG_WARNING(QStringLiteral("Processor: retrying (attempt %1/%2): %3")
                            .arg(attempt + 2)
                            .arg(plan.maxAttempts)
                            .arg(decision.reason));
            if (decision.switchPath) {
                routing.advance();
            }
        } else {
            // No policy means no retries
            return std::unexpected(lastFailure);
        }
    }

    return std::unexpected(
        DomainFailure::internal(QStringLiteral("All retry attempts exhausted")));
}

Result<SemanticResponse> Processor::processOnce(const SemanticRequest& request)
{
    IOutboundAdapter* ob = effectiveOutbound();
    IExecutor* ex = effectiveExecutor();

    if (!ob) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("outbound adapter not set")));
    }
    if (!ex) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("executor not set")));
    }

    // Build the provider-level request
    Result<ProviderRequest> provReqResult = ob->buildRequest(request);
    if (!provReqResult.has_value()) {
        return std::unexpected(provReqResult.error());
    }

    ProviderRequest provReq = provReqResult.value();

    // Execute the request
    Result<ProviderResponse> provRespResult = ex->execute(provReq);
    if (!provRespResult.has_value()) {
        return std::unexpected(provRespResult.error());
    }

    ProviderResponse provResp = provRespResult.value();
    if (provResp.adapterHint.isEmpty()) {
        provResp.adapterHint = provReq.adapterHint;
    }

    // Check for HTTP-level errors before parsing
    if (provResp.statusCode < 200 || provResp.statusCode >= 300) {
        DomainFailure httpFailure = ob->mapFailure(provResp.statusCode, provResp.body);
        return std::unexpected(httpFailure);
    }

    // Parse the provider response into a SemanticResponse
    auto parsed = ob->parseResponse(provResp);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    if (!provResp.adapterHint.isEmpty()) {
        parsed->extensions.set(QStringLiteral("provider_adapter_hint"), provResp.adapterHint);
    }
    return parsed;
}

// ---------------------------------------------------------------------------
// Streaming path
// ---------------------------------------------------------------------------

Result<StreamSession*> Processor::processStream(SemanticRequest request)
{
    // Step 1: Validate the request
    VoidResult valResult = Validate::request(request);
    if (!valResult.has_value()) {
        return std::unexpected(valResult.error());
    }

    // Step 2: Ensure capability resolver is available
    ICapabilityResolver* caps = effectiveCapabilities();
    if (!caps) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("capabilities resolver not set")));
    }

    // Step 3: Resolve capabilities
    Result<CapabilityProfile> capResult = caps->resolve(request.target);
    if (!capResult.has_value()) {
        return std::unexpected(capResult.error());
    }
    const CapabilityProfile& profile = capResult.value();

    // Step 4: Policy preflight and plan
    Policy* pol = effectivePolicy();
    ExecutionPlan plan;

    if (pol) {
        VoidResult pf = pol->preflight(request, profile);
        if (!pf.has_value()) {
            return std::unexpected(pf.error());
        }
        plan = pol->plan(request, profile);
    }

    // Step 5: Build routing table
    AttemptRouting routing = buildRouting(request.metadata);

    // Step 6: Retry loop -- for streaming, we only retry on connection-level
    // failures. Once a StreamSession is created successfully, no more retries.
    DomainFailure lastFailure = DomainFailure::internal(
        QStringLiteral("No stream attempts were made"));

    for (int attempt = 0; attempt < plan.maxAttempts; ++attempt) {
        SemanticRequest routed = withRouting(request, routing, attempt);
        routed.metadata[QStringLiteral("_stream")] = QStringLiteral("true");

        LOG_DEBUG(QStringLiteral("Processor::processStream attempt %1/%2 url=%3")
                      .arg(attempt + 1)
                      .arg(plan.maxAttempts)
                      .arg(routing.currentUrl()));

        Result<StreamSession*> result = processStreamOnce(routed);

        if (result.has_value()) {
            return result;
        }

        lastFailure = result.error();

        // Only retry connection-level (retryable) failures
        if (!lastFailure.retryable) {
            LOG_WARNING(QStringLiteral("Processor: stream failure is not retryable: %1")
                            .arg(lastFailure.message));
            return std::unexpected(lastFailure);
        }

        if (pol) {
            RetryDecision decision = pol->nextRetry(plan, attempt, lastFailure);
            if (!decision.retry) {
                LOG_WARNING(QStringLiteral("Processor: not retrying stream after attempt %1: %2")
                                .arg(attempt + 1)
                                .arg(decision.reason));
                return std::unexpected(lastFailure);
            }
            LOG_WARNING(QStringLiteral("Processor: retrying stream (attempt %1/%2): %3")
                            .arg(attempt + 2)
                            .arg(plan.maxAttempts)
                            .arg(decision.reason));
            if (decision.switchPath) {
                routing.advance();
            }
        } else {
            return std::unexpected(lastFailure);
        }
    }

    return std::unexpected(
        DomainFailure::internal(QStringLiteral("All stream retry attempts exhausted")));
}

Result<StreamSession*> Processor::processStreamOnce(const SemanticRequest& request)
{
    IOutboundAdapter* ob = effectiveOutbound();
    IExecutor* ex = effectiveExecutor();

    if (!ob) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("outbound adapter not set")));
    }
    if (!ex) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("executor not set")));
    }

    // Build the provider-level request
    Result<ProviderRequest> provReqResult = ob->buildRequest(request);
    if (!provReqResult.has_value()) {
        return std::unexpected(provReqResult.error());
    }

    // Force stream flag
    ProviderRequest provReq = provReqResult.value();
    provReq.stream = true;

    // Connect the stream via executor -- returns a live QNetworkReply
    Result<QNetworkReply*> replyResult = ex->connectStream(provReq);
    if (!replyResult.has_value()) {
        return std::unexpected(replyResult.error());
    }

    QNetworkReply* reply = replyResult.value();
    if (!reply) {
        return std::unexpected(
            DomainFailure::internal(QStringLiteral("Executor returned null reply")));
    }

    // Wrap the reply in a StreamSession. The session takes ownership.
    auto* session = new StreamSession(reply, ob, provReq.adapterHint, this);
    return session;
}
