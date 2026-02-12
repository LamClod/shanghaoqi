#include "zai.h"

QString ZaiOutbound::adapterId() const
{
    return QStringLiteral("zai");
}

Result<ProviderRequest> ZaiOutbound::buildRequest(const SemanticRequest& request)
{
    // ZhipuAI uses OpenAI-compatible format. Inject default base URL if not set.
    if (!request.metadata.contains(QStringLiteral("provider_base_url"))
        || request.metadata.value(QStringLiteral("provider_base_url")).isEmpty()) {
        SemanticRequest modified = request;
        modified.metadata[QStringLiteral("provider_base_url")] =
            QStringLiteral("https://open.bigmodel.cn/api/paas/v4");
        return OpenAIOutbound::buildRequest(modified);
    }
    return OpenAIOutbound::buildRequest(request);
}
