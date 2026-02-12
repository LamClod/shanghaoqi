#pragma once

#include "config_types.h"
#include "provider_routing.h"
#include "semantic/ports.h"

#include <QMap>
#include <QNetworkRequest>
#include <QUrl>

namespace model_list_request_builder {

struct DownstreamHeaders {
    QString authorization;
    QString xApiKey;
    QString xGoogApiKey;
    QString anthropicVersion;
    QString anthropicBeta;
};

struct Context {
    provider_routing::ModelListProvider provider = provider_routing::ModelListProvider::OpenAICompat;
    QUrl upstreamUrl;
    QString effectiveApiKey;
    QString keySource = QStringLiteral("group");
    QString incomingAuthorization;
    QString anthropicVersion;
    QString anthropicBeta;
    QMap<QString, QString> customHeaders;
    QStringList authModes;

    bool isValid() const { return upstreamUrl.isValid() && !upstreamUrl.isEmpty(); }
};

Context buildContext(const ConfigGroup& group,
                     const DownstreamHeaders& incoming = {},
                     const QString& localAuthKey = QString());

ProviderRequest makeProviderRequest(const Context& context, const QString& authMode);
QNetworkRequest makeNetworkRequest(const Context& context,
                                   const QString& authMode,
                                   int transferTimeoutMs = 15000);

} // namespace model_list_request_builder

