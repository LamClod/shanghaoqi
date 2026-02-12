#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QStringList>

namespace model_list_utils {

inline QString normalizeModelId(const QJsonObject& modelObj)
{
    QString modelId = modelObj.value(QStringLiteral("id")).toString();
    if (modelId.isEmpty()) {
        modelId = modelObj.value(QStringLiteral("name")).toString();
    }
    if (modelId.startsWith(QStringLiteral("models/"))) {
        modelId = modelId.mid(7);
    }
    return modelId;
}

inline QStringList parseModelIds(const QByteArray& rawBody)
{
    QStringList modelIds;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(rawBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return modelIds;
    }

    const QJsonObject root = doc.object();

    const auto appendModels = [&](const QJsonArray& array, QStringList& target) {
        for (const QJsonValue& item : array) {
            if (!item.isObject()) {
                continue;
            }
            const QString id = normalizeModelId(item.toObject());
            if (!id.isEmpty() && !target.contains(id)) {
                target.append(id);
            }
        }
    };

    appendModels(root.value(QStringLiteral("data")).toArray(), modelIds);

    if (modelIds.isEmpty()) {
        appendModels(root.value(QStringLiteral("models")).toArray(), modelIds);
    }

    if (modelIds.isEmpty() && root.contains(QStringLiteral("result"))) {
        const QJsonObject resultObj = root.value(QStringLiteral("result")).toObject();
        appendModels(resultObj.value(QStringLiteral("models")).toArray(), modelIds);
    }

    modelIds.sort();
    return modelIds;
}

} // namespace model_list_utils

