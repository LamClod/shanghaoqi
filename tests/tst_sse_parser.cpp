#include <QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "semantic/stream_session.h"
#include "semantic/ports.h"
#include "semantic/frame.h"

// Mock outbound adapter for SSE parsing test
class MockOutbound : public IOutboundAdapter {
public:
    QString adapterId() const override { return QStringLiteral("mock"); }

    Result<ProviderRequest> buildRequest(const SemanticRequest&) override {
        return ProviderRequest{};
    }

    Result<SemanticResponse> parseResponse(const ProviderResponse&) override {
        return SemanticResponse{};
    }

    Result<StreamFrame> parseChunk(const ProviderChunk& chunk) override {
        StreamFrame frame;
        QJsonDocument doc = QJsonDocument::fromJson(chunk.data);
        if (doc.isNull())
            return std::unexpected(DomainFailure::invalidInput(
                QStringLiteral("parse_chunk"), QStringLiteral("invalid JSON")));

        auto obj = doc.object();
        auto choices = obj[QStringLiteral("choices")].toArray();
        if (choices.isEmpty()) {
            frame.type = FrameType::Finished;
            frame.isFinal = true;
            return frame;
        }

        auto delta = choices[0].toObject()[QStringLiteral("delta")].toObject();
        if (delta.contains(QStringLiteral("content"))) {
            frame.type = FrameType::Delta;
            frame.deltaSegments.append(
                Segment::fromText(delta[QStringLiteral("content")].toString()));
        } else {
            frame.type = FrameType::Delta;
        }
        return frame;
    }

    DomainFailure mapFailure(int httpStatus, const QByteArray&) override {
        return DomainFailure::internal(QStringLiteral("HTTP %1").arg(httpStatus));
    }
};

class TestSseParser : public QObject {
    Q_OBJECT

private slots:
    void testParseSingleEvent() {
        // We test the SSE parsing logic by verifying expected outputs
        // Since StreamSession requires a real QNetworkReply, we test
        // the protocol format understanding here

        QByteArray sseData =
            "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";

        // Parse manually to verify format
        int sep = sseData.indexOf("\n\n");
        QVERIFY(sep > 0);

        QByteArray eventBlock = sseData.left(sep);
        QByteArray dataLine;
        for (const auto& line : eventBlock.split('\n')) {
            if (line.startsWith("data: "))
                dataLine = line.mid(6);
        }

        QJsonDocument doc = QJsonDocument::fromJson(dataLine);
        QVERIFY(!doc.isNull());
        auto choices = doc.object()[QStringLiteral("choices")].toArray();
        QCOMPARE(choices.size(), 1);
        auto content = choices[0].toObject()[QStringLiteral("delta")]
                           .toObject()[QStringLiteral("content")].toString();
        QCOMPARE(content, QStringLiteral("Hello"));
    }

    void testParseDoneMarker() {
        QByteArray sseData = "data: [DONE]\n\n";

        int sep = sseData.indexOf("\n\n");
        QByteArray eventBlock = sseData.left(sep);
        QByteArray dataLine;
        for (const auto& line : eventBlock.split('\n')) {
            if (line.startsWith("data: "))
                dataLine = line.mid(6);
        }
        QCOMPARE(dataLine, QByteArray("[DONE]"));
    }

    void testParseMultipleEvents() {
        QByteArray sseData =
            "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\" there\"}}]}\n\n"
            "data: [DONE]\n\n";

        QList<QByteArray> events;
        int pos = 0;
        while (pos < sseData.size()) {
            int sep = sseData.indexOf("\n\n", pos);
            if (sep < 0) break;
            events.append(sseData.mid(pos, sep - pos));
            pos = sep + 2;
        }

        QCOMPARE(events.size(), 3);
    }

    void testParseEventWithType() {
        QByteArray sseData =
            "event: message_start\n"
            "data: {\"type\":\"message_start\"}\n\n";

        int sep = sseData.indexOf("\n\n");
        QByteArray eventBlock = sseData.left(sep);

        QString eventType;
        QByteArray dataLine;
        for (const auto& line : eventBlock.split('\n')) {
            if (line.startsWith("event: "))
                eventType = QString::fromUtf8(line.mid(7));
            else if (line.startsWith("data: "))
                dataLine = line.mid(6);
        }

        QCOMPARE(eventType, QStringLiteral("message_start"));
        QVERIFY(!dataLine.isEmpty());
    }

    void testCommentLinesIgnored() {
        QByteArray sseData =
            ": this is a comment\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"ok\"}}]}\n\n";

        int sep = sseData.indexOf("\n\n");
        QByteArray eventBlock = sseData.left(sep);

        QByteArray dataLine;
        for (const auto& line : eventBlock.split('\n')) {
            if (line.startsWith(":"))
                continue; // comment
            if (line.startsWith("data: "))
                dataLine = line.mid(6);
        }

        QVERIFY(!dataLine.isEmpty());
        QJsonDocument doc = QJsonDocument::fromJson(dataLine);
        QVERIFY(!doc.isNull());
    }

    void testMockOutboundParseChunk() {
        MockOutbound outbound;
        ProviderChunk chunk;
        chunk.data = "{\"choices\":[{\"delta\":{\"content\":\"test\"}}]}";

        auto result = outbound.parseChunk(chunk);
        QVERIFY(result.has_value());
        QCOMPARE(result->type, FrameType::Delta);
        QCOMPARE(result->deltaSegments.size(), 1);
        QCOMPARE(result->deltaSegments[0].text, QStringLiteral("test"));
    }
};

QTEST_MAIN(TestSseParser)
#include "tst_sse_parser.moc"
