#pragma once
#include "openai.h"

class ZaiOutbound : public OpenAIOutbound {
public:
    ZaiOutbound() = default;
    ~ZaiOutbound() override = default;

    QString adapterId() const override;
    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;
};
