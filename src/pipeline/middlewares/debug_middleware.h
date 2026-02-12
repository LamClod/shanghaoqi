#pragma once
#include "pipeline/middleware.h"

class DebugMiddleware : public IPipelineMiddleware {
public:
    explicit DebugMiddleware(bool enabled = false) : m_enabled(enabled) {}
    QString name() const override { return "debug"; }
    Result<SemanticRequest> onRequest(SemanticRequest request) override;
    Result<SemanticResponse> onResponse(SemanticResponse response) override;
    Result<StreamFrame> onFrame(StreamFrame frame) override;

private:
    bool m_enabled;
};
