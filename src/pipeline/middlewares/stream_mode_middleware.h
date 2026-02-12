#pragma once
#include "pipeline/middleware.h"

class StreamModeMiddleware : public IPipelineMiddleware {
public:
    StreamModeMiddleware(StreamMode upstream = StreamMode::FollowClient,
                         StreamMode downstream = StreamMode::FollowClient)
        : m_upstream(upstream), m_downstream(downstream) {}
    QString name() const override { return "stream_mode"; }
    Result<SemanticRequest> onRequest(SemanticRequest request) override;

private:
    StreamMode m_upstream;
    StreamMode m_downstream;
};
