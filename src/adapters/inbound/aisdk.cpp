#include "adapters/inbound/aisdk.h"
#include <QUuid>
#include <QDateTime>

QString AiSdkAdapter::protocol() const
{
    return QStringLiteral("aisdk");
}

Result<SemanticRequest> AiSdkAdapter::decodeRequest(
    const QByteArray& body,
    const QMap<QString, QString>& metadata)
{
    return m_openaiHelper.decodeRequest(body, metadata);
}

Result<QByteArray> AiSdkAdapter::encodeResponse(const SemanticResponse& response)
{
    return m_openaiHelper.encodeResponse(response);
}

Result<QByteArray> AiSdkAdapter::encodeStreamFrame(const StreamFrame& frame)
{
    QByteArray result;

    switch (frame.type) {
    case FrameType::Started: {
        break;
    }
    case FrameType::Delta: {
        QString text;
        for (const Segment& seg : frame.deltaSegments) {
            if (seg.kind == SegmentKind::Text)
                text += seg.text;
        }
        if (!text.isEmpty()) {
            QJsonObject textObj;
            textObj[QStringLiteral("v")] = text;
            const QByteArray json = QJsonDocument(textObj).toJson(QJsonDocument::Compact);
            result = QByteArray("0:") + json + QByteArray("\n");
        }
        break;
    }
    case FrameType::ActionDelta: {
        QJsonObject toolObj;
        toolObj[QStringLiteral("toolCallId")] = frame.actionDelta.callId;
        toolObj[QStringLiteral("toolName")] = frame.actionDelta.name;
        toolObj[QStringLiteral("args")] = frame.actionDelta.argsPatch;
        const QByteArray json = QJsonDocument(toolObj).toJson(QJsonDocument::Compact);
        result = QByteArray("9:") + json + QByteArray("\n");
        break;
    }
    case FrameType::Finished: {
        QJsonObject finishObj;
        finishObj[QStringLiteral("finishReason")] = QStringLiteral("stop");
        const QByteArray json = QJsonDocument(finishObj).toJson(QJsonDocument::Compact);
        result = QByteArray("e:") + json + QByteArray("\n");
        break;
    }
    case FrameType::UsageDelta: {
        QJsonObject usageObj;
        usageObj[QStringLiteral("promptTokens")] = frame.usageDelta.promptTokens;
        usageObj[QStringLiteral("completionTokens")] = frame.usageDelta.completionTokens;
        const QByteArray json = QJsonDocument(usageObj).toJson(QJsonDocument::Compact);
        result = QByteArray("d:") + json + QByteArray("\n");
        break;
    }
    case FrameType::Failed: {
        QJsonObject errorObj;
        errorObj[QStringLiteral("message")] = frame.failure.message;
        errorObj[QStringLiteral("code")] = frame.failure.code;
        const QByteArray json = QJsonDocument(errorObj).toJson(QJsonDocument::Compact);
        result = QByteArray("3:") + json + QByteArray("\n");
        break;
    }
    }

    return result;
}

Result<QByteArray> AiSdkAdapter::encodeFailure(const DomainFailure& failure)
{
    QJsonObject errorObj;
    errorObj[QStringLiteral("message")] = failure.message;
    errorObj[QStringLiteral("code")] = failure.code;
    const QByteArray json = QJsonDocument(errorObj).toJson(QJsonDocument::Compact);
    return QByteArray("3:") + json + QByteArray("\n");
}
