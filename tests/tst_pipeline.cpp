#include <QTest>
#include "pipeline/pipeline.h"
#include "pipeline/middleware.h"
#include "pipeline/middlewares/auth_middleware.h"
#include "pipeline/middlewares/model_mapping_middleware.h"
#include "pipeline/middlewares/stream_mode_middleware.h"
#include "pipeline/middlewares/debug_middleware.h"
#include "semantic/request.h"
#include "semantic/response.h"

// Minimal mock inbound adapter for pipeline test
class MockInbound : public IInboundAdapter {
public:
    QString protocol() const override { return QStringLiteral("mock"); }

    Result<SemanticRequest> decodeRequest(
        const QByteArray& body,
        const QMap<QString, QString>&) override
    {
        SemanticRequest req;
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isNull())
            return std::unexpected(DomainFailure::invalidInput(
                QStringLiteral("decode"), QStringLiteral("invalid JSON")));
        auto obj = doc.object();
        req.target.logicalModel = obj[QStringLiteral("model")].toString();
        InteractionItem item;
        item.role = QStringLiteral("user");
        item.content.append(Segment::fromText(
            obj[QStringLiteral("prompt")].toString()));
        req.messages.append(item);
        return req;
    }

    Result<QByteArray> encodeResponse(const SemanticResponse& response) override {
        QJsonObject obj;
        obj[QStringLiteral("model")] = response.modelUsed;
        if (!response.candidates.isEmpty() &&
            !response.candidates[0].output.isEmpty()) {
            obj[QStringLiteral("text")] = response.candidates[0].output[0].text;
        }
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    Result<QByteArray> encodeStreamFrame(const StreamFrame& frame) override {
        QJsonObject obj;
        obj[QStringLiteral("type")] = static_cast<int>(frame.type);
        if (!frame.deltaSegments.isEmpty())
            obj[QStringLiteral("text")] = frame.deltaSegments[0].text;
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    Result<QByteArray> encodeFailure(const DomainFailure& failure) override {
        QJsonObject obj;
        obj[QStringLiteral("error")] = failure.message;
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }
};

class TestPipeline : public QObject {
    Q_OBJECT

private slots:
    void testAuthMiddlewarePass() {
        AuthMiddleware mw(QStringLiteral("test-key"));

        SemanticRequest req;
        req.metadata[QStringLiteral("auth_key")] = QStringLiteral("Bearer test-key");
        auto result = mw.onRequest(std::move(req));
        QVERIFY(result.has_value());
    }

    void testAuthMiddlewareFail() {
        AuthMiddleware mw(QStringLiteral("test-key"));

        SemanticRequest req;
        req.metadata[QStringLiteral("auth_key")] = QStringLiteral("Bearer wrong-key");
        auto result = mw.onRequest(std::move(req));
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().kind, ErrorKind::Unauthorized);
    }

    void testAuthMiddlewareEmptyKeyPassThrough() {
        AuthMiddleware mw{QString{}};

        SemanticRequest req;
        auto result = mw.onRequest(std::move(req));
        QVERIFY(result.has_value());
    }

    void testModelMappingRequest() {
        ModelMappingMiddleware mw(QStringLiteral("local-model"),
                                  QStringLiteral("remote-model"));

        SemanticRequest req;
        req.target.logicalModel = QStringLiteral("local-model");
        auto result = mw.onRequest(std::move(req));
        QVERIFY(result.has_value());
        QCOMPARE(result->target.logicalModel, QStringLiteral("remote-model"));
    }

    void testModelMappingResponse() {
        ModelMappingMiddleware mw(QStringLiteral("local-model"),
                                  QStringLiteral("remote-model"));

        SemanticResponse resp;
        resp.modelUsed = QStringLiteral("remote-model");
        auto result = mw.onResponse(std::move(resp));
        QVERIFY(result.has_value());
        QCOMPARE(result->modelUsed, QStringLiteral("local-model"));
    }

    void testStreamModeMiddleware() {
        StreamModeMiddleware mw(StreamMode::ForceOn, StreamMode::FollowClient);

        SemanticRequest req;
        auto result = mw.onRequest(std::move(req));
        QVERIFY(result.has_value());
        QCOMPARE(result->metadata[QStringLiteral("stream.upstream")],
                 QStringLiteral("true"));
    }

    void testDebugMiddlewareNoOp() {
        DebugMiddleware mw(false);

        SemanticRequest req;
        req.target.logicalModel = QStringLiteral("gpt-4");
        auto result = mw.onRequest(std::move(req));
        QVERIFY(result.has_value());
        QCOMPARE(result->target.logicalModel, QStringLiteral("gpt-4"));
    }

    void testMockInboundDecode() {
        MockInbound inbound;
        QByteArray body = R"({"model":"gpt-4","prompt":"Hello"})";
        QMap<QString, QString> meta;
        auto result = inbound.decodeRequest(body, meta);
        QVERIFY(result.has_value());
        QCOMPARE(result->target.logicalModel, QStringLiteral("gpt-4"));
        QCOMPARE(result->messages.size(), 1);
    }

    void testMockInboundEncode() {
        MockInbound inbound;
        SemanticResponse resp;
        resp.modelUsed = QStringLiteral("gpt-4");
        Candidate c;
        c.output.append(Segment::fromText(QStringLiteral("Hi")));
        resp.candidates.append(c);

        auto result = inbound.encodeResponse(resp);
        QVERIFY(result.has_value());
        QJsonDocument doc = QJsonDocument::fromJson(*result);
        QCOMPARE(doc.object()[QStringLiteral("model")].toString(),
                 QStringLiteral("gpt-4"));
    }
};

QTEST_MAIN(TestPipeline)
#include "tst_pipeline.moc"
