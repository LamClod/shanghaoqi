#include "gemini.h"

QString GeminiOutbound::adapterId() const
{
    return QStringLiteral("gemini");
}

Result<ProviderRequest> GeminiOutbound::buildRequest(const SemanticRequest& request)
{
    ProviderRequest pr;
    pr.method = QStringLiteral("POST");

    QString baseUrl = request.metadata.value(QStringLiteral("provider_base_url"),
                                              QStringLiteral("https://generativelanguage.googleapis.com"));
    QString middleRoute = request.metadata.value(QStringLiteral("middle_route"),
                                                  QStringLiteral("/v1beta"));
    if (!middleRoute.isEmpty() && baseUrl.endsWith(middleRoute))
        middleRoute.clear();
    QString apiKey = request.metadata.value(QStringLiteral("api_key"));
    if (apiKey.isEmpty()) {
        apiKey = request.metadata.value(QStringLiteral("provider_api_key"));
    }
    QString model = request.target.logicalModel;

    const bool stream =
        request.metadata.value(QStringLiteral("stream.upstream")) == QStringLiteral("true") ||
        request.metadata.value(QStringLiteral("stream")) == QStringLiteral("true");
    pr.stream = stream;

    if (stream) {
        pr.url = baseUrl + middleRoute + QStringLiteral("/models/") + model
                 + QStringLiteral(":streamGenerateContent?alt=sse&key=") + apiKey;
    } else {
        pr.url = baseUrl + middleRoute + QStringLiteral("/models/") + model
                 + QStringLiteral(":generateContent?key=") + apiKey;
    }

    pr.headers[QStringLiteral("Content-Type")] = QStringLiteral("application/json");
    // API key is in the URL query parameter for Gemini, but also support header-based
    if (!apiKey.isEmpty()) {
        pr.headers[QStringLiteral("x-goog-api-key")] = apiKey;
    }

    for (auto it = request.metadata.constBegin(); it != request.metadata.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("custom_header."))) {
            const QString headerName = it.key().mid(14);
            if (!headerName.isEmpty()) {
                pr.headers[headerName] = it.value();
            }
        }
    }

    QJsonObject body;

    QJsonArray systemInstruction;
    QJsonArray contents = buildContents(request.messages, systemInstruction);
    body[QStringLiteral("contents")] = contents;

    if (!systemInstruction.isEmpty()) {
        QJsonObject sysObj;
        sysObj[QStringLiteral("parts")] = systemInstruction;
        body[QStringLiteral("systemInstruction")] = sysObj;
    }

    QJsonObject genConfig = buildGenerationConfig(request.constraints);
    if (!genConfig.isEmpty()) {
        body[QStringLiteral("generationConfig")] = genConfig;
    }

    if (!request.tools.isEmpty()) {
        QJsonArray toolDecls = buildToolDeclarations(request.tools);
        QJsonArray toolsArr;
        QJsonObject toolObj;
        toolObj[QStringLiteral("functionDeclarations")] = toolDecls;
        toolsArr.append(toolObj);
        body[QStringLiteral("tools")] = toolsArr;
    }

    pr.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return pr;
}

Result<SemanticResponse> GeminiOutbound::parseResponse(const ProviderResponse& response)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(response.body, &err);
    if (err.error != QJsonParseError::NoError) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Failed to parse Gemini response JSON: ") + err.errorString()));
    }

    QJsonObject root = doc.object();
    SemanticResponse sr;
    sr.kind = TaskKind::Conversation;

    QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
    for (const QJsonValue& cv : candidates) {
        sr.candidates.append(parseGeminiCandidate(cv.toObject()));
    }

    QJsonObject usageMeta = root.value(QStringLiteral("usageMetadata")).toObject();
    sr.usage.promptTokens = usageMeta.value(QStringLiteral("promptTokenCount")).toInt();
    sr.usage.completionTokens = usageMeta.value(QStringLiteral("candidatesTokenCount")).toInt();
    sr.usage.totalTokens = usageMeta.value(QStringLiteral("totalTokenCount")).toInt();

    sr.modelUsed = root.value(QStringLiteral("modelVersion")).toString();

    return sr;
}

Result<StreamFrame> GeminiOutbound::parseChunk(const ProviderChunk& chunk)
{
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
            QStringLiteral("Failed to parse Gemini chunk JSON: ") + err.errorString()));
    }

    QJsonObject root = doc.object();

    // Check for error in stream
    QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
    if (!errorObj.isEmpty()) {
        StreamFrame frame;
        frame.type = FrameType::Failed;
        frame.failure.kind = ErrorKind::Internal;
        frame.failure.message = errorObj.value(QStringLiteral("message")).toString();
        frame.failure.code = errorObj.value(QStringLiteral("status")).toString();
        frame.isFinal = true;
        return frame;
    }

    QJsonArray candidates = root.value(QStringLiteral("candidates")).toArray();
    if (candidates.isEmpty()) {
        // Usage-only update
        QJsonObject usageMeta = root.value(QStringLiteral("usageMetadata")).toObject();
        if (!usageMeta.isEmpty()) {
            StreamFrame frame;
            frame.type = FrameType::UsageDelta;
            frame.usageDelta.promptTokens = usageMeta.value(QStringLiteral("promptTokenCount")).toInt();
            frame.usageDelta.completionTokens = usageMeta.value(QStringLiteral("candidatesTokenCount")).toInt();
            frame.usageDelta.totalTokens = usageMeta.value(QStringLiteral("totalTokenCount")).toInt();
            return frame;
        }
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    QJsonObject candidate = candidates.first().toObject();
    int index = candidate.value(QStringLiteral("index")).toInt();

    QString finishReason = candidate.value(QStringLiteral("finishReason")).toString();
    if (finishReason == QStringLiteral("STOP")
        || finishReason == QStringLiteral("MAX_TOKENS")
        || finishReason == QStringLiteral("SAFETY")
        || finishReason == QStringLiteral("RECITATION")) {

        // Check if there is content in this final chunk
        QJsonObject content = candidate.value(QStringLiteral("content")).toObject();
        QJsonArray parts = content.value(QStringLiteral("parts")).toArray();

        if (!parts.isEmpty()) {
            QJsonObject firstPart = parts.first().toObject();
            if (firstPart.contains(QStringLiteral("functionCall"))) {
                StreamFrame frame;
                frame.type = FrameType::ActionDelta;
                frame.candidateIndex = index;
                QJsonObject fc = firstPart.value(QStringLiteral("functionCall")).toObject();
                frame.actionDelta.name = fc.value(QStringLiteral("name")).toString();
                QJsonObject args = fc.value(QStringLiteral("args")).toObject();
                frame.actionDelta.argsPatch = QString::fromUtf8(
                    QJsonDocument(args).toJson(QJsonDocument::Compact));
                return frame;
            }
            if (firstPart.contains(QStringLiteral("text"))) {
                StreamFrame frame;
                frame.type = FrameType::Delta;
                frame.candidateIndex = index;
                frame.deltaSegments.append(
                    Segment::fromText(firstPart.value(QStringLiteral("text")).toString()));
                return frame;
            }
        }

        StreamFrame frame;
        frame.type = FrameType::Finished;
        frame.candidateIndex = index;
        frame.isFinal = true;
        return frame;
    }

    // Parse content parts from the candidate
    QJsonObject content = candidate.value(QStringLiteral("content")).toObject();
    QJsonArray parts = content.value(QStringLiteral("parts")).toArray();

    if (parts.isEmpty()) {
        StreamFrame frame;
        frame.type = FrameType::Delta;
        frame.candidateIndex = index;
        return frame;
    }

    QJsonObject firstPart = parts.first().toObject();

    if (firstPart.contains(QStringLiteral("functionCall"))) {
        StreamFrame frame;
        frame.type = FrameType::ActionDelta;
        frame.candidateIndex = index;
        QJsonObject fc = firstPart.value(QStringLiteral("functionCall")).toObject();
        frame.actionDelta.name = fc.value(QStringLiteral("name")).toString();
        QJsonObject args = fc.value(QStringLiteral("args")).toObject();
        frame.actionDelta.argsPatch = QString::fromUtf8(
            QJsonDocument(args).toJson(QJsonDocument::Compact));
        return frame;
    }

    if (firstPart.contains(QStringLiteral("text"))) {
        StreamFrame frame;
        frame.type = FrameType::Delta;
        frame.candidateIndex = index;
        frame.deltaSegments.append(
            Segment::fromText(firstPart.value(QStringLiteral("text")).toString()));
        return frame;
    }

    StreamFrame frame;
    frame.type = FrameType::Delta;
    frame.candidateIndex = index;
    return frame;
}

DomainFailure GeminiOutbound::mapFailure(int httpStatus, const QByteArray& body)
{
    QString message;
    QString code;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error == QJsonParseError::NoError) {
        QJsonObject root = doc.object();
        QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
        message = errorObj.value(QStringLiteral("message")).toString();
        code = errorObj.value(QStringLiteral("status")).toString();
    }

    if (message.isEmpty()) {
        message = QStringLiteral("Gemini API error (HTTP %1)").arg(httpStatus);
    }
    if (code.isEmpty()) {
        code = QStringLiteral("gemini.http_%1").arg(httpStatus);
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

QJsonArray GeminiOutbound::buildContents(const QList<InteractionItem>& items,
                                          QJsonArray& systemInstructionOut) const
{
    QJsonArray contents;
    systemInstructionOut = QJsonArray();

    for (const auto& item : items) {
        if (item.role == QStringLiteral("system")) {
            for (const auto& seg : item.content) {
                if (seg.kind == SegmentKind::Text) {
                    QJsonObject part;
                    part[QStringLiteral("text")] = seg.text;
                    systemInstructionOut.append(part);
                }
            }
            continue;
        }

        QJsonObject contentObj;

        // Map roles: assistant -> model, user -> user, tool -> user (with functionResponse)
        if (item.role == QStringLiteral("assistant")) {
            contentObj[QStringLiteral("role")] = QStringLiteral("model");
        } else if (item.role == QStringLiteral("tool")) {
            contentObj[QStringLiteral("role")] = QStringLiteral("user");
            QJsonArray parts;
            QJsonObject frPart;
            QJsonObject functionResponse;
            functionResponse[QStringLiteral("name")] = item.toolCallId;
            QString textContent;
            for (const auto& seg : item.content) {
                if (seg.kind == SegmentKind::Text) {
                    textContent += seg.text;
                }
            }
            QJsonObject responseObj;
            QJsonDocument respDoc = QJsonDocument::fromJson(textContent.toUtf8());
            if (respDoc.isObject()) {
                responseObj = respDoc.object();
            } else {
                responseObj[QStringLiteral("result")] = textContent;
            }
            functionResponse[QStringLiteral("response")] = responseObj;
            frPart[QStringLiteral("functionResponse")] = functionResponse;
            parts.append(frPart);
            contentObj[QStringLiteral("parts")] = parts;
            contents.append(contentObj);
            continue;
        } else {
            contentObj[QStringLiteral("role")] = QStringLiteral("user");
        }

        QJsonArray parts = segmentsToParts(item.content);

        // Add function calls for assistant messages
        for (const auto& tc : item.toolCalls) {
            QJsonObject fcPart;
            QJsonObject functionCall;
            functionCall[QStringLiteral("name")] = tc.name;
            QJsonDocument argsDoc = QJsonDocument::fromJson(tc.args.toUtf8());
            if (argsDoc.isObject()) {
                functionCall[QStringLiteral("args")] = argsDoc.object();
            } else {
                functionCall[QStringLiteral("args")] = QJsonObject();
            }
            fcPart[QStringLiteral("functionCall")] = functionCall;
            parts.append(fcPart);
        }

        contentObj[QStringLiteral("parts")] = parts;
        contents.append(contentObj);
    }

    return contents;
}

QJsonArray GeminiOutbound::buildToolDeclarations(const QList<ActionSpec>& tools) const
{
    QJsonArray arr;
    for (const auto& tool : tools) {
        QJsonObject decl;
        decl[QStringLiteral("name")] = tool.name;
        decl[QStringLiteral("description")] = tool.description;
        decl[QStringLiteral("parameters")] = tool.parameters;
        arr.append(decl);
    }
    return arr;
}

QJsonObject GeminiOutbound::buildGenerationConfig(const ConstraintSet& constraints) const
{
    QJsonObject config;
    if (constraints.temperature.has_value()) {
        config[QStringLiteral("temperature")] = constraints.temperature.value();
    }
    if (constraints.topP.has_value()) {
        config[QStringLiteral("topP")] = constraints.topP.value();
    }
    if (constraints.maxTokens.has_value()) {
        config[QStringLiteral("maxOutputTokens")] = constraints.maxTokens.value();
    }
    if (constraints.maxCompletionTokens.has_value()) {
        config[QStringLiteral("maxOutputTokens")] = constraints.maxCompletionTokens.value();
    }
    if (!constraints.stopSequences.isEmpty()) {
        QJsonArray stopArr;
        for (const auto& s : constraints.stopSequences) {
            stopArr.append(s);
        }
        config[QStringLiteral("stopSequences")] = stopArr;
    }
    if (constraints.seed.has_value()) {
        config[QStringLiteral("seed")] = constraints.seed.value();
    }
    return config;
}

QJsonArray GeminiOutbound::segmentsToParts(const QList<Segment>& segments) const
{
    QJsonArray parts;
    for (const auto& seg : segments) {
        if (seg.kind == SegmentKind::Text) {
            QJsonObject part;
            part[QStringLiteral("text")] = seg.text;
            parts.append(part);
        } else if (seg.kind == SegmentKind::Media) {
            QJsonObject part;
            QJsonObject inlineData;
            inlineData[QStringLiteral("mimeType")] = seg.media.mimeType;
            if (!seg.media.inlineData.isEmpty()) {
                inlineData[QStringLiteral("data")] = QString::fromLatin1(seg.media.inlineData.toBase64());
            } else if (!seg.media.uri.isEmpty()) {
                // For URIs, use fileData
                QJsonObject fileData;
                fileData[QStringLiteral("mimeType")] = seg.media.mimeType;
                fileData[QStringLiteral("fileUri")] = seg.media.uri;
                part[QStringLiteral("fileData")] = fileData;
                parts.append(part);
                continue;
            }
            part[QStringLiteral("inlineData")] = inlineData;
            parts.append(part);
        } else if (seg.kind == SegmentKind::Structured) {
            QJsonObject part;
            part[QStringLiteral("text")] = QString::fromUtf8(
                QJsonDocument(seg.structured).toJson(QJsonDocument::Compact));
            parts.append(part);
        }
    }
    return parts;
}

Candidate GeminiOutbound::parseGeminiCandidate(const QJsonObject& candidate) const
{
    Candidate c;
    c.index = candidate.value(QStringLiteral("index")).toInt();

    QJsonObject content = candidate.value(QStringLiteral("content")).toObject();
    c.role = content.value(QStringLiteral("role")).toString(QStringLiteral("model"));

    QJsonArray parts = content.value(QStringLiteral("parts")).toArray();
    for (const QJsonValue& pv : parts) {
        QJsonObject part = pv.toObject();
        if (part.contains(QStringLiteral("text"))) {
            c.output.append(Segment::fromText(part.value(QStringLiteral("text")).toString()));
        } else if (part.contains(QStringLiteral("functionCall"))) {
            c.toolCalls.append(parseFunctionCall(part));
        }
    }

    QString finishReason = candidate.value(QStringLiteral("finishReason")).toString();
    if (finishReason == QStringLiteral("STOP")) {
        c.stopCause = StopCause::Completed;
    } else if (finishReason == QStringLiteral("MAX_TOKENS")) {
        c.stopCause = StopCause::Length;
    } else if (finishReason == QStringLiteral("SAFETY") || finishReason == QStringLiteral("RECITATION")) {
        c.stopCause = StopCause::ContentFilter;
    } else if (finishReason == QStringLiteral("TOOL_CALLS")) {
        c.stopCause = StopCause::ToolCall;
    } else {
        c.stopCause = StopCause::Completed;
    }

    return c;
}

ActionCall GeminiOutbound::parseFunctionCall(const QJsonObject& part) const
{
    QJsonObject fc = part.value(QStringLiteral("functionCall")).toObject();
    ActionCall call;
    call.name = fc.value(QStringLiteral("name")).toString();
    QJsonObject args = fc.value(QStringLiteral("args")).toObject();
    call.args = QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));
    // Gemini does not provide a call ID in the same way; generate one from name
    call.callId = QStringLiteral("call_") + call.name;
    return call;
}

ErrorKind GeminiOutbound::mapHttpStatusToKind(int httpStatus) const
{
    switch (httpStatus) {
    case 400: return ErrorKind::InvalidInput;
    case 401: return ErrorKind::Unauthorized;
    case 403: return ErrorKind::Forbidden;
    case 404: return ErrorKind::InvalidInput;
    case 429: return ErrorKind::RateLimited;
    case 500: return ErrorKind::Internal;
    case 503: return ErrorKind::Unavailable;
    case 504: return ErrorKind::Timeout;
    default:
        if (httpStatus >= 500) return ErrorKind::Internal;
        if (httpStatus >= 400) return ErrorKind::InvalidInput;
        return ErrorKind::Internal;
    }
}
