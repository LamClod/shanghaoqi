#pragma once
#include <QString>
#include <QStringList>

struct FallbackPolicy {
    int maxAttempts = 1;
    bool allowDowngrade = false;
};

struct TargetSpec {
    QString logicalModel;
    QStringList preferredProfiles;
    FallbackPolicy fallback;
};
