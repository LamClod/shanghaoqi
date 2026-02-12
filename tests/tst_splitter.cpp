#include <QTest>
#include "semantic/features/stream_splitter.h"
#include "semantic/response.h"
#include "semantic/frame.h"

class TestSplitter : public QObject {
    Q_OBJECT

private slots:
    void testSplitSimpleResponse() {
        SemanticResponse resp;
        resp.envelope.requestId = QStringLiteral("req-001");
        resp.responseId = QStringLiteral("resp-001");
        resp.modelUsed = QStringLiteral("gpt-4");

        Candidate c;
        c.index = 0;
        c.role = QStringLiteral("assistant");
        c.output.append(Segment::fromText(QStringLiteral("Hello World")));
        c.stopCause = StopCause::Completed;
        resp.candidates.append(c);

        resp.usage.promptTokens = 5;
        resp.usage.completionTokens = 2;
        resp.usage.totalTokens = 7;

        StreamSplitter splitter(20);
        auto frames = splitter.split(resp);

        QVERIFY(frames.size() >= 3); // at least Started + Delta + Finished
        QCOMPARE(frames.first().type, FrameType::Started);
        QCOMPARE(frames.last().type, FrameType::Finished);
    }

    void testSplitLongTextChunked() {
        SemanticResponse resp;
        resp.envelope.requestId = QStringLiteral("req-002");

        Candidate c;
        c.index = 0;
        c.role = QStringLiteral("assistant");
        // 60 chars, chunk size 20 -> 3 delta frames
        c.output.append(Segment::fromText(QStringLiteral("123456789012345678901234567890123456789012345678901234567890")));
        c.stopCause = StopCause::Completed;
        resp.candidates.append(c);

        StreamSplitter splitter(20);
        auto frames = splitter.split(resp);

        int deltaCount = 0;
        for (const auto& f : frames) {
            if (f.type == FrameType::Delta)
                ++deltaCount;
        }
        QCOMPARE(deltaCount, 3);
    }

    void testSplitWithToolCalls() {
        SemanticResponse resp;
        resp.envelope.requestId = QStringLiteral("req-003");

        Candidate c;
        c.index = 0;
        c.role = QStringLiteral("assistant");
        c.stopCause = StopCause::ToolCall;

        ActionCall call;
        call.callId = QStringLiteral("call-1");
        call.name = QStringLiteral("get_weather");
        call.args = QStringLiteral("{\"location\":\"NYC\"}");
        c.toolCalls.append(call);
        resp.candidates.append(c);

        StreamSplitter splitter;
        auto frames = splitter.split(resp);

        bool hasActionDelta = false;
        for (const auto& f : frames) {
            if (f.type == FrameType::ActionDelta)
                hasActionDelta = true;
        }
        QVERIFY(hasActionDelta);
    }

    void testSplitMultipleCandidates() {
        SemanticResponse resp;
        resp.envelope.requestId = QStringLiteral("req-004");

        for (int i = 0; i < 3; ++i) {
            Candidate c;
            c.index = i;
            c.role = QStringLiteral("assistant");
            c.output.append(Segment::fromText(QStringLiteral("Option %1").arg(i)));
            c.stopCause = StopCause::Completed;
            resp.candidates.append(c);
        }

        StreamSplitter splitter;
        auto frames = splitter.split(resp);

        int startedCount = 0;
        int finishedCount = 0;
        for (const auto& f : frames) {
            if (f.type == FrameType::Started) ++startedCount;
            if (f.type == FrameType::Finished) ++finishedCount;
        }
        QCOMPARE(startedCount, 3);
        QCOMPARE(finishedCount, 3);
    }
};

QTEST_MAIN(TestSplitter)
#include "tst_splitter.moc"
