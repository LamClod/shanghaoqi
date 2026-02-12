#pragma once
#include "openai.h"

class BailianOutbound : public OpenAIOutbound {
public:
    BailianOutbound() = default;
    ~BailianOutbound() override = default;

    QString adapterId() const override;
    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
};
