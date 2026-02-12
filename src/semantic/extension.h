#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonValue>

struct ExtensionBag {
    QJsonObject data;

    void set(const QString& key, const QJsonValue& value) { data[key] = value; }
    QJsonValue get(const QString& key) const { return data.value(key); }
    bool has(const QString& key) const { return data.contains(key); }
    void remove(const QString& key) { data.remove(key); }
};
