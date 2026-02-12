#include <QTest>
#include "semantic/features/stream_aggregator.h"
#include "semantic/frame.h"
#include "semantic/response.h"

class TestAggregator : public QObject {
    Q_OBJECT

private slots:
    void testAggregateTextFrames() {
        StreamAggregator agg;

        // Started frame
        StreamFrame started;
        started.type = FrameType::Started;
        started.envelope.requestId = QStringLiteral("req-001");
        started.candidateIndex = 0;
        agg.addFrame(started);

        // Delta frames
        StreamFrame d1;
        d1.type = FrameType::Delta;
        d1.candidateIndex = 0;
        d1.deltaSegments.append(Segment::fromText(QStringLiteral("Hello ")));
        agg.addFrame(d1);

        StreamFrame d2;
        d2.type = FrameType::Delta;
        d2.candidateIndex = 0;
        d2.deltaSegments.append(Segment::fromText(QStringLiteral("World")));
        agg.addFrame(d2);

        // Usage
        StreamFrame usage;
        usage.type = FrameType::UsageDelta;
        usage.usageDelta.promptTokens = 10;
        usage.usageDelta.completionTokens = 5;
        usage.usageDelta.totalTokens = 15;
        agg.addFrame(usage);

        // Finished
        StreamFrame fin;
        fin.type = FrameType::Finished;
        fin.candidateIndex = 0;
        fin.isFinal = true;
        agg.addFrame(fin);

        auto result = agg.finalize();
        QVERIFY(result.has_value());
        QCOMPARE(result->candidates.size(), 1);
        QVERIFY(!result->candidates[0].output.isEmpty());
        QCOMPARE(result->usage.totalTokens, 15);
    }

    void testAggregateBatch() {
        QList<StreamFrame> frames;

        StreamFrame started;
        started.type = FrameType::Started;
        started.candidateIndex = 0;
        started.envelope.requestId = QStringLiteral("req-batch");
        frames.append(started);

        StreamFrame delta;
        delta.type = FrameType::Delta;
        delta.candidateIndex = 0;
        delta.deltaSegments.append(Segment::fromText(QStringLiteral("Test")));
        frames.append(delta);

        StreamFrame fin;
        fin.type = FrameType::Finished;
        fin.candidateIndex = 0;
        fin.isFinal = true;
        frames.append(fin);

        StreamAggregator agg;
        auto result = agg.aggregate(frames);
        QVERIFY(result.has_value());
        QCOMPARE(result->candidates.size(), 1);
    }

    void testActionDeltaAggregation() {
        StreamAggregator agg;

        StreamFrame started;
        started.type = FrameType::Started;
        started.candidateIndex = 0;
        agg.addFrame(started);

        StreamFrame ad1;
        ad1.type = FrameType::ActionDelta;
        ad1.candidateIndex = 0;
        ad1.actionDelta.callId = QStringLiteral("call-1");
        ad1.actionDelta.name = QStringLiteral("get_weather");
        ad1.actionDelta.argsPatch = QStringLiteral("{\"loc");
        agg.addFrame(ad1);

        StreamFrame ad2;
        ad2.type = FrameType::ActionDelta;
        ad2.candidateIndex = 0;
        ad2.actionDelta.callId = QStringLiteral("call-1");
        ad2.actionDelta.argsPatch = QStringLiteral("ation\":\"NYC\"}");
        agg.addFrame(ad2);

        StreamFrame fin;
        fin.type = FrameType::Finished;
        fin.candidateIndex = 0;
        agg.addFrame(fin);

        auto result = agg.finalize();
        QVERIFY(result.has_value());
        QCOMPARE(result->candidates.size(), 1);
        QCOMPARE(result->candidates[0].toolCalls.size(), 1);
        QCOMPARE(result->candidates[0].toolCalls[0].name, QStringLiteral("get_weather"));
    }

    void testReset() {
        StreamAggregator agg;

        StreamFrame started;
        started.type = FrameType::Started;
        started.candidateIndex = 0;
        agg.addFrame(started);

        agg.reset();

        auto result = agg.finalize();
        QVERIFY(result.has_value());
        QVERIFY(result->candidates.isEmpty());
    }
};

QTEST_MAIN(TestAggregator)
#include "tst_aggregator.moc"
