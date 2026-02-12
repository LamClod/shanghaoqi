#pragma once
#include "openai.h"

class CodexOutbound : public OpenAIOutbound {
public:
    CodexOutbound() = default;
    ~CodexOutbound() override = default;

    QString adapterId() const override;
    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
};
