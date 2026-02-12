#include "bailian.h"

QString BailianOutbound::adapterId() const
{
    return QStringLiteral("bailian");
}

Result<ProviderRequest> BailianOutbound::buildRequest(const SemanticRequest& request)
{
    // Bailian (Aliyun DashScope) uses OpenAI-compatible format.
    // Inject default base URL if not set.
    if (!request.metadata.contains(QStringLiteral("provider_base_url"))
        || request.metadata.value(QStringLiteral("provider_base_url")).isEmpty()) {
        SemanticRequest modified = request;
        modified.metadata[QStringLiteral("provider_base_url")] =
            QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1");
        return OpenAIOutbound::buildRequest(modified);
    }
    return OpenAIOutbound::buildRequest(request);
}
