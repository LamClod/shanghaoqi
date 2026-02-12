#pragma once
#include <QString>
#include <QStringList>
#include <optional>

struct ConstraintSet {
    std::optional<double> temperature;
    std::optional<double> topP;
    std::optional<int> maxTokens;
    std::optional<int> maxCompletionTokens;
    std::optional<int> seed;
    std::optional<double> frequencyPenalty;
    std::optional<double> presencePenalty;
    QStringList stopSequences;
};
