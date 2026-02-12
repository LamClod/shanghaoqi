#include "openai_compat.h"

OpenAICompatOutbound::OpenAICompatOutbound(const QString& id, const QString& defaultBaseUrl,
                                           const QString& defaultMiddleRoute)
    : m_id(id)
    , m_defaultBaseUrl(defaultBaseUrl)
    , m_defaultMiddleRoute(defaultMiddleRoute)
{
}

QString OpenAICompatOutbound::adapterId() const
{
    return m_id;
}

Result<ProviderRequest> OpenAICompatOutbound::buildRequest(const SemanticRequest& request)
{
    SemanticRequest modified = request;
    if (!modified.metadata.contains(QStringLiteral("provider_base_url"))
        || modified.metadata.value(QStringLiteral("provider_base_url")).isEmpty()) {
        modified.metadata[QStringLiteral("provider_base_url")] = m_defaultBaseUrl;
    }
    if (!modified.metadata.contains(QStringLiteral("middle_route"))
        || modified.metadata.value(QStringLiteral("middle_route")).isEmpty()) {
        modified.metadata[QStringLiteral("middle_route")] = m_defaultMiddleRoute;
    }
    return OpenAIOutbound::buildRequest(modified);
}
