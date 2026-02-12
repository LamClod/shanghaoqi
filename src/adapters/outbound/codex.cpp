#include "codex.h"

QString CodexOutbound::adapterId() const
{
    return QStringLiteral("codex");
}

Result<ProviderRequest> CodexOutbound::buildRequest(const SemanticRequest& request)
{
    // Codex uses OpenAI format. Inject default base URL if not set.
    if (!request.metadata.contains(QStringLiteral("provider_base_url"))
        || request.metadata.value(QStringLiteral("provider_base_url")).isEmpty()) {
        SemanticRequest modified = request;
        modified.metadata[QStringLiteral("provider_base_url")] =
            QStringLiteral("https://api.openai.com/v1");
        return OpenAIOutbound::buildRequest(modified);
    }
    return OpenAIOutbound::buildRequest(request);
}
