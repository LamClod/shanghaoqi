#pragma once
#include "semantic/ports.h"

class IPipelineMiddleware {
public:
    virtual ~IPipelineMiddleware() = default;
    virtual QString name() const = 0;

    virtual Result<SemanticRequest> onRequest(SemanticRequest request) {
        return request;
    }
    virtual Result<SemanticResponse> onResponse(SemanticResponse response) {
        return response;
    }
    virtual Result<StreamFrame> onFrame(StreamFrame frame) {
        return frame;
    }
};
