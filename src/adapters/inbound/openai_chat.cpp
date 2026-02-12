#include "adapters/inbound/openai_chat.h"

QString OpenAIChatAdapter::protocol() const
{
    return QStringLiteral("openai");
}

QString OpenAIChatAdapter::generateChatId()
{
    return QStringLiteral("chatcmpl-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString OpenAIChatAdapter::stopCauseToFinishReason(StopCause cause)
{
    switch (cause) {
    case StopCause::Completed:     return QStringLiteral("stop");
    case StopCause::Length:        return QStringLiteral("length");
    case StopCause::ContentFilter: return QStringLiteral("content_filter");
    case StopCause::ToolCall:      return QStringLiteral("tool_calls");
    }
    return QStringLiteral("stop");
}

QList<Segment> OpenAIChatAdapter::parseContentField(const QJsonValue& content)
{
    QList<Segment> segments;
    if (content.isString()) {
        segments.append(Segment::fromText(content.toString()));
    } else if (content.isArray()) {
        const QJsonArray parts = content.toArray();
        for (const QJsonValue& pv : parts) {
            const QJsonObject p = pv.toObject();
            const QString type = p[QStringLiteral("type")].toString();
            if (type == QStringLiteral("text")) {
                segments.append(Segment::fromText(p[QStringLiteral("text")].toString()));
            } else if (type == QStringLiteral("image_url")) {
                const QJsonObject imgObj = p[QStringLiteral("image_url")].toObject();
                MediaRef ref;
                ref.uri = imgObj[QStringLiteral("url")].toString();
                ref.mimeType = QStringLiteral("image/*");
                segments.append(Segment::fromMedia(ref));
            }
        }
    }
    return segments;
}

QList<ActionCall> OpenAIChatAdapter::parseToolCalls(const QJsonArray& toolCalls)
{
    QList<ActionCall> calls;
    for (const QJsonValue& tcv : toolCalls) {
        const QJsonObject tc = tcv.toObject();
        ActionCall call;
        call.callId = tc[QStringLiteral("id")].toString();
        const QJsonObject fn = tc[QStringLiteral("function")].toObject();
        call.name = fn[QStringLiteral("name")].toString();
        call.args = fn[QStringLiteral("arguments")].toString();
        calls.append(call);
    }
    return calls;
}

QJsonArray OpenAIChatAdapter::serializeToolCalls(const QList<ActionCall>& calls)
{
    QJsonArray arr;
    for (const ActionCall& call : calls) {
        QJsonObject tcObj;
        tcObj[QStringLiteral("id")] = call.callId;
        tcObj[QStringLiteral("type")] = QStringLiteral("function");
        QJsonObject fn;
        fn[QStringLiteral("name")] = call.name;
        fn[QStringLiteral("arguments")] = call.args;
        tcObj[QStringLiteral("function")] = fn;
        arr.append(tcObj);
    }
    return arr;
}

Result<SemanticRequest> OpenAIChatAdapter::decodeRequest(
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

    // Parse messages
    const QJsonArray msgs = root[QStringLiteral("messages")].toArray();
    for (const QJsonValue& mv : msgs) {
        const QJsonObject m = mv.toObject();
        InteractionItem item;
        item.role = m[QStringLiteral("role")].toString();
        item.content = parseContentField(m[QStringLiteral("content")]);

        // Tool calls in assistant messages
        if (m.contains(QStringLiteral("tool_calls"))) {
            item.toolCalls = parseToolCalls(m[QStringLiteral("tool_calls")].toArray());
        }

        // Tool call ID for tool role messages
        if (m.contains(QStringLiteral("tool_call_id"))) {
            item.toolCallId = m[QStringLiteral("tool_call_id")].toString();
        }

        req.messages.append(item);
    }

    // Parse constraints
    if (root.contains(QStringLiteral("temperature")))
        req.constraints.temperature = root[QStringLiteral("temperature")].toDouble();
    if (root.contains(QStringLiteral("top_p")))
        req.constraints.topP = root[QStringLiteral("top_p")].toDouble();
    if (root.contains(QStringLiteral("max_tokens")))
        req.constraints.maxTokens = root[QStringLiteral("max_tokens")].toInt();
    if (root.contains(QStringLiteral("max_completion_tokens")))
        req.constraints.maxCompletionTokens = root[QStringLiteral("max_completion_tokens")].toInt();
    if (root.contains(QStringLiteral("seed")))
        req.constraints.seed = root[QStringLiteral("seed")].toInt();
    if (root.contains(QStringLiteral("frequency_penalty")))
        req.constraints.frequencyPenalty = root[QStringLiteral("frequency_penalty")].toDouble();
    if (root.contains(QStringLiteral("presence_penalty")))
        req.constraints.presencePenalty = root[QStringLiteral("presence_penalty")].toDouble();
    if (root.contains(QStringLiteral("stop"))) {
        const QJsonValue stopVal = root[QStringLiteral("stop")];
        if (stopVal.isString()) {
            req.constraints.stopSequences.append(stopVal.toString());
        } else if (stopVal.isArray()) {
            for (const QJsonValue& sv : stopVal.toArray())
                req.constraints.stopSequences.append(sv.toString());
        }
    }

    // Parse tools
    const QJsonArray tools = root[QStringLiteral("tools")].toArray();
    for (const QJsonValue& tv : tools) {
        const QJsonObject t = tv.toObject();
        if (t[QStringLiteral("type")].toString() == QStringLiteral("function")) {
            const QJsonObject fn = t[QStringLiteral("function")].toObject();
            ActionSpec spec;
            spec.name = fn[QStringLiteral("name")].toString();
            spec.description = fn[QStringLiteral("description")].toString();
            spec.parameters = fn[QStringLiteral("parameters")].toObject();
            req.tools.append(spec);
        }
    }

    // Stream flag
    if (root[QStringLiteral("stream")].toBool())
        req.metadata[QStringLiteral("stream")] = QStringLiteral("true");

    // Copy incoming metadata
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it)
        req.metadata[it.key()] = it.value();

    return req;
}

Result<QByteArray> OpenAIChatAdapter::encodeResponse(const SemanticResponse& response)
{
    QJsonObject root;
    root[QStringLiteral("id")] = response.responseId.isEmpty()
        ? generateChatId() : response.responseId;
    root[QStringLiteral("object")] = QStringLiteral("chat.completion");
    root[QStringLiteral("model")] = response.modelUsed;
    root[QStringLiteral("created")] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch());

    QJsonArray choices;
    for (const Candidate& cand : response.candidates) {
        QJsonObject choice;
        choice[QStringLiteral("index")] = cand.index;

        QJsonObject message;
        message[QStringLiteral("role")] = cand.role.isEmpty()
            ? QStringLiteral("assistant") : cand.role;

        // Concatenate text segments into content
        QString textContent;
        for (const Segment& seg : cand.output) {
            if (seg.kind == SegmentKind::Text)
                textContent += seg.text;
        }
        message[QStringLiteral("content")] = textContent;

        // Tool calls
        if (!cand.toolCalls.isEmpty()) {
            message[QStringLiteral("tool_calls")] = serializeToolCalls(cand.toolCalls);
        }

        choice[QStringLiteral("message")] = message;
        choice[QStringLiteral("finish_reason")] = stopCauseToFinishReason(cand.stopCause);
        choices.append(choice);
    }
    root[QStringLiteral("choices")] = choices;

    QJsonObject usage;
    usage[QStringLiteral("prompt_tokens")] = response.usage.promptTokens;
    usage[QStringLiteral("completion_tokens")] = response.usage.completionTokens;
    usage[QStringLiteral("total_tokens")] = response.usage.totalTokens;
    root[QStringLiteral("usage")] = usage;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result<QByteArray> OpenAIChatAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    QJsonObject root;
    root[QStringLiteral("id")] = generateChatId();
    root[QStringLiteral("object")] = QStringLiteral("chat.completion.chunk");

    QJsonArray choices;
    QJsonObject choice;
    choice[QStringLiteral("index")] = frame.candidateIndex;

    switch (frame.type) {
    case FrameType::Started: {
        QJsonObject delta;
        delta[QStringLiteral("role")] = QStringLiteral("assistant");
        delta[QStringLiteral("content")] = QStringLiteral("");
        choice[QStringLiteral("delta")] = delta;
        choice[QStringLiteral("finish_reason")] = QJsonValue::Null;
        break;
    }
    case FrameType::Delta: {
        QJsonObject delta;
        QString text;
        for (const Segment& seg : frame.deltaSegments) {
            if (seg.kind == SegmentKind::Text)
                text += seg.text;
        }
        delta[QStringLiteral("content")] = text;
        choice[QStringLiteral("delta")] = delta;
        choice[QStringLiteral("finish_reason")] = QJsonValue::Null;
        break;
    }
    case FrameType::ActionDelta: {
        QJsonObject delta;
        QJsonArray tcs;
        QJsonObject tc;
        tc[QStringLiteral("index")] = 0;
        if (!frame.actionDelta.callId.isEmpty()) {
            tc[QStringLiteral("id")] = frame.actionDelta.callId;
            tc[QStringLiteral("type")] = QStringLiteral("function");
        }
        QJsonObject fn;
        if (!frame.actionDelta.name.isEmpty())
            fn[QStringLiteral("name")] = frame.actionDelta.name;
        fn[QStringLiteral("arguments")] = frame.actionDelta.argsPatch;
        tc[QStringLiteral("function")] = fn;
        tcs.append(tc);
        delta[QStringLiteral("tool_calls")] = tcs;
        choice[QStringLiteral("delta")] = delta;
        choice[QStringLiteral("finish_reason")] = QJsonValue::Null;
        break;
    }
    case FrameType::Finished: {
        choice[QStringLiteral("delta")] = QJsonObject();
        choice[QStringLiteral("finish_reason")] = QStringLiteral("stop");
        break;
    }
    case FrameType::UsageDelta: {
        QJsonObject delta;
        choice[QStringLiteral("delta")] = delta;
        choice[QStringLiteral("finish_reason")] = QJsonValue::Null;
        QJsonObject usage;
        usage[QStringLiteral("prompt_tokens")] = frame.usageDelta.promptTokens;
        usage[QStringLiteral("completion_tokens")] = frame.usageDelta.completionTokens;
        usage[QStringLiteral("total_tokens")] = frame.usageDelta.totalTokens;
        root[QStringLiteral("usage")] = usage;
        break;
    }
    case FrameType::Failed: {
        choice[QStringLiteral("delta")] = QJsonObject();
        choice[QStringLiteral("finish_reason")] = QStringLiteral("stop");
        break;
    }
    }

    choices.append(choice);
    root[QStringLiteral("choices")] = choices;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

Result<QByteArray> OpenAIChatAdapter::encodeFailure(const DomainFailure& failure)
{
    QJsonObject root;
    QJsonObject errorObj;
    errorObj[QStringLiteral("message")] = failure.message;
    errorObj[QStringLiteral("type")] = failure.code.isEmpty()
        ? QStringLiteral("invalid_request_error") : failure.code;
    errorObj[QStringLiteral("code")] = failure.code;
    root[QStringLiteral("error")] = errorObj;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
