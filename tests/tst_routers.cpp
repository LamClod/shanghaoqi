#include <QTest>

#include "proxy/request_router.h"
#include "adapters/inbound/multi_router.h"
#include "adapters/outbound/multi_router.h"

class DummyInboundAdapter final : public IInboundAdapter {
public:
    explicit DummyInboundAdapter(QString name)
        : m_name(std::move(name))
    {
    }

    QString protocol() const override
    {
        return m_name;
    }

    Result<SemanticRequest> decodeRequest(const QByteArray&, const QMap<QString, QString>& metadata) override
    {
        SemanticRequest req;
        req.metadata = metadata;
        return req;
    }

    Result<QByteArray> encodeResponse(const SemanticResponse& response) override
    {
        const QString model = response.modelUsed;
        return model.toUtf8();
    }

    Result<QByteArray> encodeStreamFrame(const StreamFrame&) override
    {
        return QByteArrayLiteral("frame");
    }

    Result<QByteArray> encodeFailure(const DomainFailure& failure) override
    {
        return failure.code.toUtf8();
    }

private:
    QString m_name;
};

class DummyOutboundAdapter final : public IOutboundAdapter {
public:
    explicit DummyOutboundAdapter(QString id)
        : m_id(std::move(id))
    {
    }

    QString adapterId() const override
    {
        return m_id;
    }

    Result<ProviderRequest> buildRequest(const SemanticRequest&) override
    {
        ProviderRequest req;
        req.method = QStringLiteral("POST");
        req.url = QStringLiteral("https://example.test");
        req.adapterHint = m_id;
        return req;
    }

    Result<SemanticResponse> parseResponse(const ProviderResponse&) override
    {
        SemanticResponse resp;
        resp.modelUsed = m_id;
        return resp;
    }

    Result<StreamFrame> parseChunk(const ProviderChunk&) override
    {
        StreamFrame frame;
        frame.type = FrameType::Delta;
        return frame;
    }

    DomainFailure mapFailure(int, const QByteArray& body) override
    {
        return DomainFailure::invalidInput(m_id, QString::fromUtf8(body));
    }

private:
    QString m_id;
};

class TestRouters : public QObject {
    Q_OBJECT

private slots:
    void requestRouter_methodNormalized();
    void requestRouter_wildcardPath();
    void inboundMultiRouter_caseInsensitiveProtocol();
    void outboundMultiRouter_caseInsensitiveResolution();
};

void TestRouters::requestRouter_methodNormalized()
{
    RequestRouter router;
    router.registerDefaults();

    const auto route = router.match(QStringLiteral("get"), QStringLiteral("/v1/models"));
    QVERIFY(route.has_value());
    QCOMPARE(route->inboundProtocol, QStringLiteral("openai"));
}

void TestRouters::requestRouter_wildcardPath()
{
    RequestRouter router;
    router.registerDefaults();

    const auto route = router.match(QStringLiteral("POST"),
                                    QStringLiteral("/gemini/v1beta/models/gemini-2.5-pro:streamGenerateContent"));
    QVERIFY(route.has_value());
    QCOMPARE(route->inboundProtocol, QStringLiteral("gemini"));
}

void TestRouters::inboundMultiRouter_caseInsensitiveProtocol()
{
    InboundMultiRouter router;
    router.registerAdapter(std::make_unique<DummyInboundAdapter>(QStringLiteral("OpenAI.Chat")));

    QMap<QString, QString> meta;
    meta[QStringLiteral("inbound.format")] = QStringLiteral("  OPENAI.CHAT ");

    const auto decoded = router.decodeRequest(QByteArrayLiteral("{}"), meta);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->metadata.value(QStringLiteral("_inbound_protocol")), QStringLiteral("openai.chat"));

    SemanticResponse response;
    response.modelUsed = QStringLiteral("ok");
    response.extensions.set(QStringLiteral("inbound_protocol"), QStringLiteral("OPENAI.CHAT"));

    const auto encoded = router.encodeResponse(response);
    QVERIFY(encoded.has_value());
    QCOMPARE(*encoded, QByteArrayLiteral("ok"));
}

void TestRouters::outboundMultiRouter_caseInsensitiveResolution()
{
    OutboundMultiRouter router;
    router.registerAdapter(std::make_unique<DummyOutboundAdapter>(QStringLiteral("OpenAI")));

    SemanticRequest req;
    req.metadata[QStringLiteral("provider")] = QStringLiteral(" openai ");

    const auto built = router.buildRequest(req);
    QVERIFY(built.has_value());
    QCOMPARE(built->adapterHint, QStringLiteral("OpenAI"));

    ProviderResponse upstream;
    upstream.adapterHint = QStringLiteral("OPENAI");
    const auto parsed = router.parseResponse(upstream);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->modelUsed, QStringLiteral("OpenAI"));

    const DomainFailure mapped = router.mapFailure(400, QByteArrayLiteral("boom"));
    QCOMPARE(mapped.code, QStringLiteral("OpenAI"));
}

QTEST_MAIN(TestRouters)
#include "tst_routers.moc"

