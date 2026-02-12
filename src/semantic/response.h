#pragma once
#include "envelope.h"
#include "segment.h"
#include "action.h"
#include "extension.h"
#include "types.h"
#include <QList>
#include <QString>

struct UsageEntry {
    int promptTokens = 0;
    int completionTokens = 0;
    int totalTokens = 0;
};

struct Candidate {
    int index = 0;
    QString role;
    QList<Segment> output;
    QList<ActionCall> toolCalls;
    StopCause stopCause = StopCause::Completed;
};

struct SemanticResponse {
    SemanticEnvelope envelope;
    QString responseId;
    TaskKind kind = TaskKind::Conversation;
    QString modelUsed;
    QList<Candidate> candidates;
    UsageEntry usage;
    ExtensionBag extensions;
};
