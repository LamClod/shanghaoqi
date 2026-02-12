#include "adapters/inbound/openai_responses.h"
#include <QUuid>
#include <QDateTime>

QString OpenAIResponsesAdapter::protocol() const
{
    return QStringLiteral("openai.responses");
}

QString OpenAIResponsesAdapter::generateResponseId()
{
    return QStringLiteral("resp_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

Result<SemanticRequest> OpenAIResponsesAdapter::decodeRequest(
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
    req.target.logicalModel = root[QStringLiteral("model")].toString();

    // Parse instructions as system message
    if (root.contains(QStringLiteral("instructions"))) {
        const QString instructions = root[QStringLiteral("instructions")].toString();
        if (!instructions.isEmpty()) {
            InteractionItem sysItem;
            sysItem.role = QStringLiteral("system");
            sysItem.content.append(Segment::fromText(instructions));
            req.messages.append(sysItem);
        }
    }

    // Parse input - can be a string or an array of items
    const QJsonValue inputVal = root[QStringLiteral("input")];
    if (inputVal.isString()) {
        InteractionItem userItem;
        userItem.role = QStringLiteral("user");
        userItem.content.append(Segment::fromText(inputVal.toString()));
        req.messages.append(userItem);
    } else if (inputVal.isArray()) {
        const QJsonArray inputArr = inputVal.toArray();
        for (const QJsonValue& iv : inputArr) {
            const QJsonObject itemObj = iv.toObject();
            InteractionItem item;
            item.role = itemObj[QStringLiteral("role")].toString();

            const QJsonValue contentVal = itemObj[QStringLiteral("content")];
            if (contentVal.isString()) {
                item.content.append(Segment::fromText(contentVal.toString()));
            } else if (contentVal.isArray()) {
                const QJsonArray contentArr = contentVal.toArray();
                for (const QJsonValue& cv : contentArr) {
                    const QJsonObject cObj = cv.toObject();
                    const QString type = cObj[QStringLiteral("type")].toString();
                    if (type == QStringLiteral("input_text") || type == QStringLiteral("text")) {
                        item.content.append(Segment::fromText(cObj[QStringLiteral("text")].toString()));
                    } else if (type == QStringLiteral("input_image") || type == QStringLiteral("image")) {
                        MediaRef ref;
                        ref.uri = cObj[QStringLiteral("image_url")].toString();
                        if (ref.uri.isEmpty())
                            ref.uri = cObj[QStringLiteral("url")].toString();
                        ref.mimeType = QStringLiteral("image/*");
                        item.content.append(Segment::fromMedia(ref));
                    }
                }
            }

            // Handle function_call_output items
            const QString type = itemObj[QStringLiteral("type")].toString();
            if (type == QStringLiteral("function_call_output")) {
                item.role = QStringLiteral("tool");
                item.toolCallId = itemObj[QStringLiteral("call_id")].toString();
                item.content.append(Segment::fromText(itemObj[QStringLiteral("output")].toString()));
            }

            req.messages.append(item);
        }
    }

    // Parse tools
    const QJsonArray tools = root[QStringLiteral("tools")].toArray();
    for (const QJsonValue& tv : tools) {
        const QJsonObject t = tv.toObject();
        const QString type = t[QStringLiteral("type")].toString();
        if (type == QStringLiteral("function")) {
            ActionSpec spec;
            spec.name = t[QStringLiteral("name")].toString();
            spec.description = t[QStringLiteral("description")].toString();
            spec.parameters = t[QStringLiteral("parameters")].toObject();
            req.tools.append(spec);
        }
    }

    // Parse constraints
    if (root.contains(QStringLiteral("temperature")))
        req.constraints.temperature = root[QStringLiteral("temperature")].toDouble();
    if (root.contains(QStringLiteral("max_output_tokens")))
        req.constraints.maxTokens = root[QStringLiteral("max_output_tokens")].toInt();
    if (root.contains(QStringLiteral("top_p")))
        req.constraints.topP = root[QStringLiteral("top_p")].toDouble();

    // Stream flag
    if (root[QStringLiteral("stream")].toBool())
        req.metadata[QStringLiteral("stream")] = QStringLiteral("true");

    // Copy incoming metadata
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
        req.metadata[it.key()] = it.value();

    return req;
}

QJsonObject OpenAIResponsesAdapter::buildOutputItem(const Candidate& candidate)
{
    QJsonObject item;
    item[QStringLiteral("type")] = QStringLiteral("message");
    item[QStringLiteral("role")] = candidate.role.isEmpty()
        ? QStringLiteral("assistant") : candidate.role;

    QJsonArray contentArr;
    for (const Segment& seg : candidate.output) {
        if (seg.kind == SegmentKind::Text) {
            QJsonObject textBlock;
            textBlock[QStringLiteral("type")] = QStringLiteral("output_text");
            textBlock[QStringLiteral("text")] = seg.text;
            contentArr.append(textBlock);
        }
    }

    // Tool calls become function_call output items
    for (const ActionCall& call : candidate.toolCalls) {
        QJsonObject funcCallItem;
        funcCallItem[QStringLiteral("type")] = QStringLiteral("function_call");
        funcCallItem[QStringLiteral("call_id")] = call.callId;
        funcCallItem[QStringLiteral("name")] = call.name;
        funcCallItem[QStringLiteral("arguments")] = call.args;
        contentArr.append(funcCallItem);
    }

    item[QStringLiteral("content")] = contentArr;
    return item;
}

Result<QByteArray> OpenAIResponsesAdapter::encodeResponse(const SemanticResponse& response)
{
    QJsonObject root;
    root[QStringLiteral("id")] = response.responseId.isEmpty()
        ? generateResponseId() : response.responseId;
    root[QStringLiteral("object")] = QStringLiteral("response");
    root[QStringLiteral("model")] = response.modelUsed;
    root[QStringLiteral("created_at")] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch());

    QJsonArray output;
    for (const Candidate& cand : response.candidates) {
        output.append(buildOutputItem(cand));
    }
    root[QStringLiteral("output")] = output;

    root[QStringLiteral("status")] = QStringLiteral("completed");

    QJsonObject usage;
    usage[QStringLiteral("input_tokens")] = response.usage.promptTokens;
    usage[QStringLiteral("output_tokens")] = response.usage.completionTokens;
    usage[QStringLiteral("total_tokens")] = response.usage.totalTokens;
    root[QStringLiteral("usage")] = usage;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result<QByteArray> OpenAIResponsesAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    QByteArray result;

    switch (frame.type) {
    case FrameType::Started: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("response.created");
        QJsonObject responseObj;
        responseObj[QStringLiteral("id")] = generateResponseId();
        responseObj[QStringLiteral("object")] = QStringLiteral("response");
        responseObj[QStringLiteral("status")] = QStringLiteral("in_progress");
        event[QStringLiteral("response")] = responseObj;
        result = QByteArray("event: response.created\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::Delta: {
        QString text;
        for (const Segment& seg : frame.deltaSegments) {
            if (seg.kind == SegmentKind::Text)
                text += seg.text;
        }
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("response.output_item.added");
        QJsonObject delta;
        delta[QStringLiteral("type")] = QStringLiteral("output_text");
        delta[QStringLiteral("text")] = text;
        event[QStringLiteral("delta")] = delta;
        result = QByteArray("event: response.content_part.delta\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::ActionDelta: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("response.function_call_arguments.delta");
        QJsonObject delta;
        delta[QStringLiteral("call_id")] = frame.actionDelta.callId;
        delta[QStringLiteral("name")] = frame.actionDelta.name;
        delta[QStringLiteral("arguments")] = frame.actionDelta.argsPatch;
        event[QStringLiteral("delta")] = delta;
        result = QByteArray("event: response.function_call_arguments.delta\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::Finished: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("response.completed");
        QJsonObject responseObj;
        responseObj[QStringLiteral("status")] = QStringLiteral("completed");
        event[QStringLiteral("response")] = responseObj;
        result = QByteArray("event: response.completed\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::UsageDelta: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("response.usage");
        QJsonObject usage;
        usage[QStringLiteral("input_tokens")] = frame.usageDelta.promptTokens;
        usage[QStringLiteral("output_tokens")] = frame.usageDelta.completionTokens;
        usage[QStringLiteral("total_tokens")] = frame.usageDelta.totalTokens;
        event[QStringLiteral("usage")] = usage;
        result = QByteArray("event: response.usage\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::Failed: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("response.failed");
        QJsonObject errorObj;
        errorObj[QStringLiteral("message")] = frame.failure.message;
        errorObj[QStringLiteral("code")] = frame.failure.code;
        event[QStringLiteral("error")] = errorObj;
        result = QByteArray("event: response.failed\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    }

    return result;
}

Result<QByteArray> OpenAIResponsesAdapter::encodeFailure(const DomainFailure& failure)
{
    QJsonObject root;
    QJsonObject errorObj;
    errorObj[QStringLiteral("message")] = failure.message;
    errorObj[QStringLiteral("code")] = failure.code;
    root[QStringLiteral("error")] = errorObj;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
