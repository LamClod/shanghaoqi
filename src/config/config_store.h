#pragma once
#include "config_types.h"
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class ConfigStore : public QObject {
    Q_OBJECT

public:
    explicit ConfigStore(QObject* parent = nullptr);

    bool load(const QString& path);
    bool save();

    QVariantList configGroups() const;
    void addGroup(const QVariantMap& group);
    void updateGroup(int index, const QVariantMap& group);
    void removeGroup(int index);
    int currentGroupIndex() const;
    void setCurrentGroupIndex(int index);

    const QList<ConfigGroup>& groups() const { return m_config.groups; }
    ConfigGroup groupAt(int index) const;

    QString mappedModelId() const;
    void setMappedModelId(const QString& id);
    QString authKey() const;
    void setAuthKey(const QString& key);

    QStringList hijackDomains() const;
    void setHijackDomains(const QStringList& domains);

    QVariantMap runtimeOptions() const;
    void setRuntimeOptions(const QVariantMap& opts);

    QString encodeApiKeyForExternal(const QString& plain) const;
    QString decodeApiKeyFromExternal(const QString& encoded) const;

    ProxyConfig proxyConfig() const { return m_config; }
    RuntimeOptions runtimeConfig() const { return m_config.runtime; }

signals:
    void configChanged();

private:
    ProxyConfig m_config;
    QString m_filePath;
    QString encryptApiKey(const QString& plain) const;
    QString decryptApiKey(const QString& cipher) const;
    QJsonObject groupToJson(const ConfigGroup& g) const;
    ConfigGroup jsonToGroup(const QJsonObject& obj) const;
};
