#include "openai.h"

QString OpenAIOutbound::adapterId() const
{
    return QStringLiteral("openai");
}

Result<ProviderRequest> OpenAIOutbound::buildRequest(const SemanticRequest& request)
{
    ProviderRequest pr;
    pr.method = QStringLiteral("POST");

    QString baseUrl = request.metadata.value(QStringLiteral("provider_base_url"),
                                              QStringLiteral("https://api.openai.com"));
    QString middleRoute = request.metadata.value(QStringLiteral("middle_route"),
                                                  QStringLiteral("/v1"));
    // Guard against double-append: if baseUrl already ends with middleRoute, skip
    if (!middleRoute.isEmpty() && baseUrl.endsWith(middleRoute))
        middleRoute.clear();
    pr.url = baseUrl + middleRoute + QStringLiteral("/chat/completions");

    QString apiKey = request.metadata.value(QStringLiteral("api_key"));
    if (apiKey.isEmpty()) {
        apiKey = request.metadata.value(QStringLiteral("provider_api_key"));
    }
    pr.headers[QStringLiteral("Authorization")] = QStringLiteral("Bearer ") + apiKey;
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
    body[QStringLiteral("messages")] = buildMessages(request.messages);

    if (!request.tools.isEmpty()) {
        body[QStringLiteral("tools")] = buildToolDefs(request.tools);
    }

    const bool stream =
        request.metadata.value(QStringLiteral("stream.upstream")) == QStringLiteral("true") ||
        request.metadata.value(QStringLiteral("stream")) == QStringLiteral("true");
    if (stream) {
        body[QStringLiteral("stream")] = true;
    }
    pr.stream = stream;

    buildConstraints(body, request.constraints);

    pr.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return pr;
}

Result<SemanticResponse> OpenAIOutbound::parseResponse(const ProviderResponse& response)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(response.body, &err);
    if (err.error != QJsonParseError::NoError) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Failed to parse OpenAI response JSON: ") + err.errorString()));
    }

    QJsonObject root = doc.object();
    SemanticResponse sr;
    sr.responseId = root.value(QStringLiteral("id")).toString();
    sr.modelUsed = root.value(QStringLiteral("model")).toString();
    sr.kind = TaskKind::Conversation;

    QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    for (const QJsonValue& cv : choices) {
        sr.candidates.append(parseChoice(cv.toObject()));
    }

    QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
    sr.usage.promptTokens = usage.value(QStringLiteral("prompt_tokens")).toInt();
    sr.usage.completionTokens = usage.value(QStringLiteral("completion_tokens")).toInt();
    sr.usage.totalTokens = usage.value(QStringLiteral("total_tokens")).toInt();

    return sr;
}

Result<StreamFrame> OpenAIOutbound::parseChunk(const ProviderChunk& chunk)
{
    QString dataStr = QString::fromUtf8(chunk.data).trimmed();

    if (dataStr == QStringLiteral("[DONE]")) {
        StreamFrame frame;
        frame.type = FrameType::Finished;
        frame.isFinal = true;
        return frame;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(dataStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Failed to parse OpenAI chunk JSON: ") + err.errorString()));
    }

    QJsonObject root = doc.object();
    QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
        if (!usage.isEmpty()) {
            StreamFrame frame;
            frame.type = FrameType::UsageDelta;
            frame.usageDelta.promptTokens = usage.value(QStringLiteral("prompt_tokens")).toInt();
            frame.usageDelta.completionTokens = usage.value(QStringLiteral("completion_tokens")).toInt();
            frame.usageDelta.totalTokens = usage.value(QStringLiteral("total_tokens")).toInt();
            return frame;
        }
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    QJsonObject choiceObj = choices.first().toObject();
    int index = choiceObj.value(QStringLiteral("index")).toInt();
    QJsonObject delta = choiceObj.value(QStringLiteral("delta")).toObject();
    QString finishReason = choiceObj.value(QStringLiteral("finish_reason")).toString();

    if (!finishReason.isEmpty() && finishReason != QStringLiteral("null")) {
        StreamFrame frame;
        frame.type = FrameType::Finished;
        frame.candidateIndex = index;
        frame.isFinal = true;
        return frame;
    }

    return parseDeltaChunk(delta, index);
}

DomainFailure OpenAIOutbound::mapFailure(int httpStatus, const QByteArray& body)
{
    QString message;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error == QJsonParseError::NoError) {
        QJsonObject root = doc.object();
        QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
        message = errorObj.value(QStringLiteral("message")).toString();
    }

    if (message.isEmpty()) {
        message = QStringLiteral("OpenAI API error (HTTP %1)").arg(httpStatus);
    }

    ErrorKind kind = mapHttpStatusToKind(httpStatus);
    DomainFailure failure;
    failure.kind = kind;
    failure.code = QStringLiteral("openai.http_%1").arg(httpStatus);
    failure.message = message;
    failure.retryable = (kind == ErrorKind::RateLimited ||
                         kind == ErrorKind::Unavailable ||
                         kind == ErrorKind::Timeout);
    failure.temporary = failure.retryable;
    return failure;
}

// ---------------------------------------------------------------------------
// Protected helpers
// ---------------------------------------------------------------------------

QJsonArray OpenAIOutbound::buildMessages(const QList<InteractionItem>& items) const
{
    QJsonArray messages;
    for (const auto& item : items) {
        QJsonObject msg;
        msg[QStringLiteral("role")] = item.role;

        if (item.role == QStringLiteral("tool")) {
            msg[QStringLiteral("tool_call_id")] = item.toolCallId;
            QString textContent;
            for (const auto& seg : item.content) {
                if (seg.kind == SegmentKind::Text) {
                    textContent += seg.text;
                }
            }
            msg[QStringLiteral("content")] = textContent;
        } else if (item.content.size() == 1
                   && item.content.first().kind == SegmentKind::Text
                   && item.toolCalls.isEmpty()) {
            msg[QStringLiteral("content")] = item.content.first().text;
        } else {
            QJsonArray contentArr;
            for (const auto& seg : item.content) {
                QJsonObject part;
                if (seg.kind == SegmentKind::Text) {
                    part[QStringLiteral("type")] = QStringLiteral("text");
                    part[QStringLiteral("text")] = seg.text;
                } else if (seg.kind == SegmentKind::Media) {
                    part[QStringLiteral("type")] = QStringLiteral("image_url");
                    QJsonObject imageUrl;
                    if (!seg.media.uri.isEmpty()) {
                        imageUrl[QStringLiteral("url")] = seg.media.uri;
                    } else if (!seg.media.inlineData.isEmpty()) {
                        QString dataUri = QStringLiteral("data:") + seg.media.mimeType
                                          + QStringLiteral(";base64,")
                                          + QString::fromLatin1(seg.media.inlineData.toBase64());
                        imageUrl[QStringLiteral("url")] = dataUri;
                    }
                    part[QStringLiteral("image_url")] = imageUrl;
                } else if (seg.kind == SegmentKind::Structured) {
                    part[QStringLiteral("type")] = QStringLiteral("text");
                    part[QStringLiteral("text")] = QString::fromUtf8(
                        QJsonDocument(seg.structured).toJson(QJsonDocument::Compact));
                }
                contentArr.append(part);
            }
            msg[QStringLiteral("content")] = contentArr;
        }

        if (!item.toolCalls.isEmpty()) {
            QJsonArray tcArr;
            for (const auto& tc : item.toolCalls) {
                QJsonObject tcObj;
                tcObj[QStringLiteral("id")] = tc.callId;
                tcObj[QStringLiteral("type")] = QStringLiteral("function");
                QJsonObject fn;
                fn[QStringLiteral("name")] = tc.name;
                fn[QStringLiteral("arguments")] = tc.args;
                tcObj[QStringLiteral("function")] = fn;
                tcArr.append(tcObj);
            }
            msg[QStringLiteral("tool_calls")] = tcArr;
        }

        messages.append(msg);
    }
    return messages;
}

QJsonArray OpenAIOutbound::buildToolDefs(const QList<ActionSpec>& tools) const
{
    QJsonArray arr;
    for (const auto& tool : tools) {
        QJsonObject toolObj;
        toolObj[QStringLiteral("type")] = QStringLiteral("function");
        QJsonObject fn;
        fn[QStringLiteral("name")] = tool.name;
        fn[QStringLiteral("description")] = tool.description;
        fn[QStringLiteral("parameters")] = tool.parameters;
        toolObj[QStringLiteral("function")] = fn;
        arr.append(toolObj);
    }
    return arr;
}

void OpenAIOutbound::buildConstraints(QJsonObject& body, const ConstraintSet& constraints) const
{
    if (constraints.temperature.has_value()) {
        body[QStringLiteral("temperature")] = constraints.temperature.value();
    }
    if (constraints.topP.has_value()) {
        body[QStringLiteral("top_p")] = constraints.topP.value();
    }
    if (constraints.maxTokens.has_value()) {
        body[QStringLiteral("max_tokens")] = constraints.maxTokens.value();
    }
    if (constraints.maxCompletionTokens.has_value()) {
        body[QStringLiteral("max_completion_tokens")] = constraints.maxCompletionTokens.value();
    }
    if (constraints.seed.has_value()) {
        body[QStringLiteral("seed")] = constraints.seed.value();
    }
    if (constraints.frequencyPenalty.has_value()) {
        body[QStringLiteral("frequency_penalty")] = constraints.frequencyPenalty.value();
    }
    if (constraints.presencePenalty.has_value()) {
        body[QStringLiteral("presence_penalty")] = constraints.presencePenalty.value();
    }
    if (!constraints.stopSequences.isEmpty()) {
        QJsonArray stopArr;
        for (const auto& s : constraints.stopSequences) {
            stopArr.append(s);
        }
        body[QStringLiteral("stop")] = stopArr;
    }
}

Candidate OpenAIOutbound::parseChoice(const QJsonObject& choice) const
{
    Candidate c;
    c.index = choice.value(QStringLiteral("index")).toInt();

    QJsonObject msg = choice.value(QStringLiteral("message")).toObject();
    c.role = msg.value(QStringLiteral("role")).toString(QStringLiteral("assistant"));

    QJsonValue contentVal = msg.value(QStringLiteral("content"));
    if (contentVal.isString()) {
        c.output.append(Segment::fromText(contentVal.toString()));
    } else if (contentVal.isArray()) {
        QJsonArray contentArr = contentVal.toArray();
        for (const QJsonValue& part : contentArr) {
            QJsonObject partObj = part.toObject();
            QString type = partObj.value(QStringLiteral("type")).toString();
            if (type == QStringLiteral("text")) {
                c.output.append(Segment::fromText(partObj.value(QStringLiteral("text")).toString()));
            }
        }
    }

    QJsonArray toolCalls = msg.value(QStringLiteral("tool_calls")).toArray();
    for (const QJsonValue& tcv : toolCalls) {
        c.toolCalls.append(parseToolCall(tcv.toObject()));
    }

    QString finishReason = choice.value(QStringLiteral("finish_reason")).toString();
    if (finishReason == QStringLiteral("stop")) {
        c.stopCause = StopCause::Completed;
    } else if (finishReason == QStringLiteral("length")) {
        c.stopCause = StopCause::Length;
    } else if (finishReason == QStringLiteral("content_filter")) {
        c.stopCause = StopCause::ContentFilter;
    } else if (finishReason == QStringLiteral("tool_calls")) {
        c.stopCause = StopCause::ToolCall;
    } else {
        c.stopCause = StopCause::Completed;
    }

    return c;
}

ActionCall OpenAIOutbound::parseToolCall(const QJsonObject& tc) const
{
    ActionCall call;
    call.callId = tc.value(QStringLiteral("id")).toString();
    QJsonObject fn = tc.value(QStringLiteral("function")).toObject();
    call.name = fn.value(QStringLiteral("name")).toString();
    call.args = fn.value(QStringLiteral("arguments")).toString();
    return call;
}

StreamFrame OpenAIOutbound::parseDeltaChunk(const QJsonObject& delta, int index) const
{
    StreamFrame frame;
    frame.candidateIndex = index;

    QJsonArray toolCallsDelta = delta.value(QStringLiteral("tool_calls")).toArray();
    if (!toolCallsDelta.isEmpty()) {
        frame.type = FrameType::ActionDelta;
        QJsonObject tcObj = toolCallsDelta.first().toObject();
        frame.actionDelta.callId = tcObj.value(QStringLiteral("id")).toString();
        QJsonObject fn = tcObj.value(QStringLiteral("function")).toObject();
        frame.actionDelta.name = fn.value(QStringLiteral("name")).toString();
        frame.actionDelta.argsPatch = fn.value(QStringLiteral("arguments")).toString();
        return frame;
    }

    QString content = delta.value(QStringLiteral("content")).toString();
    if (!content.isEmpty()) {
        frame.type = FrameType::Delta;
        frame.deltaSegments.append(Segment::fromText(content));
        return frame;
    }

    frame.type = FrameType::Started;
    return frame;
}

ErrorKind OpenAIOutbound::mapHttpStatusToKind(int httpStatus) const
{
    switch (httpStatus) {
    case 400: return ErrorKind::InvalidInput;
    case 401: return ErrorKind::Unauthorized;
    case 403: return ErrorKind::Forbidden;
    case 404: return ErrorKind::InvalidInput;
    case 429: return ErrorKind::RateLimited;
    case 500: return ErrorKind::Internal;
    case 501: return ErrorKind::NotSupported;
    case 502: return ErrorKind::Unavailable;
    case 503: return ErrorKind::Unavailable;
    case 504: return ErrorKind::Timeout;
    default:
        if (httpStatus >= 500) return ErrorKind::Internal;
        if (httpStatus >= 400) return ErrorKind::InvalidInput;
        return ErrorKind::Internal;
    }
}
