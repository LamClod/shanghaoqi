#include "modelscope.h"

QString ModelScopeOutbound::adapterId() const
{
    return QStringLiteral("modelscope");
}

Result<ProviderRequest> ModelScopeOutbound::buildRequest(const SemanticRequest& request)
{
    // ModelScope uses OpenAI-compatible format.
    // Inject default base URL if not set.
    if (!request.metadata.contains(QStringLiteral("provider_base_url"))
        || request.metadata.value(QStringLiteral("provider_base_url")).isEmpty()) {
        SemanticRequest modified = request;
        modified.metadata[QStringLiteral("provider_base_url")] =
            QStringLiteral("https://api-inference.modelscope.cn/v1");
        return OpenAIOutbound::buildRequest(modified);
    }
    return OpenAIOutbound::buildRequest(request);
}
