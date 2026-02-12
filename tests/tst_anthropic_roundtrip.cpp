#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "adapters/inbound/anthropic.h"
#include "adapters/outbound/anthropic.h"
#include "semantic/request.h"
#include "semantic/response.h"
#include "semantic/frame.h"

class TestAnthropicRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void testDecodeRequest() {
        AnthropicAdapter inbound;

        QJsonObject body;
        body[QStringLiteral("model")] = QStringLiteral("claude-3-opus-20240229");
        body[QStringLiteral("max_tokens")] = 1024;
        body[QStringLiteral("system")] = QStringLiteral("You are helpful.");

        QJsonArray messages;
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")] = QStringLiteral("user");
        QJsonArray content;
        QJsonObject textBlock;
        textBlock[QStringLiteral("type")] = QStringLiteral("text");
        textBlock[QStringLiteral("text")] = QStringLiteral("Hello Claude");
        content.append(textBlock);
        userMsg[QStringLiteral("content")] = content;
        messages.append(userMsg);
        body[QStringLiteral("messages")] = messages;

        QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QMap<QString, QString> meta;

        auto result = inbound.decodeRequest(jsonBody, meta);
        QVERIFY(result.has_value());
        QCOMPARE(result->target.logicalModel,
                 QStringLiteral("claude-3-opus-20240229"));
        // system message + user message
        QVERIFY(result->messages.size() >= 1);
    }

    void testDecodeRequestWithStringContent() {
        AnthropicAdapter inbound;

        QJsonObject body;
        body[QStringLiteral("model")] = QStringLiteral("claude-3-opus-20240229");
        body[QStringLiteral("max_tokens")] = 1024;

        QJsonArray messages;
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")] = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = QStringLiteral("Simple text");
        messages.append(userMsg);
        body[QStringLiteral("messages")] = messages;

        QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QMap<QString, QString> meta;

        auto result = inbound.decodeRequest(jsonBody, meta);
        QVERIFY(result.has_value());
        QCOMPARE(result->messages.size(), 1);
        QCOMPARE(result->messages[0].role, QStringLiteral("user"));
    }

    void testEncodeResponse() {
        AnthropicAdapter inbound;

        SemanticResponse resp;
        resp.responseId = QStringLiteral("msg_123");
        resp.modelUsed = QStringLiteral("claude-3-opus-20240229");

        Candidate c;
        c.index = 0;
        c.role = QStringLiteral("assistant");
        c.output.append(Segment::fromText(QStringLiteral("Hello! How can I help?")));
        c.stopCause = StopCause::Completed;
        resp.candidates.append(c);

        resp.usage.promptTokens = 10;
        resp.usage.completionTokens = 8;
        resp.usage.totalTokens = 18;

        auto result = inbound.encodeResponse(resp);
        QVERIFY(result.has_value());

        QJsonDocument doc = QJsonDocument::fromJson(*result);
        QVERIFY(!doc.isNull());
        auto obj = doc.object();
        QCOMPARE(obj[QStringLiteral("type")].toString(), QStringLiteral("message"));
        QCOMPARE(obj[QStringLiteral("role")].toString(), QStringLiteral("assistant"));
    }

    void testEncodeStreamFrameDelta() {
        AnthropicAdapter inbound;

        StreamFrame frame;
        frame.type = FrameType::Delta;
        frame.candidateIndex = 0;
        frame.deltaSegments.append(Segment::fromText(QStringLiteral("Hello")));

        auto result = inbound.encodeStreamFrame(frame);
        QVERIFY(result.has_value());
        QVERIFY(!result->isEmpty());
    }

    void testEncodeStreamFrameFinished() {
        AnthropicAdapter inbound;

        StreamFrame frame;
        frame.type = FrameType::Finished;
        frame.isFinal = true;

        auto result = inbound.encodeStreamFrame(frame);
        QVERIFY(result.has_value());
    }

    void testOutboundBuildRequest() {
        AnthropicOutbound outbound;

        SemanticRequest req;
        req.target.logicalModel = QStringLiteral("claude-3-opus-20240229");
        req.metadata[QStringLiteral("provider_base_url")] =
            QStringLiteral("https://api.anthropic.com/v1");
        req.metadata[QStringLiteral("provider_api_key")] =
            QStringLiteral("sk-ant-test");
        req.constraints.maxTokens = 1024;

        InteractionItem sys;
        sys.role = QStringLiteral("system");
        sys.content.append(Segment::fromText(QStringLiteral("You are helpful")));
        req.messages.append(sys);

        InteractionItem user;
        user.role = QStringLiteral("user");
        user.content.append(Segment::fromText(QStringLiteral("Hi")));
        req.messages.append(user);

        auto result = outbound.buildRequest(req);
        QVERIFY(result.has_value());
        QCOMPARE(result->method, QStringLiteral("POST"));
        QVERIFY(result->url.contains(QStringLiteral("messages")));
        QVERIFY(result->headers.contains(QStringLiteral("x-api-key")));
    }

    void testOutboundParseResponse() {
        AnthropicOutbound outbound;

        QJsonObject resp;
        resp[QStringLiteral("id")] = QStringLiteral("msg_123");
        resp[QStringLiteral("type")] = QStringLiteral("message");
        resp[QStringLiteral("model")] = QStringLiteral("claude-3-opus-20240229");
        resp[QStringLiteral("role")] = QStringLiteral("assistant");
        resp[QStringLiteral("stop_reason")] = QStringLiteral("end_turn");

        QJsonArray content;
        QJsonObject textBlock;
        textBlock[QStringLiteral("type")] = QStringLiteral("text");
        textBlock[QStringLiteral("text")] = QStringLiteral("Hi there!");
        content.append(textBlock);
        resp[QStringLiteral("content")] = content;

        QJsonObject usage;
        usage[QStringLiteral("input_tokens")] = 10;
        usage[QStringLiteral("output_tokens")] = 5;
        resp[QStringLiteral("usage")] = usage;

        ProviderResponse provResp;
        provResp.statusCode = 200;
        provResp.body = QJsonDocument(resp).toJson(QJsonDocument::Compact);

        auto result = outbound.parseResponse(provResp);
        QVERIFY(result.has_value());
        QCOMPARE(result->modelUsed,
                 QStringLiteral("claude-3-opus-20240229"));
        QCOMPARE(result->candidates.size(), 1);
    }

    void testEncodeFailure() {
        AnthropicAdapter inbound;

        auto failure = DomainFailure::rateLimited(QStringLiteral("Too many requests"));
        auto result = inbound.encodeFailure(failure);
        QVERIFY(result.has_value());

        QJsonDocument doc = QJsonDocument::fromJson(*result);
        QVERIFY(!doc.isNull());
        QVERIFY(doc.object().contains(QStringLiteral("error")));
    }
};

QTEST_MAIN(TestAnthropicRoundtrip)
#include "tst_anthropic_roundtrip.moc"
