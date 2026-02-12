#include "stream_splitter.h"

StreamSplitter::StreamSplitter(int chunkSize)
    : m_chunkSize(chunkSize > 0 ? chunkSize : 20)
{
}

QList<StreamFrame> StreamSplitter::split(const SemanticResponse& response)
{
    QList<StreamFrame> frames;

    for (const Candidate& candidate : response.candidates) {
        int ci = candidate.index;

        // ---------------------------------------------------------------
        // 1. Emit a Started frame for this candidate
        // ---------------------------------------------------------------
        {
            StreamFrame started;
            started.envelope = response.envelope;
            started.type = FrameType::Started;
            started.candidateIndex = ci;
            started.isFinal = false;

            // Encode response-level metadata into extensions so the
            // aggregator can reconstruct them.
            if (!response.responseId.isEmpty()) {
                started.extensions.set(QStringLiteral("response_id"),
                                       response.responseId);
            }
            if (!response.modelUsed.isEmpty()) {
                started.extensions.set(QStringLiteral("model"),
                                       response.modelUsed);
            }

            frames.append(started);
        }

        // ---------------------------------------------------------------
        // 2. Emit Delta frames for each output segment
        // ---------------------------------------------------------------
        for (const Segment& segment : candidate.output) {
            if (segment.kind == SegmentKind::Text && !segment.text.isEmpty()) {
                // Split text into chunks of m_chunkSize characters
                const QString& text = segment.text;
                int pos = 0;
                while (pos < text.size()) {
                    int len = qMin(m_chunkSize, text.size() - pos);
                    QString chunk = text.mid(pos, len);

                    StreamFrame delta;
                    delta.envelope = response.envelope;
                    delta.type = FrameType::Delta;
                    delta.candidateIndex = ci;
                    delta.deltaSegments.append(Segment::fromText(chunk));
                    delta.isFinal = false;
                    frames.append(delta);

                    pos += len;
                }
            } else {
                // Non-text segments (Media, Structured, Redacted) are emitted
                // as a single Delta frame without chunking.
                StreamFrame delta;
                delta.envelope = response.envelope;
                delta.type = FrameType::Delta;
                delta.candidateIndex = ci;
                delta.deltaSegments.append(segment);
                delta.isFinal = false;
                frames.append(delta);
            }
        }

        // ---------------------------------------------------------------
        // 3. Emit ActionDelta frames for each tool call
        // ---------------------------------------------------------------
        for (const ActionCall& call : candidate.toolCalls) {
            StreamFrame actionFrame;
            actionFrame.envelope = response.envelope;
            actionFrame.type = FrameType::ActionDelta;
            actionFrame.candidateIndex = ci;

            ActionDelta delta;
            delta.callId = call.callId;
            delta.name = call.name;
            delta.argsPatch = call.args;
            actionFrame.actionDelta = delta;

            actionFrame.isFinal = false;
            frames.append(actionFrame);
        }

        // ---------------------------------------------------------------
        // 4. Emit UsageDelta frame (once, for the first candidate only,
        //    to avoid double-counting)
        // ---------------------------------------------------------------
        if (!response.candidates.isEmpty()
            && ci == response.candidates.first().index) {
            StreamFrame usageFrame;
            usageFrame.envelope = response.envelope;
            usageFrame.type = FrameType::UsageDelta;
            usageFrame.candidateIndex = ci;
            usageFrame.usageDelta = response.usage;
            usageFrame.isFinal = false;
            frames.append(usageFrame);
        }

        // ---------------------------------------------------------------
        // 5. Emit Finished frame for this candidate
        // ---------------------------------------------------------------
        {
            StreamFrame finished;
            finished.envelope = response.envelope;
            finished.type = FrameType::Finished;
            finished.candidateIndex = ci;
            finished.isFinal = true;

            // Encode the stop cause so the aggregator can recover it
            finished.extensions.set(QStringLiteral("stop_cause"),
                                    static_cast<int>(candidate.stopCause));

            frames.append(finished);
        }
    }

    // Handle edge case: response with no candidates
    if (response.candidates.isEmpty()) {
        // Still emit usage if present
        if (response.usage.totalTokens > 0
            || response.usage.promptTokens > 0
            || response.usage.completionTokens > 0) {
            StreamFrame usageFrame;
            usageFrame.envelope = response.envelope;
            usageFrame.type = FrameType::UsageDelta;
            usageFrame.candidateIndex = 0;
            usageFrame.usageDelta = response.usage;
            usageFrame.isFinal = false;
            frames.append(usageFrame);
        }

        // Emit a terminal finished frame
        StreamFrame finished;
        finished.envelope = response.envelope;
        finished.type = FrameType::Finished;
        finished.candidateIndex = 0;
        finished.isFinal = true;
        frames.append(finished);
    }

    return frames;
}
