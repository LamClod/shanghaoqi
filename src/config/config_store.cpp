#include "config_store.h"
#include "provider_routing.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QSysInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "Crypt32.lib")
#endif
#endif

namespace {

QJsonValue jsonValueEither(const QJsonObject& obj, const char* snakeKey, const char* camelKey)
{
    const QString snake = QString::fromUtf8(snakeKey);
    if (obj.contains(snake))
        return obj.value(snake);
    return obj.value(QString::fromUtf8(camelKey));
}

QString jsonStringEither(const QJsonObject& obj, const char* snakeKey, const char* camelKey)
{
    return jsonValueEither(obj, snakeKey, camelKey).toString();
}

QJsonArray jsonArrayEither(const QJsonObject& obj, const char* snakeKey, const char* camelKey)
{
    return jsonValueEither(obj, snakeKey, camelKey).toArray();
}

int jsonIntEither(const QJsonObject& obj, const char* snakeKey, const char* camelKey, int fallback)
{
    const QJsonValue value = jsonValueEither(obj, snakeKey, camelKey);
    return value.isUndefined() ? fallback : value.toInt(fallback);
}

bool jsonBoolEither(const QJsonObject& obj, const char* snakeKey, const char* camelKey, bool fallback)
{
    const QJsonValue value = jsonValueEither(obj, snakeKey, camelKey);
    return value.isUndefined() ? fallback : value.toBool(fallback);
}

bool mapContainsEither(const QVariantMap& map, const char* snakeKey, const char* camelKey)
{
    return map.contains(QString::fromUtf8(snakeKey)) || map.contains(QString::fromUtf8(camelKey));
}

QVariant mapValueEither(const QVariantMap& map, const char* snakeKey, const char* camelKey)
{
    const QString snake = QString::fromUtf8(snakeKey);
    if (map.contains(snake))
        return map.value(snake);
    return map.value(QString::fromUtf8(camelKey));
}

int clampInt(int value, int minValue, int maxValue)
{
    return qBound(minValue, value, maxValue);
}

#ifdef Q_OS_WIN
QByteArray buildDpapiEntropy()
{
    QByteArray seed;
    seed.append(QCoreApplication::organizationName().toUtf8());
    seed.append('|');
    seed.append(QCoreApplication::applicationName().toUtf8());
    seed.append('|');
    seed.append(QSysInfo::machineUniqueId());
    if (seed.isEmpty()) {
        seed = QByteArrayLiteral("shanghaoqi");
    }
    return QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
}

bool dpapiProtect(const QByteArray& plain, QByteArray& encrypted)
{
    DATA_BLOB inBlob;
    inBlob.cbData = static_cast<DWORD>(plain.size());
    inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()));

    const QByteArray entropyBytes = buildDpapiEntropy();
    DATA_BLOB entropyBlob;
    entropyBlob.cbData = static_cast<DWORD>(entropyBytes.size());
    entropyBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(entropyBytes.data()));

    DATA_BLOB outBlob;
    if (!CryptProtectData(&inBlob,
                          L"shanghaoqi_api_key",
                          &entropyBlob,
                          nullptr,
                          nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN,
                          &outBlob)) {
        return false;
    }

    encrypted = QByteArray(reinterpret_cast<const char*>(outBlob.pbData),
                           static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
}

bool dpapiUnprotect(const QByteArray& encrypted, QByteArray& plain)
{
    DATA_BLOB inBlob;
    inBlob.cbData = static_cast<DWORD>(encrypted.size());
    inBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()));

    const QByteArray entropyBytes = buildDpapiEntropy();
    DATA_BLOB entropyBlob;
    entropyBlob.cbData = static_cast<DWORD>(entropyBytes.size());
    entropyBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(entropyBytes.data()));

    DATA_BLOB outBlob;
    if (!CryptUnprotectData(&inBlob,
                            nullptr,
                            &entropyBlob,
                            nullptr,
                            nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN,
                            &outBlob)) {
        return false;
    }

    plain = QByteArray(reinterpret_cast<const char*>(outBlob.pbData),
                       static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return true;
}
#endif

}

ConfigStore::ConfigStore(QObject* parent)
    : QObject(parent)
{
}

bool ConfigStore::load(const QString& path) {
    m_filePath = path;
    if (m_filePath.isEmpty()) {
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appData);
        m_filePath = appData + "/config.json";
    }
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    QJsonObject root = doc.object();

    // global
    QJsonObject g = root["global"].toObject();
    m_config.global.mappedModelId = jsonStringEither(g, "mapped_model_id", "mappedModelId");
    m_config.global.authKey = jsonStringEither(g, "auth_key", "authKey");
    QJsonArray domains = jsonArrayEither(g, "hijack_domains", "hijackDomains");
    m_config.global.hijackDomains.clear();
    for (const auto& d : domains)
        m_config.global.hijackDomains.append(d.toString());

    // groups
    m_config.groups.clear();
    QJsonArray groups = root["groups"].toArray();
    for (const auto& gv : groups)
        m_config.groups.append(jsonToGroup(gv.toObject()));

    m_config.currentGroupIndex = jsonIntEither(root, "current_group_index", "currentGroupIndex", 0);

    // runtime
    QJsonObject rt = root["runtime"].toObject();
    m_config.runtime.debugMode = jsonBoolEither(rt, "debug_mode", "debugMode", false);
    m_config.runtime.proxyPort = jsonIntEither(rt, "proxy_port", "proxyPort", 443);
    m_config.runtime.upstreamStreamMode = static_cast<StreamMode>(jsonIntEither(rt, "upstream_stream_mode", "upstreamStreamMode", 0));
    m_config.runtime.downstreamStreamMode = static_cast<StreamMode>(jsonIntEither(rt, "downstream_stream_mode", "downstreamStreamMode", 0));
    m_config.runtime.connectionPoolSize = jsonIntEither(rt, "connection_pool_size", "connectionPoolSize", 10);
    m_config.runtime.requestTimeout = jsonIntEither(rt, "request_timeout", "requestTimeout", 120000);
    m_config.runtime.disableSslStrict = jsonBoolEither(rt, "disable_ssl_strict", "disableSslStrict", false);
    m_config.runtime.enableHttp2 = jsonBoolEither(rt, "enable_http2", "enableHttp2", true);
    m_config.runtime.enableConnectionPool = jsonBoolEither(rt, "enable_connection_pool", "enableConnectionPool", true);
    m_config.runtime.connectionTimeout = jsonIntEither(rt, "connection_timeout", "connectionTimeout", 30000);

    emit configChanged();
    return true;
}

bool ConfigStore::save() {
    if (m_filePath.isEmpty())
        return false;

    QJsonObject root;
    root["version"] = 1;

    QJsonObject g;
    g["mapped_model_id"] = m_config.global.mappedModelId;
    g["auth_key"] = m_config.global.authKey;
    QJsonArray domains;
    for (const auto& d : m_config.global.hijackDomains)
        domains.append(d);
    g["hijack_domains"] = domains;
    root["global"] = g;

    QJsonArray groups;
    for (const auto& grp : m_config.groups)
        groups.append(groupToJson(grp));
    root["groups"] = groups;
    root["current_group_index"] = m_config.currentGroupIndex;

    QJsonObject rt;
    rt["debug_mode"] = m_config.runtime.debugMode;
    rt["proxy_port"] = m_config.runtime.proxyPort;
    rt["upstream_stream_mode"] = static_cast<int>(m_config.runtime.upstreamStreamMode);
    rt["downstream_stream_mode"] = static_cast<int>(m_config.runtime.downstreamStreamMode);
    rt["connection_pool_size"] = m_config.runtime.connectionPoolSize;
    rt["request_timeout"] = m_config.runtime.requestTimeout;
    rt["disable_ssl_strict"] = m_config.runtime.disableSslStrict;
    rt["enable_http2"] = m_config.runtime.enableHttp2;
    rt["enable_connection_pool"] = m_config.runtime.enableConnectionPool;
    rt["connection_timeout"] = m_config.runtime.connectionTimeout;
    root["runtime"] = rt;

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

QVariantList ConfigStore::configGroups() const {
    QVariantList list;
    for (const auto& grp : m_config.groups) {
        QVariantMap map;
        map["name"] = grp.name;
        map["provider"] = grp.provider;
        map["outbound_adapter"] = grp.outboundAdapter;
        map["outboundAdapter"] = grp.outboundAdapter;
        map["base_url"] = grp.baseUrl;
        map["baseUrl"] = grp.baseUrl;
        map["model_id"] = grp.modelId;
        map["modelId"] = grp.modelId;
        map["api_key"] = grp.apiKey;
        map["apiKey"] = grp.apiKey;
        map["middle_route"] = grp.middleRoute;
        map["middleRoute"] = grp.middleRoute;
        map["max_retry_attempts"] = grp.maxRetryAttempts;
        map["maxRetryAttempts"] = grp.maxRetryAttempts;
        map["base_url_candidates"] = QVariant(grp.baseUrlCandidates);
        map["baseUrlCandidates"] = QVariant(grp.baseUrlCandidates);
        QVariantMap headerMap;
        for (auto it = grp.customHeaders.cbegin(); it != grp.customHeaders.cend(); ++it)
            headerMap[it.key()] = it.value();
        map["custom_headers"] = headerMap;
        map["customHeaders"] = headerMap;
        map["hijack_domain_override"] = grp.hijackDomainOverride;
        map["hijackDomainOverride"] = grp.hijackDomainOverride;
        list.append(map);
    }
    return list;
}

void ConfigStore::addGroup(const QVariantMap& group) {
    ConfigGroup g;
    g.name = group.value("name").toString();
    g.provider = group.value("provider").toString();
    g.outboundAdapter = mapValueEither(group, "outbound_adapter", "outboundAdapter").toString();
    g.baseUrl = mapValueEither(group, "base_url", "baseUrl").toString();
    g.modelId = mapValueEither(group, "model_id", "modelId").toString();
    g.apiKey = mapValueEither(group, "api_key", "apiKey").toString();
    if (mapContainsEither(group, "middle_route", "middleRoute"))
        g.middleRoute = mapValueEither(group, "middle_route", "middleRoute").toString();
    if (mapContainsEither(group, "max_retry_attempts", "maxRetryAttempts"))
        g.maxRetryAttempts = mapValueEither(group, "max_retry_attempts", "maxRetryAttempts").toInt();
    if (mapContainsEither(group, "base_url_candidates", "baseUrlCandidates"))
        g.baseUrlCandidates = mapValueEither(group, "base_url_candidates", "baseUrlCandidates").toStringList();
    if (mapContainsEither(group, "custom_headers", "customHeaders")) {
        QVariantMap hm = mapValueEither(group, "custom_headers", "customHeaders").toMap();
        for (auto it = hm.cbegin(); it != hm.cend(); ++it)
            g.customHeaders[it.key()] = it.value().toString();
    }
    if (mapContainsEither(group, "hijack_domain_override", "hijackDomainOverride"))
        g.hijackDomainOverride = mapValueEither(group, "hijack_domain_override", "hijackDomainOverride").toString();
    m_config.groups.append(g);
    save();
    emit configChanged();
}

void ConfigStore::updateGroup(int index, const QVariantMap& group) {
    if (index < 0 || index >= m_config.groups.size())
        return;
    auto& g = m_config.groups[index];
    if (group.contains("name"))
        g.name = group.value("name").toString();
    if (group.contains("provider"))
        g.provider = group.value("provider").toString();
    if (mapContainsEither(group, "outbound_adapter", "outboundAdapter"))
        g.outboundAdapter = mapValueEither(group, "outbound_adapter", "outboundAdapter").toString();
    if (mapContainsEither(group, "base_url", "baseUrl"))
        g.baseUrl = mapValueEither(group, "base_url", "baseUrl").toString();
    if (mapContainsEither(group, "model_id", "modelId"))
        g.modelId = mapValueEither(group, "model_id", "modelId").toString();
    if (mapContainsEither(group, "api_key", "apiKey"))
        g.apiKey = mapValueEither(group, "api_key", "apiKey").toString();
    if (mapContainsEither(group, "middle_route", "middleRoute"))
        g.middleRoute = mapValueEither(group, "middle_route", "middleRoute").toString();
    if (mapContainsEither(group, "max_retry_attempts", "maxRetryAttempts"))
        g.maxRetryAttempts = mapValueEither(group, "max_retry_attempts", "maxRetryAttempts").toInt();
    if (mapContainsEither(group, "base_url_candidates", "baseUrlCandidates"))
        g.baseUrlCandidates = mapValueEither(group, "base_url_candidates", "baseUrlCandidates").toStringList();
    if (mapContainsEither(group, "custom_headers", "customHeaders")) {
        QVariantMap hm = mapValueEither(group, "custom_headers", "customHeaders").toMap();
        for (auto it = hm.cbegin(); it != hm.cend(); ++it)
            g.customHeaders[it.key()] = it.value().toString();
    }
    if (mapContainsEither(group, "hijack_domain_override", "hijackDomainOverride"))
        g.hijackDomainOverride = mapValueEither(group, "hijack_domain_override", "hijackDomainOverride").toString();
    save();
    emit configChanged();
}

void ConfigStore::removeGroup(int index) {
    if (index < 0 || index >= m_config.groups.size())
        return;
    m_config.groups.removeAt(index);
    if (m_config.currentGroupIndex >= m_config.groups.size())
        m_config.currentGroupIndex = qMax(0, m_config.groups.size() - 1);
    save();
    emit configChanged();
}

int ConfigStore::currentGroupIndex() const {
    return m_config.currentGroupIndex;
}

void ConfigStore::setCurrentGroupIndex(int index) {
    if (index >= 0 && index < m_config.groups.size()) {
        m_config.currentGroupIndex = index;
        save();
        emit configChanged();
    }
}

ConfigGroup ConfigStore::groupAt(int index) const {
    if (index >= 0 && index < m_config.groups.size())
        return m_config.groups[index];
    return {};
}

QString ConfigStore::mappedModelId() const {
    return m_config.global.mappedModelId;
}

void ConfigStore::setMappedModelId(const QString& id) {
    m_config.global.mappedModelId = id;
    save();
    emit configChanged();
}

QString ConfigStore::authKey() const {
    return m_config.global.authKey;
}

void ConfigStore::setAuthKey(const QString& key) {
    m_config.global.authKey = key;
    save();
    emit configChanged();
}

QVariantMap ConfigStore::runtimeOptions() const {
    QVariantMap map;
    map["debug_mode"] = m_config.runtime.debugMode;
    map["debugMode"] = m_config.runtime.debugMode;
    map["proxy_port"] = m_config.runtime.proxyPort;
    map["proxyPort"] = m_config.runtime.proxyPort;
    map["upstream_stream_mode"] = static_cast<int>(m_config.runtime.upstreamStreamMode);
    map["upstreamStreamMode"] = static_cast<int>(m_config.runtime.upstreamStreamMode);
    map["downstream_stream_mode"] = static_cast<int>(m_config.runtime.downstreamStreamMode);
    map["downstreamStreamMode"] = static_cast<int>(m_config.runtime.downstreamStreamMode);
    map["connection_pool_size"] = m_config.runtime.connectionPoolSize;
    map["connectionPoolSize"] = m_config.runtime.connectionPoolSize;
    map["request_timeout"] = m_config.runtime.requestTimeout;
    map["requestTimeout"] = m_config.runtime.requestTimeout;
    map["disable_ssl_strict"] = m_config.runtime.disableSslStrict;
    map["disableSslStrict"] = m_config.runtime.disableSslStrict;
    map["enable_http2"] = m_config.runtime.enableHttp2;
    map["enableHttp2"] = m_config.runtime.enableHttp2;
    map["enable_connection_pool"] = m_config.runtime.enableConnectionPool;
    map["enableConnectionPool"] = m_config.runtime.enableConnectionPool;
    map["connection_timeout"] = m_config.runtime.connectionTimeout;
    map["connectionTimeout"] = m_config.runtime.connectionTimeout;
    return map;
}

void ConfigStore::setRuntimeOptions(const QVariantMap& opts) {
    if (mapContainsEither(opts, "debug_mode", "debugMode"))
        m_config.runtime.debugMode = mapValueEither(opts, "debug_mode", "debugMode").toBool();
    if (mapContainsEither(opts, "proxy_port", "proxyPort"))
        m_config.runtime.proxyPort = clampInt(mapValueEither(opts, "proxy_port", "proxyPort").toInt(), 1, 65535);
    if (mapContainsEither(opts, "upstream_stream_mode", "upstreamStreamMode"))
        m_config.runtime.upstreamStreamMode = static_cast<StreamMode>(clampInt(mapValueEither(opts, "upstream_stream_mode", "upstreamStreamMode").toInt(),
                                                                                static_cast<int>(StreamMode::FollowClient),
                                                                                static_cast<int>(StreamMode::ForceOff)));
    if (mapContainsEither(opts, "downstream_stream_mode", "downstreamStreamMode"))
        m_config.runtime.downstreamStreamMode = static_cast<StreamMode>(clampInt(mapValueEither(opts, "downstream_stream_mode", "downstreamStreamMode").toInt(),
                                                                                  static_cast<int>(StreamMode::FollowClient),
                                                                                  static_cast<int>(StreamMode::ForceOff)));
    if (mapContainsEither(opts, "connection_pool_size", "connectionPoolSize"))
        m_config.runtime.connectionPoolSize = clampInt(mapValueEither(opts, "connection_pool_size", "connectionPoolSize").toInt(), 1, 200);
    if (mapContainsEither(opts, "request_timeout", "requestTimeout"))
        m_config.runtime.requestTimeout = clampInt(mapValueEither(opts, "request_timeout", "requestTimeout").toInt(), 1000, 600000);
    if (mapContainsEither(opts, "disable_ssl_strict", "disableSslStrict"))
        m_config.runtime.disableSslStrict = mapValueEither(opts, "disable_ssl_strict", "disableSslStrict").toBool();
    if (mapContainsEither(opts, "enable_http2", "enableHttp2"))
        m_config.runtime.enableHttp2 = mapValueEither(opts, "enable_http2", "enableHttp2").toBool();
    if (mapContainsEither(opts, "enable_connection_pool", "enableConnectionPool"))
        m_config.runtime.enableConnectionPool = mapValueEither(opts, "enable_connection_pool", "enableConnectionPool").toBool();
    if (mapContainsEither(opts, "connection_timeout", "connectionTimeout"))
        m_config.runtime.connectionTimeout = clampInt(mapValueEither(opts, "connection_timeout", "connectionTimeout").toInt(), 500, 300000);
    save();
    emit configChanged();
}

QString ConfigStore::encryptApiKey(const QString& plain) const {
    if (plain.isEmpty()) {
        return QString();
    }

#ifdef Q_OS_WIN
    QByteArray encrypted;
    if (dpapiProtect(plain.toUtf8(), encrypted)) {
        return QStringLiteral("DPAPI:") + QString::fromUtf8(encrypted.toBase64());
    }
#endif

    return QStringLiteral("ENC:") + QString::fromUtf8(plain.toUtf8().toBase64());
}

QString ConfigStore::decryptApiKey(const QString& cipher) const {
    if (cipher.startsWith(QStringLiteral("DPAPI:"))) {
#ifdef Q_OS_WIN
        const QByteArray payload = QByteArray::fromBase64(cipher.mid(6).toUtf8());
        QByteArray plain;
        if (dpapiUnprotect(payload, plain)) {
            return QString::fromUtf8(plain);
        }
#endif
        return QString();
    }

    if (cipher.startsWith(QStringLiteral("ENC:")))
        return QString::fromUtf8(QByteArray::fromBase64(cipher.mid(4).toUtf8()));

    return cipher;
}

QString ConfigStore::encodeApiKeyForExternal(const QString& plain) const
{
    if (plain.isEmpty()) {
        return QString();
    }

    // External import/export should stay machine-portable.
    return QStringLiteral("ENC:") + QString::fromUtf8(plain.toUtf8().toBase64());
}

QString ConfigStore::decodeApiKeyFromExternal(const QString& encoded) const
{
    return decryptApiKey(encoded);
}

QJsonObject ConfigStore::groupToJson(const ConfigGroup& g) const {
    QJsonObject obj;
    obj["name"] = g.name;
    obj["provider"] = g.provider;
    obj["outbound_adapter"] = g.outboundAdapter;
    obj["base_url"] = g.baseUrl;
    QJsonArray candidates;
    for (const auto& u : g.baseUrlCandidates)
        candidates.append(u);
    obj["base_url_candidates"] = candidates;
    obj["model_id"] = g.modelId;
    obj["api_key"] = encryptApiKey(g.apiKey);
    obj["middle_route"] = g.middleRoute;
    obj["max_retry_attempts"] = g.maxRetryAttempts;
    QJsonObject headers;
    for (auto it = g.customHeaders.cbegin(); it != g.customHeaders.cend(); ++it)
        headers[it.key()] = it.value();
    if (!headers.isEmpty())
        obj["custom_headers"] = headers;
    if (!g.hijackDomainOverride.isEmpty())
        obj["hijack_domain_override"] = g.hijackDomainOverride;
    return obj;
}

ConfigGroup ConfigStore::jsonToGroup(const QJsonObject& obj) const {
    ConfigGroup g;
    g.name = obj["name"].toString();
    g.provider = provider_routing::migrateProviderField(obj["provider"].toString());
    g.outboundAdapter = jsonStringEither(obj, "outbound_adapter", "outboundAdapter");
    g.baseUrl = jsonStringEither(obj, "base_url", "baseUrl");
    QJsonArray candidates = jsonArrayEither(obj, "base_url_candidates", "baseUrlCandidates");
    for (const auto& c : candidates)
        g.baseUrlCandidates.append(c.toString());
    g.modelId = jsonStringEither(obj, "model_id", "modelId");
    g.apiKey = decryptApiKey(jsonStringEither(obj, "api_key", "apiKey"));
    {
        const QJsonValue middleRoute = jsonValueEither(obj, "middle_route", "middleRoute");
        g.middleRoute = middleRoute.isUndefined() ? QStringLiteral("/v1") : middleRoute.toString(QStringLiteral("/v1"));
    }
    g.maxRetryAttempts = jsonIntEither(obj, "max_retry_attempts", "maxRetryAttempts", 3);
    QJsonObject headers = jsonValueEither(obj, "custom_headers", "customHeaders").toObject();
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        g.customHeaders[it.key()] = it.value().toString();
    g.hijackDomainOverride = jsonStringEither(obj, "hijack_domain_override", "hijackDomainOverride");
    return g;
}

QStringList ConfigStore::hijackDomains() const {
    return m_config.global.hijackDomains;
}

void ConfigStore::setHijackDomains(const QStringList& domains) {
    m_config.global.hijackDomains = domains;
    save();
    emit configChanged();
}
