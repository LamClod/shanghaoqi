#pragma once
#include "types.h"
#include <QMap>
#include <QString>

struct CapabilityProfile {
    QString adapterId;
    QString modelPattern;
    QMap<TaskKind, bool> taskSupport;
};
