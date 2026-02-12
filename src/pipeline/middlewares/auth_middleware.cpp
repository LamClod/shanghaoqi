#include "auth_middleware.h"

Result<SemanticRequest> AuthMiddleware::onRequest(SemanticRequest request) {
    if (m_authKey.isEmpty()) {
        return request;
    }

    QString clientKey = request.metadata.value(QStringLiteral("auth_key"));

    // Strip "Bearer " prefix if present
    if (clientKey.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive)) {
        clientKey = clientKey.mid(7);
    }

    if (clientKey != m_authKey) {
        return std::unexpected(DomainFailure::unauthorized(
            QStringLiteral("Invalid or missing authentication key")));
    }

    return request;
}
