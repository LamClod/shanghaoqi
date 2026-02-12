#include "adapters/inbound/antigravity.h"
#include <QJsonDocument>
#include <QJsonObject>

AntigravityAdapter::AntigravityAdapter(IInboundAdapter* chatDelegate, IInboundAdapter* responsesDelegate)
    : m_chatDelegate(chatDelegate)
    , m_responsesDelegate(responsesDelegate)
{
}

QString AntigravityAdapter::protocol() const
{
    return QStringLiteral("antigravity");
}

bool AntigravityAdapter::isResponsesFormat(const QJsonObject& root)
{
    // Responses API uses "input" instead of "messages"
    // and may have "instructions" field
    if (root.contains(QStringLiteral("input")))
        return true;
    if (root.contains(QStringLiteral("instructions")) && !root.contains(QStringLiteral("messages")))
        return true;
    return false;
}

IInboundAdapter* AntigravityAdapter::delegateFromProtocol(const QString& protocol) const
{
    if (protocol == QStringLiteral("openai.responses")) {
        return m_responsesDelegate;
    }
    if (protocol == QStringLiteral("openai.chat")) {
        return m_chatDelegate;
    }
    return nullptr;
}

IInboundAdapter* AntigravityAdapter::delegateFromRequest(const QJsonObject& root) const
{
    return isResponsesFormat(root) ? m_responsesDelegate : m_chatDelegate;
}

Result<SemanticRequest> AntigravityAdapter::decodeRequest(
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
    IInboundAdapter* delegate = delegateFromRequest(root);
    if (!delegate) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Antigravity delegate is not configured")));
    }

    auto result = delegate->decodeRequest(body, metadata);
    if (result.has_value()) {
        result->metadata[QStringLiteral("_client")] = QStringLiteral("antigravity");
        result->metadata[QStringLiteral("_antigravity_delegate")] =
            isResponsesFormat(root) ? QStringLiteral("openai.responses")
                                    : QStringLiteral("openai.chat");
    }
    return result;
}

Result<QByteArray> AntigravityAdapter::encodeResponse(const SemanticResponse& response)
{
    const QString delegateKey = response.extensions.get(QStringLiteral("antigravity_delegate")).toString();
    IInboundAdapter* delegate = delegateFromProtocol(delegateKey);
    if (!delegate) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Antigravity delegate is not available for response")));
    }
    return delegate->encodeResponse(response);
}

Result<QByteArray> AntigravityAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    const QString delegateKey = frame.extensions.get(QStringLiteral("antigravity_delegate")).toString();
    IInboundAdapter* delegate = delegateFromProtocol(delegateKey);
    if (!delegate) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Antigravity delegate is not available for stream frame")));
    }
    return delegate->encodeStreamFrame(frame);
}

Result<QByteArray> AntigravityAdapter::encodeFailure(const DomainFailure& failure)
{
    Q_UNUSED(failure);
    if (!m_chatDelegate) {
        return std::unexpected(DomainFailure::internal(
            QStringLiteral("Antigravity chat delegate is not configured")));
    }
    return m_chatDelegate->encodeFailure(failure);
}
