#include "validate.h"

namespace Validate {

VoidResult request(const SemanticRequest& req) {
    if (req.messages.isEmpty())
        return std::unexpected(DomainFailure::invalidInput(
            "empty_messages", "请求消息列表不能为空"));
    if (req.target.logicalModel.isEmpty())
        return std::unexpected(DomainFailure::invalidInput(
            "empty_model", "目标模型不能为空"));
    return {};
}

VoidResult response(const SemanticResponse& resp) {
    if (resp.candidates.isEmpty())
        return std::unexpected(DomainFailure::internal(
            "响应缺少 candidates"));
    return {};
}

VoidResult frame(const StreamFrame& f) {
    if (f.type == FrameType::Failed && f.failure.message.isEmpty())
        return std::unexpected(DomainFailure::internal(
            "Failed 帧缺少错误信息"));
    return {};
}

}
