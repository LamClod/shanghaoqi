#include "anthropic.h"

QString AnthropicOutbound::adapterId() const
{
    return QStringLiteral("anthropic");
}

Result<ProviderRequest> AnthropicOutbound::buildRequest(const SemanticRequest& request)
{
    ProviderRequest pr;
    pr.method = QStringLiteral("POST");

    QString baseUrl = request.metadata.value(QStringLiteral("provider_base_url"),
                                              QStringLiteral("https://api.anthropic.com"));
    QString middleRoute = request.metadata.value(QStringLiteral("middle_route"),
                                                  QStringLiteral("/v1"));
    if (!middleRoute.isEmpty() && baseUrl.endsWith(middleRoute))
        middleRoute.clear();
    pr.url = baseUrl + middleRoute + QStringLiteral("/messages");

    QString apiKey = request.metadata.value(QStringLiteral("api_key"));
    if (apiKey.isEmpty()) {
        apiKey = request.metadata.value(QStringLiteral("provider_api_key"));
    }
    pr.headers[QStringLiteral("x-api-key")] = apiKey;
    pr.headers[QStringLiteral("anthropic-version")] = QStringLiteral("2023-06-01");
    pr.headers[QStringLiteral("Content-Type")] = QStringLiteral("application/json");

    for (auto it = request.metadata.constBegin(); it != request.metadata.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("custom_header."))) {
            const QString headerName = it.key().mid(14);
            if (!headerName.isEmpty()) {
                pr.headers[headerName] = it.value();
            }
        }
    }

    QJsonObject body;
    body[QStringLiteral("model")] = request.target.logicalModel;

    QString systemPrompt;
    QJsonArray messages = buildMessages(request.messages, systemPrompt);
    body[QStringLiteral("messages")] = messages;

    if (!systemPrompt.isEmpty()) {
        body[QStringLiteral("system")] = systemPrompt;
    }

    if (!request.tools.isEmpty()) {
        body[QStringLiteral("tools")] = buildToolDefs(request.tools);
    }

    // max_tokens is required for Anthropic
    int maxTokens = request.constraints.maxTokens.value_or(
                        request.constraints.maxCompletionTokens.value_or(4096));
    body[QStringLiteral("max_tokens")] = maxTokens;

    if (request.constraints.temperature.has_value()) {
        body[QStringLiteral("temperature")] = request.constraints.temperature.value();
    }
    if (request.constraints.topP.has_value()) {
        body[QStringLiteral("top_p")] = request.constraints.topP.value();
    }
    if (!request.constraints.stopSequences.isEmpty()) {
        QJsonArray stopArr;
        for (const auto& s : request.constraints.stopSequences) {
            stopArr.append(s);
        }
        body[QStringLiteral("stop_sequences")] = stopArr;
    }

    const bool stream =
        request.metadata.value(QStringLiteral("stream.upstream")) == QStringLiteral("true") ||
        request.metadata.value(QStringLiteral("stream")) == QStringLiteral("true");
    if (stream) {
        body[QStringLiteral("stream")] = true;
    }
    pr.stream = stream;

    pr.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return pr;
}

Result<SemanticResponse> AnthropicOutbound::parseResponse(const ProviderResponse& response)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(response.body, &err);
    if (err.error != QJsonParseError::NoError) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Failed to parse Anthropic response JSON: ") + err.errorString()));
    }

    QJsonObject root = doc.object();
    SemanticResponse sr;
    sr.responseId = root.value(QStringLiteral("id")).toString();
    sr.modelUsed = root.value(QStringLiteral("model")).toString();
    sr.kind = TaskKind::Conversation;

    sr.candidates.append(parseCandidate(root));

    QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
    sr.usage.promptTokens = usage.value(QStringLiteral("input_tokens")).toInt();
    sr.usage.completionTokens = usage.value(QStringLiteral("output_tokens")).toInt();
    sr.usage.totalTokens = sr.usage.promptTokens + sr.usage.completionTokens;

    return sr;
}

Result<StreamFrame> AnthropicOutbound::parseChunk(const ProviderChunk& chunk)
{
    // Anthropic streams event-typed chunks. The chunk.type contains the event type.
    // The chunk.data contains the JSON payload.
    QString eventType = chunk.type;

    // Strip adapter prefix if present (e.g., "adapter:anthropic|event:message_start")
    for (const QString& part : eventType.split(QLatin1Char('|'))) {
        if (part.startsWith(QStringLiteral("event:"))) {
            eventType = part.mid(6);
            break;
        }
    }

    QString dataStr = QString::fromUtf8(chunk.data).trimmed();
    if (dataStr.isEmpty()) {
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Failed to parse Anthropic chunk JSON: ") + err.errorString()));
    }

    QJsonObject root = doc.object();
    QString type = root.value(QStringLiteral("type")).toString();
    if (!type.isEmpty()) {
        eventType = type;
    }

    if (eventType == QStringLiteral("message_start")) {
        StreamFrame frame;
        frame.type = FrameType::Started;
        QJsonObject message = root.value(QStringLiteral("message")).toObject();
        QJsonObject usage = message.value(QStringLiteral("usage")).toObject();
        frame.usageDelta.promptTokens = usage.value(QStringLiteral("input_tokens")).toInt();
        return frame;
    }

    if (eventType == QStringLiteral("content_block_start")) {
        QJsonObject contentBlock = root.value(QStringLiteral("content_block")).toObject();
        QString blockType = contentBlock.value(QStringLiteral("type")).toString();
        if (blockType == QStringLiteral("tool_use")) {
            StreamFrame frame;
            frame.type = FrameType::ActionDelta;
            frame.actionDelta.callId = contentBlock.value(QStringLiteral("id")).toString();
            frame.actionDelta.name = contentBlock.value(QStringLiteral("name")).toString();
            return frame;
        }
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    if (eventType == QStringLiteral("content_block_delta")) {
        QJsonObject delta = root.value(QStringLiteral("delta")).toObject();
        QString deltaType = delta.value(QStringLiteral("type")).toString();

        if (deltaType == QStringLiteral("text_delta")) {
            StreamFrame frame;
            frame.type = FrameType::Delta;
            QString text = delta.value(QStringLiteral("text")).toString();
            frame.deltaSegments.append(Segment::fromText(text));
            return frame;
        }

        if (deltaType == QStringLiteral("input_json_delta")) {
            StreamFrame frame;
            frame.type = FrameType::ActionDelta;
            frame.actionDelta.argsPatch = delta.value(QStringLiteral("partial_json")).toString();
            return frame;
        }

        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    if (eventType == QStringLiteral("content_block_stop")) {
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    if (eventType == QStringLiteral("message_delta")) {
        StreamFrame frame;
        frame.type = FrameType::UsageDelta;
        QJsonObject delta = root.value(QStringLiteral("delta")).toObject();
        QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
        frame.usageDelta.completionTokens = usage.value(QStringLiteral("output_tokens")).toInt();

        QString stopReason = delta.value(QStringLiteral("stop_reason")).toString();
        if (!stopReason.isEmpty()) {
            frame.type = FrameType::UsageDelta;
        }
        return frame;
    }

    if (eventType == QStringLiteral("message_stop")) {
        StreamFrame frame;
        frame.type = FrameType::Finished;
        frame.isFinal = true;
        return frame;
    }

    if (eventType == QStringLiteral("ping")) {
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    if (eventType == QStringLiteral("error")) {
        QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
        StreamFrame frame;
        frame.type = FrameType::Failed;
        frame.failure.kind = ErrorKind::Internal;
        frame.failure.message = errorObj.value(QStringLiteral("message")).toString();
        frame.failure.code = errorObj.value(QStringLiteral("type")).toString();
        frame.isFinal = true;
        return frame;
    }

    // Unknown event type: treat as no-op delta
    StreamFrame frame;
    frame.type = FrameType::Delta;
    return frame;
}

DomainFailure AnthropicOutbound::mapFailure(int httpStatus, const QByteArray& body)
{
    QString message;
    QString code;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error == QJsonParseError::NoError) {
        QJsonObject root = doc.object();
        // Anthropic error format: {"type":"error","error":{"type":"...","message":"..."}}
        QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
        message = errorObj.value(QStringLiteral("message")).toString();
        code = errorObj.value(QStringLiteral("type")).toString();
    }

    if (message.isEmpty()) {
        message = QStringLiteral("Anthropic API error (HTTP %1)").arg(httpStatus);
    }
    if (code.isEmpty()) {
        code = QStringLiteral("anthropic.http_%1").arg(httpStatus);
    }

    ErrorKind kind = mapHttpStatusToKind(httpStatus);
    DomainFailure failure;
    failure.kind = kind;
    failure.code = code;
    failure.message = message;
    failure.retryable = (kind == ErrorKind::RateLimited ||
                         kind == ErrorKind::Unavailable ||
                         kind == ErrorKind::Timeout);
    failure.temporary = failure.retryable;
    return failure;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QJsonArray AnthropicOutbound::buildMessages(const QList<InteractionItem>& items, QString& systemOut) const
{
    QJsonArray messages;
    systemOut.clear();

    for (const auto& item : items) {
        // Extract system messages separately
        if (item.role == QStringLiteral("system")) {
            for (const auto& seg : item.content) {
                if (seg.kind == SegmentKind::Text) {
                    if (!systemOut.isEmpty()) {
                        systemOut += QStringLiteral("\n");
                    }
                    systemOut += seg.text;
                }
            }
            continue;
        }

        QJsonObject msg;

        // Map role: "tool" -> "user" with tool_result content block
        if (item.role == QStringLiteral("tool")) {
            msg[QStringLiteral("role")] = QStringLiteral("user");
            QJsonArray contentArr;
            QJsonObject toolResult;
            toolResult[QStringLiteral("type")] = QStringLiteral("tool_result");
            toolResult[QStringLiteral("tool_use_id")] = item.toolCallId;
            QString textContent;
            for (const auto& seg : item.content) {
                if (seg.kind == SegmentKind::Text) {
                    textContent += seg.text;
                }
            }
            toolResult[QStringLiteral("content")] = textContent;
            contentArr.append(toolResult);
            msg[QStringLiteral("content")] = contentArr;
        } else {
            msg[QStringLiteral("role")] = item.role;

            QJsonArray contentArr = segmentsToContentBlocks(item.content);

            // Add tool_use blocks for tool calls (assistant messages)
            for (const auto& tc : item.toolCalls) {
                QJsonObject toolUse;
                toolUse[QStringLiteral("type")] = QStringLiteral("tool_use");
                toolUse[QStringLiteral("id")] = tc.callId;
                toolUse[QStringLiteral("name")] = tc.name;
                QJsonDocument argsDoc = QJsonDocument::fromJson(tc.args.toUtf8());
                if (argsDoc.isObject()) {
                    toolUse[QStringLiteral("input")] = argsDoc.object();
                } else {
                    toolUse[QStringLiteral("input")] = QJsonObject();
                }
                contentArr.append(toolUse);
            }

            msg[QStringLiteral("content")] = contentArr;
        }

        messages.append(msg);
    }

    return messages;
}

QJsonArray AnthropicOutbound::buildToolDefs(const QList<ActionSpec>& tools) const
{
    QJsonArray arr;
    for (const auto& tool : tools) {
        QJsonObject toolObj;
        toolObj[QStringLiteral("name")] = tool.name;
        toolObj[QStringLiteral("description")] = tool.description;
        toolObj[QStringLiteral("input_schema")] = tool.parameters;
        arr.append(toolObj);
    }
    return arr;
}

QJsonArray AnthropicOutbound::segmentsToContentBlocks(const QList<Segment>& segments) const
{
    QJsonArray arr;
    for (const auto& seg : segments) {
        if (seg.kind == SegmentKind::Text) {
            QJsonObject block;
            block[QStringLiteral("type")] = QStringLiteral("text");
            block[QStringLiteral("text")] = seg.text;
            arr.append(block);
        } else if (seg.kind == SegmentKind::Media) {
            QJsonObject block;
            block[QStringLiteral("type")] = QStringLiteral("image");
            QJsonObject source;
            if (!seg.media.inlineData.isEmpty()) {
                source[QStringLiteral("type")] = QStringLiteral("base64");
                source[QStringLiteral("media_type")] = seg.media.mimeType;
                source[QStringLiteral("data")] = QString::fromLatin1(seg.media.inlineData.toBase64());
            } else if (!seg.media.uri.isEmpty()) {
                source[QStringLiteral("type")] = QStringLiteral("url");
                source[QStringLiteral("url")] = seg.media.uri;
            }
            block[QStringLiteral("source")] = source;
            arr.append(block);
        } else if (seg.kind == SegmentKind::Structured) {
            QJsonObject block;
            block[QStringLiteral("type")] = QStringLiteral("text");
            block[QStringLiteral("text")] = QString::fromUtf8(
                QJsonDocument(seg.structured).toJson(QJsonDocument::Compact));
            arr.append(block);
        }
    }
    return arr;
}

Candidate AnthropicOutbound::parseCandidate(const QJsonObject& root) const
{
    Candidate c;
    c.index = 0;
    c.role = root.value(QStringLiteral("role")).toString(QStringLiteral("assistant"));

    QJsonArray content = root.value(QStringLiteral("content")).toArray();
    for (const QJsonValue& blockVal : content) {
        QJsonObject block = blockVal.toObject();
        QString type = block.value(QStringLiteral("type")).toString();

        if (type == QStringLiteral("text")) {
            c.output.append(Segment::fromText(block.value(QStringLiteral("text")).toString()));
        } else if (type == QStringLiteral("tool_use")) {
            c.toolCalls.append(parseToolUseBlock(block));
        }
    }

    QString stopReason = root.value(QStringLiteral("stop_reason")).toString();
    if (stopReason == QStringLiteral("end_turn")) {
        c.stopCause = StopCause::Completed;
    } else if (stopReason == QStringLiteral("max_tokens")) {
        c.stopCause = StopCause::Length;
    } else if (stopReason == QStringLiteral("tool_use")) {
        c.stopCause = StopCause::ToolCall;
    } else if (stopReason == QStringLiteral("stop_sequence")) {
        c.stopCause = StopCause::Completed;
    } else {
        c.stopCause = StopCause::Completed;
    }

    return c;
}

ActionCall AnthropicOutbound::parseToolUseBlock(const QJsonObject& block) const
{
    ActionCall call;
    call.callId = block.value(QStringLiteral("id")).toString();
    call.name = block.value(QStringLiteral("name")).toString();
    QJsonObject input = block.value(QStringLiteral("input")).toObject();
    call.args = QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact));
    return call;
}

ErrorKind AnthropicOutbound::mapHttpStatusToKind(int httpStatus) const
{
    switch (httpStatus) {
    case 400: return ErrorKind::InvalidInput;
    case 401: return ErrorKind::Unauthorized;
    case 403: return ErrorKind::Forbidden;
    case 404: return ErrorKind::InvalidInput;
    case 429: return ErrorKind::RateLimited;
    case 500: return ErrorKind::Internal;
    case 529: return ErrorKind::Unavailable;  // Anthropic overloaded
    case 503: return ErrorKind::Unavailable;
    case 504: return ErrorKind::Timeout;
    default:
        if (httpStatus >= 500) return ErrorKind::Internal;
        if (httpStatus >= 400) return ErrorKind::InvalidInput;
        return ErrorKind::Internal;
    }
}
