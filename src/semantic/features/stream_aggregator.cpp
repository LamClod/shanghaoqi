#include "stream_aggregator.h"

// ---------------------------------------------------------------------------
// Convenience batch API
// ---------------------------------------------------------------------------

Result<SemanticResponse> StreamAggregator::aggregate(const QList<StreamFrame>& frames)
{
    reset();
    for (const StreamFrame& frame : frames) {
        addFrame(frame);
    }
    return finalize();
}

// ---------------------------------------------------------------------------
// Incremental frame ingestion
// ---------------------------------------------------------------------------

void StreamAggregator::addFrame(const StreamFrame& frame)
{
    switch (frame.type) {

    case FrameType::Started: {
        // Capture envelope from the first Started frame we see
        if (m_envelope.requestId.isEmpty()) {
            m_envelope = frame.envelope;
        }
        // Initialize candidate state for this index if not already present
        if (!m_states.contains(frame.candidateIndex)) {
            CandidateState state;
            state.candidate.index = frame.candidateIndex;
            state.candidate.role = QStringLiteral("assistant");
            m_states[frame.candidateIndex] = state;
        }
        // Pick up response-level metadata from extensions if available
        if (frame.extensions.has(QStringLiteral("response_id"))) {
            m_responseId = frame.extensions.get(
                QStringLiteral("response_id")).toString();
        }
        if (frame.extensions.has(QStringLiteral("model"))) {
            m_modelUsed = frame.extensions.get(
                QStringLiteral("model")).toString();
        }
        break;
    }

    case FrameType::Delta: {
        // Ensure candidate state exists
        if (!m_states.contains(frame.candidateIndex)) {
            CandidateState state;
            state.candidate.index = frame.candidateIndex;
            state.candidate.role = QStringLiteral("assistant");
            m_states[frame.candidateIndex] = state;
        }
        applyDelta(m_states[frame.candidateIndex], frame);
        break;
    }

    case FrameType::ActionDelta: {
        // Ensure candidate state exists
        if (!m_states.contains(frame.candidateIndex)) {
            CandidateState state;
            state.candidate.index = frame.candidateIndex;
            state.candidate.role = QStringLiteral("assistant");
            m_states[frame.candidateIndex] = state;
        }
        applyActionDelta(m_states[frame.candidateIndex], frame.actionDelta);
        break;
    }

    case FrameType::UsageDelta: {
        applyUsage(m_totalUsage, frame.usageDelta);
        break;
    }

    case FrameType::Finished: {
        if (m_states.contains(frame.candidateIndex)) {
            CandidateState& state = m_states[frame.candidateIndex];
            // Extract stop cause from extensions if the splitter encoded it
            if (frame.extensions.has(QStringLiteral("stop_cause"))) {
                int raw = frame.extensions.get(
                    QStringLiteral("stop_cause")).toInt();
                state.candidate.stopCause = static_cast<StopCause>(raw);
            } else {
                state.candidate.stopCause = StopCause::Completed;
            }
        }
        break;
    }

    case FrameType::Failed: {
        m_hasFailed = true;
        m_lastFailure = frame.failure;
        break;
    }

    }  // end switch
}

// ---------------------------------------------------------------------------
// Finalize: build the SemanticResponse from accumulated state
// ---------------------------------------------------------------------------

Result<SemanticResponse> StreamAggregator::finalize()
{
    if (m_hasFailed) {
        DomainFailure failure = m_lastFailure;
        reset();
        return std::unexpected(failure);
    }

    SemanticResponse response;
    response.envelope = m_envelope;
    response.responseId = m_responseId;
    response.kind = TaskKind::Conversation;
    response.modelUsed = m_modelUsed;
    response.usage = m_totalUsage;

    // Collect candidates in index order
    QList<int> indices = m_states.keys();
    std::sort(indices.begin(), indices.end());
    for (int idx : indices) {
        response.candidates.append(m_states[idx].candidate);
    }

    // An empty candidate list after aggregation is unusual but not necessarily
    // an error -- the stream may have contained only usage or metadata frames.
    // We still return a valid response in that case.

    reset();
    return response;
}

// ---------------------------------------------------------------------------
// Reset all accumulated state
// ---------------------------------------------------------------------------

void StreamAggregator::reset()
{
    m_states.clear();
    m_totalUsage = UsageEntry{};
    m_envelope = SemanticEnvelope{};
    m_responseId.clear();
    m_modelUsed.clear();
    m_lastFailure = DomainFailure{};
    m_hasFailed = false;
}

// ---------------------------------------------------------------------------
// Apply a Delta frame: merge text segments, append non-text segments
// ---------------------------------------------------------------------------

void StreamAggregator::applyDelta(CandidateState& state, const StreamFrame& frame)
{
    Candidate& candidate = state.candidate;

    for (const Segment& deltaSeg : frame.deltaSegments) {
        bool merged = false;

        // For text segments, try to merge with the trailing text segment in the
        // candidate output to avoid fragmentation.
        if (deltaSeg.kind == SegmentKind::Text && !candidate.output.isEmpty()) {
            Segment& last = candidate.output.last();
            if (last.kind == SegmentKind::Text) {
                last.text.append(deltaSeg.text);
                merged = true;
            }
        }

        if (!merged) {
            candidate.output.append(deltaSeg);
        }
    }
}

// ---------------------------------------------------------------------------
// Apply an ActionDelta: accumulate tool call args by callId
// ---------------------------------------------------------------------------

void StreamAggregator::applyActionDelta(CandidateState& state,
                                         const ActionDelta& delta)
{
    if (delta.callId.isEmpty()) {
        return;
    }

    Candidate& candidate = state.candidate;

    if (state.actionByCallId.contains(delta.callId)) {
        // Append to existing tool call
        int idx = state.actionByCallId[delta.callId];
        if (idx >= 0 && idx < candidate.toolCalls.size()) {
            ActionCall& existing = candidate.toolCalls[idx];
            existing.args.append(delta.argsPatch);
            // Update name if the existing one is empty and the delta provides one
            if (existing.name.isEmpty() && !delta.name.isEmpty()) {
                existing.name = delta.name;
            }
        }
    } else {
        // Create a new ActionCall entry
        ActionCall call;
        call.callId = delta.callId;
        call.name = delta.name;
        call.args = delta.argsPatch;

        int newIndex = candidate.toolCalls.size();
        candidate.toolCalls.append(call);
        state.actionByCallId[delta.callId] = newIndex;
    }
}

// ---------------------------------------------------------------------------
// Accumulate usage tokens
// ---------------------------------------------------------------------------

void StreamAggregator::applyUsage(UsageEntry& total, const UsageEntry& delta)
{
    total.promptTokens += delta.promptTokens;
    total.completionTokens += delta.completionTokens;
    total.totalTokens += delta.totalTokens;
}
