#include "model_list_request_builder.h"

#include <QUrlQuery>

namespace model_list_request_builder {

namespace {

QString extractBearerToken(const QString& authorization)
{
    const QString prefix = QStringLiteral("Bearer ");
    if (authorization.startsWith(prefix, Qt::CaseInsensitive)) {
        return authorization.mid(prefix.size()).trimmed();
    }
    return QString();
}

} // namespace

Context buildContext(const ConfigGroup& group,
                     const DownstreamHeaders& incoming,
                     const QString& localAuthKey)
{
    Context context;
    context.provider = provider_routing::detectModelListProvider(group);
    context.customHeaders = group.customHeaders;

    QString baseUrl = group.baseUrl.trimmed();
    QString middleRoute = provider_routing::effectiveMiddleRoute(group, context.provider);
    if (!middleRoute.isEmpty() && baseUrl.endsWith(middleRoute)) {
        middleRoute.clear();
    }

    QUrl upstreamUrl(baseUrl + middleRoute + QStringLiteral("/models"));

    context.incomingAuthorization = incoming.authorization.trimmed();
    context.anthropicVersion = incoming.anthropicVersion.trimmed();
    context.anthropicBeta = incoming.anthropicBeta.trimmed();

    context.effectiveApiKey = group.apiKey;
    const QString bearerToken = extractBearerToken(context.incomingAuthorization);
    const QString normalizedLocalAuth = localAuthKey.trimmed();

    auto tryUseIncomingKey = [&](const QString& token, const QString& sourceName) {
        const QString normalizedToken = token.trimmed();
        if (normalizedToken.isEmpty()) {
            return false;
        }
        if (!normalizedLocalAuth.isEmpty()
            && normalizedToken == normalizedLocalAuth
            && !group.apiKey.isEmpty()) {
            return false;
        }

        context.effectiveApiKey = normalizedToken;
        context.keySource = sourceName;
        return true;
    };

    if (!tryUseIncomingKey(incoming.xGoogApiKey, QStringLiteral("x-goog-api-key"))) {
        if (!tryUseIncomingKey(incoming.xApiKey, QStringLiteral("x-api-key"))) {
            tryUseIncomingKey(bearerToken, QStringLiteral("authorization"));
        }
    }

    if (context.provider == provider_routing::ModelListProvider::Gemini
        && !context.effectiveApiKey.isEmpty()) {
        QUrlQuery query(upstreamUrl);
        query.addQueryItem(QStringLiteral("key"), context.effectiveApiKey);
        upstreamUrl.setQuery(query);
    }

    context.upstreamUrl = upstreamUrl;
    context.authModes = provider_routing::authModesForModelList(context.provider);
    return context;
}

ProviderRequest makeProviderRequest(const Context& context, const QString& authMode)
{
    ProviderRequest req;
    req.method = QStringLiteral("GET");
    req.url = context.upstreamUrl.toString(QUrl::FullyEncoded);

    if (authMode == QStringLiteral("anthropic")) {
        if (!context.effectiveApiKey.isEmpty()) {
            req.headers[QStringLiteral("x-api-key")] = context.effectiveApiKey;
        }

        req.headers[QStringLiteral("anthropic-version")] =
            context.anthropicVersion.isEmpty()
                ? QStringLiteral("2023-06-01")
                : context.anthropicVersion;

        if (!context.anthropicBeta.isEmpty()) {
            req.headers[QStringLiteral("anthropic-beta")] = context.anthropicBeta;
        }
    } else if (authMode == QStringLiteral("gemini")) {
        if (!context.effectiveApiKey.isEmpty()) {
            req.headers[QStringLiteral("x-goog-api-key")] = context.effectiveApiKey;
        }
    } else {
        if (!context.effectiveApiKey.isEmpty()) {
            req.headers[QStringLiteral("Authorization")] =
                QStringLiteral("Bearer ") + context.effectiveApiKey;
        } else if (!context.incomingAuthorization.isEmpty()) {
            req.headers[QStringLiteral("Authorization")] = context.incomingAuthorization;
        }
    }

    for (auto it = context.customHeaders.cbegin(); it != context.customHeaders.cend(); ++it) {
        req.headers[it.key()] = it.value();
    }

    return req;
}

QNetworkRequest makeNetworkRequest(const Context& context,
                                   const QString& authMode,
                                   int transferTimeoutMs)
{
    const ProviderRequest providerReq = makeProviderRequest(context, authMode);

    QNetworkRequest req(QUrl(providerReq.url));
    for (auto it = providerReq.headers.cbegin(); it != providerReq.headers.cend(); ++it) {
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
    req.setTransferTimeout(transferTimeoutMs);
    return req;
}

} // namespace model_list_request_builder

