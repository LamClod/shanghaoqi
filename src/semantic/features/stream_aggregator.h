#pragma once
#include "semantic/response.h"
#include "semantic/frame.h"
#include "semantic/failure.h"
#include "semantic/ports.h"
#include <QMap>
#include <QList>

class StreamAggregator {
public:
    // Convenience: aggregate a complete list of frames in one call
    Result<SemanticResponse> aggregate(const QList<StreamFrame>& frames);

    // Incremental API
    void addFrame(const StreamFrame& frame);
    Result<SemanticResponse> finalize();
    void reset();

private:
    struct CandidateState {
        Candidate candidate;
        QMap<QString, int> actionByCallId;  // callId -> index in toolCalls
    };

    QMap<int, CandidateState> m_states;     // candidateIndex -> state
    UsageEntry m_totalUsage;
    SemanticEnvelope m_envelope;
    QString m_responseId;
    QString m_modelUsed;
    DomainFailure m_lastFailure;
    bool m_hasFailed = false;

    void applyDelta(CandidateState& state, const StreamFrame& frame);
    void applyActionDelta(CandidateState& state, const ActionDelta& delta);
    void applyUsage(UsageEntry& total, const UsageEntry& delta);
};
