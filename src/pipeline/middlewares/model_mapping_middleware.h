#pragma once
#include "pipeline/middleware.h"

class ModelMappingMiddleware : public IPipelineMiddleware {
public:
    ModelMappingMiddleware(const QString& localModelId = {},
                           const QString& mappedModelId = {})
        : m_localModelId(localModelId), m_mappedModelId(mappedModelId) {}
    QString name() const override { return "model_mapping"; }
    Result<SemanticRequest> onRequest(SemanticRequest request) override;
    Result<SemanticResponse> onResponse(SemanticResponse response) override;
    Result<StreamFrame> onFrame(StreamFrame frame) override;

private:
    QString m_localModelId;
    QString m_mappedModelId;
};