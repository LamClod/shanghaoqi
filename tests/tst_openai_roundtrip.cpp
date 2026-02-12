#include <QTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "adapters/inbound/openai_chat.h"
#include "adapters/outbound/openai.h"
#include "semantic/request.h"
#include "semantic/response.h"
#include "semantic/frame.h"

class TestOpenAIRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void testDecodeSimpleChatRequest() {
        OpenAIChatAdapter inbound;

        QJsonObject body;
        body[QStringLiteral("model")] = QStringLiteral("gpt-4");
        body[QStringLiteral("stream")] = false;

        QJsonArray messages;
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")] = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = QStringLiteral("Hello, how are you?");
        messages.append(userMsg);
        body[QStringLiteral("messages")] = messages;

        QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QMap<QString, QString> meta;

        auto result = inbound.decodeRequest(jsonBody, meta);
        QVERIFY(result.has_value());
        QCOMPARE(result->target.logicalModel, QStringLiteral("gpt-4"));
        QCOMPARE(result->messages.size(), 1);
        QCOMPARE(result->messages[0].role, QStringLiteral("user"));
    }

    void testDecodeWithTools() {
        OpenAIChatAdapter inbound;

        QJsonObject body;
        body[QStringLiteral("model")] = QStringLiteral("gpt-4");

        QJsonArray messages;
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")] = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = QStringLiteral("What's the weather?");
        messages.append(userMsg);
        body[QStringLiteral("messages")] = messages;

        QJsonArray tools;
        QJsonObject tool;
        tool[QStringLiteral("type")] = QStringLiteral("function");
        QJsonObject func;
        func[QStringLiteral("name")] = QStringLiteral("get_weather");
        func[QStringLiteral("description")] = QStringLiteral("Get weather");
        QJsonObject params;
        params[QStringLiteral("type")] = QStringLiteral("object");
        func[QStringLiteral("parameters")] = params;
        tool[QStringLiteral("function")] = func;
        tools.append(tool);
        body[QStringLiteral("tools")] = tools;

        QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
        QMap<QString, QString> meta;

        auto result = inbound.decodeRequest(jsonBody, meta);
        QVERIFY(result.has_value());
        QCOMPARE(result->tools.size(), 1);
        QCOMPARE(result->tools[0].name, QStringLiteral("get_weather"));
    }

    void testEncodeResponse() {
        OpenAIChatAdapter inbound;

        SemanticResponse resp;
        resp.envelope.requestId = QStringLiteral("req-1");
        resp.responseId = QStringLiteral("chatcmpl-123");
        resp.modelUsed = QStringLiteral("gpt-4");

        Candidate c;
        c.index = 0;
        c.role = QStringLiteral("assistant");
        c.output.append(Segment::fromText(QStringLiteral("I'm fine, thanks!")));
        c.stopCause = StopCause::Completed;
        resp.candidates.append(c);

        resp.usage.promptTokens = 10;
        resp.usage.completionTokens = 5;
        resp.usage.totalTokens = 15;

        auto result = inbound.encodeResponse(resp);
        QVERIFY(result.has_value());

        QJsonDocument doc = QJsonDocument::fromJson(*result);
        QVERIFY(!doc.isNull());
        auto obj = doc.object();
        QCOMPARE(obj[QStringLiteral("object")].toString(),
                 QStringLiteral("chat.completion"));
        auto choices = obj[QStringLiteral("choices")].toArray();
        QCOMPARE(choices.size(), 1);
    }

    void testEncodeStreamFrame() {
        OpenAIChatAdapter inbound;

        StreamFrame frame;
        frame.envelope.requestId = QStringLiteral("req-1");
        frame.type = FrameType::Delta;
        frame.candidateIndex = 0;
        frame.deltaSegments.append(Segment::fromText(QStringLiteral("Hello")));

        auto result = inbound.encodeStreamFrame(frame);
        QVERIFY(result.has_value());

        QJsonDocument doc = QJsonDocument::fromJson(*result);
        QVERIFY(!doc.isNull());
        auto obj = doc.object();
        QCOMPARE(obj[QStringLiteral("object")].toString(),
                 QStringLiteral("chat.completion.chunk"));
    }

    void testOutboundBuildRequest() {
        OpenAIOutbound outbound;

        SemanticRequest req;
        req.target.logicalModel = QStringLiteral("gpt-4");
        req.metadata[QStringLiteral("provider_base_url")] =
            QStringLiteral("https://api.openai.com/v1");
        req.metadata[QStringLiteral("provider_api_key")] =
            QStringLiteral("sk-test");

        InteractionItem item;
        item.role = QStringLiteral("user");
        item.content.append(Segment::fromText(QStringLiteral("Hello")));
        req.messages.append(item);

        auto result = outbound.buildRequest(req);
        QVERIFY(result.has_value());
        QCOMPARE(result->method, QStringLiteral("POST"));
        QVERIFY(result->url.contains(QStringLiteral("chat/completions")));
        QVERIFY(result->headers.contains(QStringLiteral("Authorization")));
    }

    void testOutboundParseResponse() {
        OpenAIOutbound outbound;

        QJsonObject resp;
        resp[QStringLiteral("id")] = QStringLiteral("chatcmpl-123");
        resp[QStringLiteral("model")] = QStringLiteral("gpt-4");

        QJsonArray choices;
        QJsonObject choice;
        choice[QStringLiteral("index")] = 0;
        QJsonObject message;
        message[QStringLiteral("role")] = QStringLiteral("assistant");
        message[QStringLiteral("content")] = QStringLiteral("Hi there!");
        choice[QStringLiteral("message")] = message;
        choice[QStringLiteral("finish_reason")] = QStringLiteral("stop");
        choices.append(choice);
        resp[QStringLiteral("choices")] = choices;

        QJsonObject usage;
        usage[QStringLiteral("prompt_tokens")] = 5;
        usage[QStringLiteral("completion_tokens")] = 3;
        usage[QStringLiteral("total_tokens")] = 8;
        resp[QStringLiteral("usage")] = usage;

        ProviderResponse provResp;
        provResp.statusCode = 200;
        provResp.body = QJsonDocument(resp).toJson(QJsonDocument::Compact);

        auto result = outbound.parseResponse(provResp);
        QVERIFY(result.has_value());
        QCOMPARE(result->modelUsed, QStringLiteral("gpt-4"));
        QCOMPARE(result->candidates.size(), 1);
        QCOMPARE(result->usage.totalTokens, 8);
    }

    void testEncodeFailure() {
        OpenAIChatAdapter inbound;

        auto failure = DomainFailure::unauthorized(QStringLiteral("Invalid API key"));
        auto result = inbound.encodeFailure(failure);
        QVERIFY(result.has_value());

        QJsonDocument doc = QJsonDocument::fromJson(*result);
        QVERIFY(!doc.isNull());
        QVERIFY(doc.object().contains(QStringLiteral("error")));
    }
};

QTEST_MAIN(TestOpenAIRoundtrip)
#include "tst_openai_roundtrip.moc"
