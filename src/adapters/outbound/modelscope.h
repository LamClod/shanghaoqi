#pragma once
#include "openai.h"

class ModelScopeOutbound : public OpenAIOutbound {
public:
    ModelScopeOutbound() = default;
    ~ModelScopeOutbound() override = default;

    QString adapterId() const override;
    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
};
