#pragma once
#include <QString>

struct SemanticEnvelope {
    QString requestId;
    QString traceId;
    qint64 timestampMs = 0;
};
