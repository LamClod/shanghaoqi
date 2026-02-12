#pragma once
#include "request.h"
#include "response.h"
#include "frame.h"
#include "ports.h"

namespace Validate {
    VoidResult request(const SemanticRequest& req);
    VoidResult response(const SemanticResponse& resp);
    VoidResult frame(const StreamFrame& frame);
}
