#include "static_resolver.h"

Result<CapabilityProfile> StaticCapabilityResolver::resolve(const TargetSpec& target) {
    CapabilityProfile profile;
    profile.adapterId = "static";
    profile.modelPattern = target.logicalModel;
    profile.taskSupport = {
        {TaskKind::Conversation, true},
        {TaskKind::Embedding, true},
        {TaskKind::Ranking, true},
        {TaskKind::ImageGeneration, false}
    };
    return profile;
}
