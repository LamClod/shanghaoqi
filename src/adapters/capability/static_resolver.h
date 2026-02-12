#pragma once
#include "semantic/ports.h"

class StaticCapabilityResolver : public ICapabilityResolver {
public:
    Result<CapabilityProfile> resolve(const TargetSpec& target) override;
};
