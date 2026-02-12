#include "debug_middleware.h"
#include "core/log_manager.h"

Result<SemanticRequest> DebugMiddleware::onRequest(SemanticRequest request) {
    if (m_enabled) {
        LOG_DEBUG(QStringLiteral("[Debug] Request: model=%1, messages=%2, target=%3")
            .arg(request.target.logicalModel)
            .arg(request.messages.size())
            .arg(request.metadata.value(QStringLiteral("provider_base_url"))));
    }
    return request;
}

Result<SemanticResponse> DebugMiddleware::onResponse(SemanticResponse response) {
    if (m_enabled) {
        LOG_DEBUG(QStringLiteral("[Debug] Response: model=%1, candidates=%2, tokens=%3")
            .arg(response.modelUsed)
            .arg(response.candidates.size())
            .arg(response.usage.totalTokens));
    }
    return response;
}

Result<StreamFrame> DebugMiddleware::onFrame(StreamFrame frame) {
    if (m_enabled) {
        LOG_DEBUG(QStringLiteral("[Debug] Frame: type=%1, final=%2")
            .arg(static_cast<int>(frame.type))
            .arg(frame.isFinal));
    }
    return frame;
}
