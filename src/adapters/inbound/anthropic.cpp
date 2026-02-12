#include "adapters/inbound/anthropic.h"
#include <QUuid>
#include <QDateTime>

QString AnthropicAdapter::protocol() const
{
    return QStringLiteral("anthropic");
}

QString AnthropicAdapter::generateMessageId()
{
    return QStringLiteral("msg_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString AnthropicAdapter::stopReasonFromCause(StopCause cause)
{
    switch (cause) {
    case StopCause::Completed:     return QStringLiteral("end_turn");
    case StopCause::Length:        return QStringLiteral("max_tokens");
    case StopCause::ContentFilter: return QStringLiteral("content_filter");
    case StopCause::ToolCall:      return QStringLiteral("tool_use");
    }
    return QStringLiteral("end_turn");
}

QList<Segment> AnthropicAdapter::parseContentBlocks(const QJsonValue& content)
{
    QList<Segment> segments;
    if (content.isString()) {
        segments.append(Segment::fromText(content.toString()));
    } else if (content.isArray()) {
        const QJsonArray blocks = content.toArray();
        for (const QJsonValue& bv : blocks) {
            const QJsonObject block = bv.toObject();
            const QString type = block[QStringLiteral("type")].toString();
            if (type == QStringLiteral("text")) {
                segments.append(Segment::fromText(block[QStringLiteral("text")].toString()));
            } else if (type == QStringLiteral("image")) {
                const QJsonObject source = block[QStringLiteral("source")].toObject();
                MediaRef ref;
                ref.mimeType = source[QStringLiteral("media_type")].toString();
                const QString sourceType = source[QStringLiteral("type")].toString();
                if (sourceType == QStringLiteral("base64")) {
                    ref.inlineData = QByteArray::fromBase64(
                        source[QStringLiteral("data")].toString().toUtf8());
                } else if (sourceType == QStringLiteral("url")) {
                    ref.uri = source[QStringLiteral("url")].toString();
                }
                segments.append(Segment::fromMedia(ref));
            }
        }
    }
    return segments;
}

QList<ActionCall> AnthropicAdapter::parseToolUseBlocks(const QJsonArray& blocks)
{
    QList<ActionCall> calls;
    for (const QJsonValue& bv : blocks) {
        const QJsonObject block = bv.toObject();
        if (block[QStringLiteral("type")].toString() == QStringLiteral("tool_use")) {
            ActionCall call;
            call.callId = block[QStringLiteral("id")].toString();
            call.name = block[QStringLiteral("name")].toString();
            call.args = QString::fromUtf8(
                QJsonDocument(block[QStringLiteral("input")].toObject()).toJson(QJsonDocument::Compact));
            calls.append(call);
        }
    }
    return calls;
}

QJsonArray AnthropicAdapter::serializeContentBlocks(const QList<Segment>& segments)
{
    QJsonArray blocks;
    for (const Segment& seg : segments) {
        if (seg.kind == SegmentKind::Text) {
            QJsonObject block;
            block[QStringLiteral("type")] = QStringLiteral("text");
            block[QStringLiteral("text")] = seg.text;
            blocks.append(block);
        } else if (seg.kind == SegmentKind::Media) {
            QJsonObject block;
            block[QStringLiteral("type")] = QStringLiteral("image");
            QJsonObject source;
            if (!seg.media.inlineData.isEmpty()) {
                source[QStringLiteral("type")] = QStringLiteral("base64");
                source[QStringLiteral("media_type")] = seg.media.mimeType;
                source[QStringLiteral("data")] = QString::fromUtf8(seg.media.inlineData.toBase64());
            } else {
                source[QStringLiteral("type")] = QStringLiteral("url");
                source[QStringLiteral("url")] = seg.media.uri;
            }
            block[QStringLiteral("source")] = source;
            blocks.append(block);
        }
    }
    return blocks;
}

QJsonArray AnthropicAdapter::serializeToolUseBlocks(const QList<ActionCall>& calls)
{
    QJsonArray blocks;
    for (const ActionCall& call : calls) {
        QJsonObject block;
        block[QStringLiteral("type")] = QStringLiteral("tool_use");
        block[QStringLiteral("id")] = call.callId;
        block[QStringLiteral("name")] = call.name;
        const QJsonDocument argsDoc = QJsonDocument::fromJson(call.args.toUtf8());
        block[QStringLiteral("input")] = argsDoc.isObject()
            ? QJsonValue(argsDoc.object()) : QJsonValue(call.args);
        blocks.append(block);
    }
    return blocks;
}

Result<SemanticRequest> AnthropicAdapter::decodeRequest(
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

    // System message is top-level in Anthropic format
    if (root.contains(QStringLiteral("system"))) {
        const QJsonValue sysVal = root[QStringLiteral("system")];
        InteractionItem sysItem;
        sysItem.role = QStringLiteral("system");
        if (sysVal.isString()) {
            sysItem.content.append(Segment::fromText(sysVal.toString()));
        } else if (sysVal.isArray()) {
            const QJsonArray sysBlocks = sysVal.toArray();
            for (const QJsonValue& bv : sysBlocks) {
                const QJsonObject block = bv.toObject();
                if (block[QStringLiteral("type")].toString() == QStringLiteral("text")) {
                    sysItem.content.append(Segment::fromText(block[QStringLiteral("text")].toString()));
                }
            }
        }
        req.messages.append(sysItem);
    }

    // Parse messages
    const QJsonArray msgs = root[QStringLiteral("messages")].toArray();
    for (const QJsonValue& mv : msgs) {
        const QJsonObject m = mv.toObject();
        InteractionItem item;
        item.role = m[QStringLiteral("role")].toString();

        const QJsonValue contentVal = m[QStringLiteral("content")];
        if (contentVal.isString()) {
            item.content.append(Segment::fromText(contentVal.toString()));
        } else if (contentVal.isArray()) {
            const QJsonArray contentBlocks = contentVal.toArray();
            // Parse text and image blocks
            item.content = parseContentBlocks(contentVal);

            // Parse tool_use blocks into toolCalls
            item.toolCalls = parseToolUseBlocks(contentBlocks);

            // Parse tool_result blocks
            for (const QJsonValue& bv : contentBlocks) {
                const QJsonObject block = bv.toObject();
                if (block[QStringLiteral("type")].toString() == QStringLiteral("tool_result")) {
                    item.toolCallId = block[QStringLiteral("tool_use_id")].toString();
                    const QJsonValue resultContent = block[QStringLiteral("content")];
                    if (resultContent.isString()) {
                        item.content.append(Segment::fromText(resultContent.toString()));
                    } else if (resultContent.isArray()) {
                        for (const QJsonValue& rc : resultContent.toArray()) {
                            const QJsonObject rcObj = rc.toObject();
                            if (rcObj[QStringLiteral("type")].toString() == QStringLiteral("text")) {
                                item.content.append(Segment::fromText(rcObj[QStringLiteral("text")].toString()));
                            }
                        }
                    }
                }
            }
        }

        req.messages.append(item);
    }

    // Constraints
    if (root.contains(QStringLiteral("max_tokens")))
        req.constraints.maxTokens = root[QStringLiteral("max_tokens")].toInt();
    if (root.contains(QStringLiteral("temperature")))
        req.constraints.temperature = root[QStringLiteral("temperature")].toDouble();
    if (root.contains(QStringLiteral("top_p")))
        req.constraints.topP = root[QStringLiteral("top_p")].toDouble();
    if (root.contains(QStringLiteral("stop_sequences"))) {
        const QJsonArray stopArr = root[QStringLiteral("stop_sequences")].toArray();
        for (const QJsonValue& sv : stopArr)
            req.constraints.stopSequences.append(sv.toString());
    }

    // Tools
    const QJsonArray tools = root[QStringLiteral("tools")].toArray();
    for (const QJsonValue& tv : tools) {
        const QJsonObject t = tv.toObject();
        ActionSpec spec;
        spec.name = t[QStringLiteral("name")].toString();
        spec.description = t[QStringLiteral("description")].toString();
        spec.parameters = t[QStringLiteral("input_schema")].toObject();
        req.tools.append(spec);
    }

    // Stream flag
    if (root[QStringLiteral("stream")].toBool())
        req.metadata[QStringLiteral("stream")] = QStringLiteral("true");

    // Copy incoming metadata
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
        req.metadata[it.key()] = it.value();

    return req;
}

Result<QByteArray> AnthropicAdapter::encodeResponse(const SemanticResponse& response)
{
    QJsonObject root;
    root[QStringLiteral("id")] = response.responseId.isEmpty()
        ? generateMessageId() : response.responseId;
    root[QStringLiteral("type")] = QStringLiteral("message");
    root[QStringLiteral("role")] = QStringLiteral("assistant");
    root[QStringLiteral("model")] = response.modelUsed;

    // Build content blocks from first candidate
    QJsonArray contentBlocks;
    if (!response.candidates.isEmpty()) {
        const Candidate& cand = response.candidates.first();
        contentBlocks = serializeContentBlocks(cand.output);

        // Append tool_use blocks
        const QJsonArray toolBlocks = serializeToolUseBlocks(cand.toolCalls);
        for (const QJsonValue& tb : toolBlocks)
            contentBlocks.append(tb);

        root[QStringLiteral("stop_reason")] = stopReasonFromCause(cand.stopCause);
    } else {
        root[QStringLiteral("stop_reason")] = QStringLiteral("end_turn");
    }
    root[QStringLiteral("content")] = contentBlocks;

    QJsonObject usage;
    usage[QStringLiteral("input_tokens")] = response.usage.promptTokens;
    usage[QStringLiteral("output_tokens")] = response.usage.completionTokens;
    root[QStringLiteral("usage")] = usage;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result<QByteArray> AnthropicAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    QByteArray result;

    switch (frame.type) {
    case FrameType::Started: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("message_start");
        QJsonObject message;
        message[QStringLiteral("id")] = generateMessageId();
        message[QStringLiteral("type")] = QStringLiteral("message");
        message[QStringLiteral("role")] = QStringLiteral("assistant");
        message[QStringLiteral("content")] = QJsonArray();
        message[QStringLiteral("stop_reason")] = QJsonValue::Null;
        QJsonObject usage;
        usage[QStringLiteral("input_tokens")] = frame.usageDelta.promptTokens;
        usage[QStringLiteral("output_tokens")] = 0;
        message[QStringLiteral("usage")] = usage;
        event[QStringLiteral("message")] = message;
        result = QByteArray("event: message_start\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");

        // Also emit content_block_start
        QJsonObject blockStart;
        blockStart[QStringLiteral("type")] = QStringLiteral("content_block_start");
        blockStart[QStringLiteral("index")] = 0;
        QJsonObject contentBlock;
        contentBlock[QStringLiteral("type")] = QStringLiteral("text");
        contentBlock[QStringLiteral("text")] = QStringLiteral("");
        blockStart[QStringLiteral("content_block")] = contentBlock;
        result += QByteArray("event: content_block_start\ndata: ")
            + QJsonDocument(blockStart).toJson(QJsonDocument::Compact)
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
        event[QStringLiteral("type")] = QStringLiteral("content_block_delta");
        event[QStringLiteral("index")] = frame.candidateIndex;
        QJsonObject delta;
        delta[QStringLiteral("type")] = QStringLiteral("text_delta");
        delta[QStringLiteral("text")] = text;
        event[QStringLiteral("delta")] = delta;
        result = QByteArray("event: content_block_delta\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::ActionDelta: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("content_block_delta");
        event[QStringLiteral("index")] = frame.candidateIndex;
        QJsonObject delta;
        delta[QStringLiteral("type")] = QStringLiteral("input_json_delta");
        delta[QStringLiteral("partial_json")] = frame.actionDelta.argsPatch;
        event[QStringLiteral("delta")] = delta;
        result = QByteArray("event: content_block_delta\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::Finished: {
        // content_block_stop
        QJsonObject blockStop;
        blockStop[QStringLiteral("type")] = QStringLiteral("content_block_stop");
        blockStop[QStringLiteral("index")] = frame.candidateIndex;
        result = QByteArray("event: content_block_stop\ndata: ")
            + QJsonDocument(blockStop).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");

        // message_delta
        QJsonObject msgDelta;
        msgDelta[QStringLiteral("type")] = QStringLiteral("message_delta");
        QJsonObject delta;
        delta[QStringLiteral("stop_reason")] = QStringLiteral("end_turn");
        msgDelta[QStringLiteral("delta")] = delta;
        QJsonObject usage;
        usage[QStringLiteral("output_tokens")] = frame.usageDelta.completionTokens;
        msgDelta[QStringLiteral("usage")] = usage;
        result += QByteArray("event: message_delta\ndata: ")
            + QJsonDocument(msgDelta).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");

        // message_stop
        QJsonObject msgStop;
        msgStop[QStringLiteral("type")] = QStringLiteral("message_stop");
        result += QByteArray("event: message_stop\ndata: ")
            + QJsonDocument(msgStop).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::UsageDelta: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("message_delta");
        QJsonObject delta;
        delta[QStringLiteral("stop_reason")] = QJsonValue::Null;
        event[QStringLiteral("delta")] = delta;
        QJsonObject usage;
        usage[QStringLiteral("output_tokens")] = frame.usageDelta.completionTokens;
        event[QStringLiteral("usage")] = usage;
        result = QByteArray("event: message_delta\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    case FrameType::Failed: {
        QJsonObject event;
        event[QStringLiteral("type")] = QStringLiteral("error");
        QJsonObject errorObj;
        errorObj[QStringLiteral("type")] = QStringLiteral("server_error");
        errorObj[QStringLiteral("message")] = frame.failure.message;
        event[QStringLiteral("error")] = errorObj;
        result = QByteArray("event: error\ndata: ")
            + QJsonDocument(event).toJson(QJsonDocument::Compact)
            + QByteArray("\n\n");
        break;
    }
    }

    return result;
}

Result<QByteArray> AnthropicAdapter::encodeFailure(const DomainFailure& failure)
{
    QJsonObject root;
    root[QStringLiteral("type")] = QStringLiteral("error");
    QJsonObject errorObj;
    errorObj[QStringLiteral("type")] = failure.code.isEmpty()
        ? QStringLiteral("api_error") : failure.code;
    errorObj[QStringLiteral("message")] = failure.message;
    root[QStringLiteral("error")] = errorObj;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
