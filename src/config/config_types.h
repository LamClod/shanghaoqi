#pragma once
#include "semantic/types.h"
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>

struct ConfigGroup {
    QString name;
    QString provider;              // inbound adapter ID (e.g. "openai", "anthropic")
    QString outboundAdapter;       // outbound adapter ID (empty = auto-detect)
    QString baseUrl;
    QStringList baseUrlCandidates;
    QString modelId;
    QString apiKey;
    QString middleRoute = "/v1";
    int maxRetryAttempts = 3;
    QMap<QString, QString> customHeaders;
    QString hijackDomainOverride;  // if non-empty, overrides auto-derived hijack domain

    bool isValid() const {
        return !baseUrl.isEmpty() && !modelId.isEmpty() && !apiKey.isEmpty();
    }
};

struct RuntimeOptions {
    bool debugMode = false;
    bool disableSslStrict = false;
    bool enableHttp2 = true;
    bool enableConnectionPool = true;
    StreamMode upstreamStreamMode = StreamMode::FollowClient;
    StreamMode downstreamStreamMode = StreamMode::FollowClient;
    int proxyPort = 443;
    int connectionPoolSize = 10;
    int requestTimeout = 120000;
    int connectionTimeout = 30000;
};

struct GlobalConfig {
    QString mappedModelId;
    QString authKey;
    QStringList hijackDomains;
};

struct ProxyConfig {
    GlobalConfig global;
    QList<ConfigGroup> groups;
    int currentGroupIndex = 0;
    RuntimeOptions runtime;
    QString certPath;
    QString keyPath;

    const ConfigGroup& currentGroup() const {
        static ConfigGroup empty;
        if (currentGroupIndex >= 0 && currentGroupIndex < groups.size())
            return groups[currentGroupIndex];
        return empty;
    }

    bool isValid() const {
        return !groups.isEmpty() && currentGroup().isValid();
    }
};
