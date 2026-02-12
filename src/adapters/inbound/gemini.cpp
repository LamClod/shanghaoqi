#include "adapters/inbound/gemini.h"
#include <QUuid>
#include <QDateTime>

QString GeminiAdapter::protocol() const
{
    return QStringLiteral("gemini");
}

QString GeminiAdapter::finishReasonFromCause(StopCause cause)
{
    switch (cause) {
    case StopCause::Completed:     return QStringLiteral("STOP");
    case StopCause::Length:        return QStringLiteral("MAX_TOKENS");
    case StopCause::ContentFilter: return QStringLiteral("SAFETY");
    case StopCause::ToolCall:      return QStringLiteral("STOP");
    }
    return QStringLiteral("STOP");
}

QList<Segment> GeminiAdapter::parseParts(const QJsonArray& parts)
{
    QList<Segment> segments;
    for (const QJsonValue& pv : parts) {
        const QJsonObject part = pv.toObject();
        if (part.contains(QStringLiteral("text"))) {
            segments.append(Segment::fromText(part[QStringLiteral("text")].toString()));
        } else if (part.contains(QStringLiteral("inlineData"))) {
            const QJsonObject inlineData = part[QStringLiteral("inlineData")].toObject();
            MediaRef ref;
            ref.mimeType = inlineData[QStringLiteral("mimeType")].toString();
            ref.inlineData = QByteArray::fromBase64(
                inlineData[QStringLiteral("data")].toString().toUtf8());
            segments.append(Segment::fromMedia(ref));
        } else if (part.contains(QStringLiteral("fileData"))) {
            const QJsonObject fileData = part[QStringLiteral("fileData")].toObject();
            MediaRef ref;
            ref.mimeType = fileData[QStringLiteral("mimeType")].toString();
            ref.uri = fileData[QStringLiteral("fileUri")].toString();
            segments.append(Segment::fromMedia(ref));
        }
    }
    return segments;
}

QJsonArray GeminiAdapter::serializeParts(const QList<Segment>& segments)
{
    QJsonArray parts;
    for (const Segment& seg : segments) {
        QJsonObject part;
        if (seg.kind == SegmentKind::Text) {
            part[QStringLiteral("text")] = seg.text;
        } else if (seg.kind == SegmentKind::Media) {
            if (!seg.media.inlineData.isEmpty()) {
                QJsonObject inlineData;
                inlineData[QStringLiteral("mimeType")] = seg.media.mimeType;
                inlineData[QStringLiteral("data")] = QString::fromUtf8(seg.media.inlineData.toBase64());
                part[QStringLiteral("inlineData")] = inlineData;
            } else {
                QJsonObject fileData;
                fileData[QStringLiteral("mimeType")] = seg.media.mimeType;
                fileData[QStringLiteral("fileUri")] = seg.media.uri;
                part[QStringLiteral("fileData")] = fileData;
            }
        }
        parts.append(part);
    }
    return parts;
}

Result<SemanticRequest> GeminiAdapter::decodeRequest(
    const QByteArray& body,
    const QMap<QString, QString>& metadata)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::unexpected(DomainFailure::invalidInput(
            QStringLiteral("invalid_json"),
            QStringLiteral("Request body is not valid JSON: %1").arg(parseErr.errorString())));
    }

    const QJsonObject root = doc.object();
    SemanticRequest req;
    req.envelope.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Model comes from metadata or URL typically, but check root too
    if (root.contains(QStringLiteral("model")))
        req.target.logicalModel = root[QStringLiteral("model")].toString();

    // System instruction
    if (root.contains(QStringLiteral("systemInstruction"))) {
        const QJsonObject sysObj = root[QStringLiteral("systemInstruction")].toObject();
        InteractionItem sysItem;
        sysItem.role = QStringLiteral("system");
        const QJsonArray sysParts = sysObj[QStringLiteral("parts")].toArray();
        sysItem.content = parseParts(sysParts);
        req.messages.append(sysItem);
    }

    // Parse contents
    const QJsonArray contents = root[QStringLiteral("contents")].toArray();
    for (const QJsonValue& cv : contents) {
        const QJsonObject contentObj = cv.toObject();
        InteractionItem item;
        const QString role = contentObj[QStringLiteral("role")].toString();
        if (role == QStringLiteral("model")) {
            item.role = QStringLiteral("assistant");
        } else {
            item.role = role.isEmpty() ? QStringLiteral("user") : role;
        }

        const QJsonArray parts = contentObj[QStringLiteral("parts")].toArray();
        item.content = parseParts(parts);

        // Parse functionCall parts
        for (const QJsonValue& pv : parts) {
            const QJsonObject part = pv.toObject();
            if (part.contains(QStringLiteral("functionCall"))) {
                const QJsonObject fc = part[QStringLiteral("functionCall")].toObject();
                ActionCall call;
                call.callId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                call.name = fc[QStringLiteral("name")].toString();
                call.args = QString::fromUtf8(
                    QJsonDocument(fc[QStringLiteral("args")].toObject()).toJson(QJsonDocument::Compact));
                item.toolCalls.append(call);
            }
            if (part.contains(QStringLiteral("functionResponse"))) {
                const QJsonObject fr = part[QStringLiteral("functionResponse")].toObject();
                item.toolCallId = fr[QStringLiteral("name")].toString();
                const QJsonObject response = fr[QStringLiteral("response")].toObject();
                item.content.append(Segment::fromText(
                    QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact))));
            }
        }

        req.messages.append(item);
    }

    // Generation config
    if (root.contains(QStringLiteral("generationConfig"))) {
        const QJsonObject config = root[QStringLiteral("generationConfig")].toObject();
        if (config.contains(QStringLiteral("temperature")))
            req.constraints.temperature = config[QStringLiteral("temperature")].toDouble();
        if (config.contains(QStringLiteral("topP")))
            req.constraints.topP = config[QStringLiteral("topP")].toDouble();
        if (config.contains(QStringLiteral("maxOutputTokens")))
            req.constraints.maxTokens = config[QStringLiteral("maxOutputTokens")].toInt();
        if (config.contains(QStringLiteral("stopSequences"))) {
            const QJsonArray stopArr = config[QStringLiteral("stopSequences")].toArray();
            for (const QJsonValue& sv : stopArr)
                req.constraints.stopSequences.append(sv.toString());
        }
        if (config.contains(QStringLiteral("frequencyPenalty")))
            req.constraints.frequencyPenalty = config[QStringLiteral("frequencyPenalty")].toDouble();
        if (config.contains(QStringLiteral("presencePenalty")))
            req.constraints.presencePenalty = config[QStringLiteral("presencePenalty")].toDouble();
        if (config.contains(QStringLiteral("seed")))
            req.constraints.seed = config[QStringLiteral("seed")].toInt();
    }

    // Tools
    if (root.contains(QStringLiteral("tools"))) {
        const QJsonArray toolsArr = root[QStringLiteral("tools")].toArray();
        for (const QJsonValue& tv : toolsArr) {
            const QJsonObject toolObj = tv.toObject();
            if (toolObj.contains(QStringLiteral("functionDeclarations"))) {
                const QJsonArray funcs = toolObj[QStringLiteral("functionDeclarations")].toArray();
                for (const QJsonValue& fv : funcs) {
                    const QJsonObject f = fv.toObject();
                    ActionSpec spec;
                    spec.name = f[QStringLiteral("name")].toString();
                    spec.description = f[QStringLiteral("description")].toString();
                    spec.parameters = f[QStringLiteral("parameters")].toObject();
                    req.tools.append(spec);
                }
            }
        }
    }

    // Copy incoming metadata
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
        req.metadata[it.key()] = it.value();

    return req;
}

Result<QByteArray> GeminiAdapter::encodeResponse(const SemanticResponse& response)
{
    QJsonObject root;

    QJsonArray candidates;
    for (const Candidate& cand : response.candidates) {
        QJsonObject candObj;

        QJsonObject content;
        QJsonArray parts = serializeParts(cand.output);

        // Serialize tool calls as functionCall parts
        for (const ActionCall& call : cand.toolCalls) {
            QJsonObject part;
            QJsonObject fc;
            fc[QStringLiteral("name")] = call.name;
            const QJsonDocument argsDoc = QJsonDocument::fromJson(call.args.toUtf8());
            fc[QStringLiteral("args")] = argsDoc.isObject()
                ? QJsonValue(argsDoc.object()) : QJsonValue(QJsonObject());
            part[QStringLiteral("functionCall")] = fc;
            parts.append(part);
        }

        content[QStringLiteral("parts")] = parts;
        content[QStringLiteral("role")] = QStringLiteral("model");
        candObj[QStringLiteral("content")] = content;
        candObj[QStringLiteral("finishReason")] = finishReasonFromCause(cand.stopCause);
        candObj[QStringLiteral("index")] = cand.index;
        candidates.append(candObj);
    }
    root[QStringLiteral("candidates")] = candidates;

    QJsonObject usageMetadata;
    usageMetadata[QStringLiteral("promptTokenCount")] = response.usage.promptTokens;
    usageMetadata[QStringLiteral("candidatesTokenCount")] = response.usage.completionTokens;
    usageMetadata[QStringLiteral("totalTokenCount")] = response.usage.totalTokens;
    root[QStringLiteral("usageMetadata")] = usageMetadata;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result<QByteArray> GeminiAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    QJsonObject root;

    QJsonArray candidates;
    QJsonObject candObj;
    candObj[QStringLiteral("index")] = frame.candidateIndex;

    switch (frame.type) {
    case FrameType::Started: {
        QJsonObject content;
        content[QStringLiteral("role")] = QStringLiteral("model");
        content[QStringLiteral("parts")] = QJsonArray();
        candObj[QStringLiteral("content")] = content;
        break;
    }
    case FrameType::Delta: {
        QJsonObject content;
        content[QStringLiteral("role")] = QStringLiteral("model");
        content[QStringLiteral("parts")] = serializeParts(frame.deltaSegments);
        candObj[QStringLiteral("content")] = content;
        break;
    }
    case FrameType::ActionDelta: {
        QJsonObject content;
        content[QStringLiteral("role")] = QStringLiteral("model");
        QJsonArray parts;
        QJsonObject part;
        QJsonObject fc;
        fc[QStringLiteral("name")] = frame.actionDelta.name;
        fc[QStringLiteral("args")] = frame.actionDelta.argsPatch;
        part[QStringLiteral("functionCall")] = fc;
        parts.append(part);
        content[QStringLiteral("parts")] = parts;
        candObj[QStringLiteral("content")] = content;
        break;
    }
    case FrameType::Finished: {
        candObj[QStringLiteral("finishReason")] = QStringLiteral("STOP");
        QJsonObject content;
        content[QStringLiteral("role")] = QStringLiteral("model");
        content[QStringLiteral("parts")] = QJsonArray();
        candObj[QStringLiteral("content")] = content;
        break;
    }
    case FrameType::UsageDelta: {
        QJsonObject usageMetadata;
        usageMetadata[QStringLiteral("promptTokenCount")] = frame.usageDelta.promptTokens;
        usageMetadata[QStringLiteral("candidatesTokenCount")] = frame.usageDelta.completionTokens;
        usageMetadata[QStringLiteral("totalTokenCount")] = frame.usageDelta.totalTokens;
        root[QStringLiteral("usageMetadata")] = usageMetadata;
        QJsonObject content;
        content[QStringLiteral("role")] = QStringLiteral("model");
        content[QStringLiteral("parts")] = QJsonArray();
        candObj[QStringLiteral("content")] = content;
        break;
    }
    case FrameType::Failed: {
        candObj[QStringLiteral("finishReason")] = QStringLiteral("ERROR");
        QJsonObject content;
        content[QStringLiteral("role")] = QStringLiteral("model");
        content[QStringLiteral("parts")] = QJsonArray();
        candObj[QStringLiteral("content")] = content;
        break;
    }
    }

    candidates.append(candObj);
    root[QStringLiteral("candidates")] = candidates;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result<QByteArray> GeminiAdapter::encodeFailure(const DomainFailure& failure)
{
    QJsonObject root;
    QJsonObject errorObj;

    int httpCode = failure.httpStatus();
    errorObj[QStringLiteral("code")] = httpCode;
    errorObj[QStringLiteral("message")] = failure.message;

    QString status;
    switch (failure.kind) {
    case ErrorKind::InvalidInput:  status = QStringLiteral("INVALID_ARGUMENT"); break;
    case ErrorKind::Unauthorized:  status = QStringLiteral("UNAUTHENTICATED"); break;
    case ErrorKind::Forbidden:     status = QStringLiteral("PERMISSION_DENIED"); break;
    case ErrorKind::RateLimited:   status = QStringLiteral("RESOURCE_EXHAUSTED"); break;
    case ErrorKind::Unavailable:   status = QStringLiteral("UNAVAILABLE"); break;
    case ErrorKind::Timeout:       status = QStringLiteral("DEADLINE_EXCEEDED"); break;
    case ErrorKind::NotSupported:  status = QStringLiteral("UNIMPLEMENTED"); break;
    case ErrorKind::Internal:      status = QStringLiteral("INTERNAL"); break;
    }
    errorObj[QStringLiteral("status")] = status;

    root[QStringLiteral("error")] = errorObj;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
