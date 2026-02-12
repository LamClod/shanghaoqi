#include "stream_mode_middleware.h"
#include "semantic/types.h"

Result<SemanticRequest> StreamModeMiddleware::onRequest(SemanticRequest request) {
    const bool clientRequestedStream =
        request.metadata.value(QStringLiteral("stream")) == QStringLiteral("true") ||
        request.metadata.value(QStringLiteral("stream.upstream")) == QStringLiteral("true");

    // Apply upstream stream mode override from constructor config
    if (m_upstream == StreamMode::ForceOn)
        request.metadata[QStringLiteral("stream.upstream")] = QStringLiteral("true");
    else if (m_upstream == StreamMode::ForceOff)
        request.metadata[QStringLiteral("stream.upstream")] = QStringLiteral("false");
    else
        request.metadata[QStringLiteral("stream.upstream")] =
            clientRequestedStream ? QStringLiteral("true") : QStringLiteral("false");

    // Apply downstream stream mode
    if (m_downstream == StreamMode::ForceOn)
        request.metadata[QStringLiteral("stream.downstream")] = QStringLiteral("true");
    else if (m_downstream == StreamMode::ForceOff)
        request.metadata[QStringLiteral("stream.downstream")] = QStringLiteral("false");
    else
        request.metadata[QStringLiteral("stream.downstream")] =
            clientRequestedStream ? QStringLiteral("true") : QStringLiteral("false");

    return request;
}
