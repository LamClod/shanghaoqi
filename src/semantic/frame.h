#pragma once
#include "envelope.h"
#include "segment.h"
#include "action.h"
#include "extension.h"
#include "failure.h"
#include "response.h"
#include "types.h"
#include <QList>

struct StreamFrame {
    SemanticEnvelope envelope;
    FrameType type = FrameType::Delta;
    int candidateIndex = 0;
    QList<Segment> deltaSegments;
    ActionDelta actionDelta;
    UsageEntry usageDelta;
    DomainFailure failure;
    bool isFinal = false;
    ExtensionBag extensions;
};
