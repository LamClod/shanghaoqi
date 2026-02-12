#include "model_mapping_middleware.h"

Result<SemanticRequest> ModelMappingMiddleware::onRequest(SemanticRequest request) {
    // Forward mapping: if the client sends the local model ID,
    // replace it with the mapped (provider) model ID.
    if (!m_localModelId.isEmpty() && !m_mappedModelId.isEmpty()) {
        if (request.target.logicalModel == m_localModelId ||
            request.target.logicalModel.isEmpty()) {
            request.metadata[QStringLiteral("original_model")] = request.target.logicalModel;
            request.target.logicalModel = m_mappedModelId;
        }
    }

    // Metadata-level override always wins.
    const QString metaMapped = request.metadata.value(QStringLiteral("mapped_model_id"));
    if (!metaMapped.isEmpty()) {
        request.metadata[QStringLiteral("original_model")] = request.target.logicalModel;
        request.target.logicalModel = metaMapped;
    }

    return request;
}

Result<SemanticResponse> ModelMappingMiddleware::onResponse(SemanticResponse response) {
    if (!m_localModelId.isEmpty() && !m_mappedModelId.isEmpty()) {
        if (response.modelUsed == m_mappedModelId) {
            response.modelUsed = m_localModelId;
        }
    }
    return response;
}

Result<StreamFrame> ModelMappingMiddleware::onFrame(StreamFrame frame) {
    return frame;
}