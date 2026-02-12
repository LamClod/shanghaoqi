#pragma once
#include "envelope.h"
#include "segment.h"
#include "action.h"
#include "constraints.h"
#include "target.h"
#include "extension.h"
#include "types.h"
#include <QList>
#include <QMap>
#include <QString>

struct InteractionItem {
    QString role;
    QList<Segment> content;
    QList<ActionCall> toolCalls;
    QString toolCallId;
};

struct SemanticRequest {
    SemanticEnvelope envelope;
    TaskKind kind = TaskKind::Conversation;
    TargetSpec target;
    QList<InteractionItem> messages;
    ConstraintSet constraints;
    QList<ActionSpec> tools;
    QMap<QString, QString> metadata;
    ExtensionBag extensions;
};
