#pragma once
#include "openai.h"

class AntigravityOutbound : public OpenAIOutbound {
public:
    AntigravityOutbound() = default;
    ~AntigravityOutbound() override = default;

    QString adapterId() const override;
    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
};
