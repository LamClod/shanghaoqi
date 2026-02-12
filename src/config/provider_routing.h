#pragma once

#include "config_types.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace provider_routing {

enum class ModelListProvider {
    OpenAICompat,
    Anthropic,
    Gemini
};

inline ModelListProvider detectModelListProvider(const QString& providerText,
                                                 const QString& baseUrlText)
{
    const QString provider = providerText.trimmed().toLower();
    const QString baseUrl = baseUrlText.trimmed().toLower();

    if (provider.contains(QStringLiteral("anthropic"))
        || provider.contains(QStringLiteral("claudecode"))
        || baseUrl.contains(QStringLiteral("anthropic"))) {
        return ModelListProvider::Anthropic;
    }

    if (provider.contains(QStringLiteral("gemini"))
        || baseUrl.contains(QStringLiteral("generativelanguage.googleapis.com"))) {
        return ModelListProvider::Gemini;
    }

    return ModelListProvider::OpenAICompat;
}

inline ModelListProvider detectModelListProvider(const ConfigGroup& group)
{
    return detectModelListProvider(group.provider, group.baseUrl);
}

inline QString effectiveMiddleRoute(const QString& middleRoute,
                                    ModelListProvider provider)
{
    const QString trimmed = middleRoute.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }

    if (provider == ModelListProvider::Gemini) {
        return QStringLiteral("/v1beta");
    }

    return QStringLiteral("/v1");
}

inline QString effectiveMiddleRoute(const ConfigGroup& group,
                                    ModelListProvider provider)
{
    return effectiveMiddleRoute(group.middleRoute, provider);
}

inline QStringList authModesForModelList(ModelListProvider provider)
{
    QStringList authModes;
    switch (provider) {
    case ModelListProvider::Anthropic:
        authModes << QStringLiteral("bearer") << QStringLiteral("anthropic");
        break;
    case ModelListProvider::Gemini:
        authModes << QStringLiteral("gemini") << QStringLiteral("bearer");
        break;
    case ModelListProvider::OpenAICompat:
    default:
        authModes << QStringLiteral("bearer") << QStringLiteral("anthropic");
        break;
    }
    authModes.removeDuplicates();
    return authModes;
}

inline QString extractHostFromText(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QString();
    }

    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("https://"));
    }

    const QUrl parsed(text);
    QString host = parsed.host().trimmed().toLower();
    if (!host.isEmpty()) {
        return host;
    }

    QString fallback = text;
    int slashPos = fallback.indexOf(QLatin1Char('/'));
    if (slashPos >= 0) {
        fallback = fallback.left(slashPos);
    }
    int colonPos = fallback.indexOf(QLatin1Char(':'));
    if (colonPos >= 0) {
        fallback = fallback.left(colonPos);
    }
    return fallback.trimmed().toLower();
}

inline QString canonicalHijackDomain(const QString& providerText,
                                     const QString& baseUrlText)
{
    const QString providerHost = extractHostFromText(providerText);
    const QString baseHost = extractHostFromText(baseUrlText);

    auto looksLikeDomain = [](const QString& host) {
        return host.contains(QLatin1Char('.'));
    };

    if (looksLikeDomain(providerHost)) {
        return providerHost;
    }

    if (looksLikeDomain(baseHost)) {
        return baseHost;
    }

    if (!providerHost.isEmpty()) {
        return providerHost;
    }

    return baseHost;
}

inline QString defaultHijackDomain(const QString& inboundAdapterId)
{
    static const QMap<QString, QString> mapping = {
        {QStringLiteral("openai"),           QStringLiteral("api.openai.com")},
        {QStringLiteral("openai.responses"), QStringLiteral("api.openai.com")},
        {QStringLiteral("anthropic"),        QStringLiteral("api.anthropic.com")},
        {QStringLiteral("gemini"),           QStringLiteral("generativelanguage.googleapis.com")},
        {QStringLiteral("aisdk"),            QStringLiteral("api.openai.com")},
        {QStringLiteral("jina"),             QStringLiteral("api.jina.ai")},
        {QStringLiteral("codex"),            QStringLiteral("api.openai.com")},
        {QStringLiteral("claudecode"),       QStringLiteral("api.anthropic.com")},
        {QStringLiteral("antigravity"),      QStringLiteral("api.antigravity.ai")},
    };
    return mapping.value(inboundAdapterId.trimmed().toLower(),
                         QStringLiteral("api.openai.com"));
}

inline QString migrateProviderField(const QString& oldProvider)
{
    const QString lc = oldProvider.trimmed().toLower();
    if (lc.isEmpty()) {
        return QStringLiteral("openai");
    }
    if (!lc.contains(QLatin1Char('.'))) {
        return lc; // already an adapter ID
    }
    if (lc.contains(QStringLiteral("anthropic")))   return QStringLiteral("anthropic");
    if (lc.contains(QStringLiteral("googleapis"))
        || lc.contains(QStringLiteral("gemini")))    return QStringLiteral("gemini");
    if (lc.contains(QStringLiteral("antigravity")))  return QStringLiteral("antigravity");
    if (lc.contains(QStringLiteral("jina")))         return QStringLiteral("jina");
    return QStringLiteral("openai");
}

inline QString canonicalHijackDomain(const ConfigGroup& group)
{
    // 1. Explicit override wins
    if (!group.hijackDomainOverride.trimmed().isEmpty()) {
        return group.hijackDomainOverride.trimmed().toLower();
    }
    // 2. Derive from inbound adapter ID (no dots = adapter ID)
    const QString provider = group.provider.trimmed().toLower();
    if (!provider.isEmpty() && !provider.contains(QLatin1Char('.'))) {
        return defaultHijackDomain(provider);
    }
    // 3. Legacy fallback: provider is a domain string
    return canonicalHijackDomain(provider, group.baseUrl);
}

} // namespace provider_routing

