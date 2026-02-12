#include <QTest>
#include "semantic/validate.h"
#include "semantic/request.h"
#include "semantic/response.h"
#include "semantic/frame.h"

class TestValidate : public QObject {
    Q_OBJECT

private slots:
    void testValidRequest() {
        SemanticRequest req;
        req.envelope.requestId = QStringLiteral("req-001");
        req.kind = TaskKind::Conversation;
        req.target.logicalModel = QStringLiteral("gpt-4");

        InteractionItem item;
        item.role = QStringLiteral("user");
        item.content.append(Segment::fromText(QStringLiteral("Hello")));
        req.messages.append(item);

        auto result = Validate::request(req);
        QVERIFY(result.has_value());
    }

    void testEmptyMessagesInvalid() {
        SemanticRequest req;
        req.envelope.requestId = QStringLiteral("req-002");
        req.target.logicalModel = QStringLiteral("gpt-4");

        auto result = Validate::request(req);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().kind, ErrorKind::InvalidInput);
    }

    void testEmptyModelInvalid() {
        SemanticRequest req;
        req.envelope.requestId = QStringLiteral("req-003");

        InteractionItem item;
        item.role = QStringLiteral("user");
        item.content.append(Segment::fromText(QStringLiteral("Hi")));
        req.messages.append(item);

        auto result = Validate::request(req);
        QVERIFY(!result.has_value());
    }

    void testValidResponse() {
        SemanticResponse resp;
        resp.envelope.requestId = QStringLiteral("req-001");
        resp.responseId = QStringLiteral("resp-001");
        resp.modelUsed = QStringLiteral("gpt-4");

        Candidate c;
        c.role = QStringLiteral("assistant");
        c.output.append(Segment::fromText(QStringLiteral("Hello back")));
        resp.candidates.append(c);

        auto result = Validate::response(resp);
        QVERIFY(result.has_value());
    }

    void testValidFrame() {
        StreamFrame frame;
        frame.envelope.requestId = QStringLiteral("req-001");
        frame.type = FrameType::Delta;
        frame.deltaSegments.append(Segment::fromText(QStringLiteral("chunk")));

        auto result = Validate::frame(frame);
        QVERIFY(result.has_value());
    }
};

QTEST_MAIN(TestValidate)
#include "tst_validate.moc"
