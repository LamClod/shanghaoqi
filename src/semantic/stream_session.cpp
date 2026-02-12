#include "stream_session.h"
#include "core/log_manager.h"

StreamSession::StreamSession(QNetworkReply* reply,
                             IOutboundAdapter* outbound,
                             const QString& adapterHint,
                             QObject* parent)
    : QObject(parent)
    , m_reply(reply)
    , m_outbound(outbound)
    , m_adapterHint(adapterHint)
{
    Q_ASSERT(m_reply);
    Q_ASSERT(m_outbound);

    // Take ownership of the reply so it is cleaned up with this session
    m_reply->setParent(this);

    connect(m_reply, &QNetworkReply::readyRead,
            this, &StreamSession::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,
            this, &StreamSession::onReplyFinished);
    connect(m_reply, &QNetworkReply::errorOccurred,
            this, &StreamSession::onReplyError);
}

StreamSession::~StreamSession()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void StreamSession::abort()
{
    if (m_reply) {
        m_reply->abort();
    }
}

void StreamSession::onReadyRead()
{
    if (!m_reply) return;
    m_sseBuffer.append(m_reply->readAll());
    parseSseEvents();
}

void StreamSession::onReplyFinished()
{
    if (m_finished) return;

    // Process any remaining data in the buffer by appending a trailing
    // double-newline to flush the last event block if it was incomplete.
    if (!m_sseBuffer.isEmpty()) {
        m_sseBuffer.append("\n\n");
        parseSseEvents();
    }

    m_finished = true;
    emit finished();
}

void StreamSession::onReplyError(QNetworkReply::NetworkError code)
{
    if (m_finished) return;

    DomainFailure failure;

    switch (code) {
    case QNetworkReply::NoError:
        return;

    case QNetworkReply::OperationCanceledError:
        failure = DomainFailure::timeout(
            QStringLiteral("Stream operation was cancelled"));
        break;

    case QNetworkReply::TimeoutError:
        failure = DomainFailure::timeout(
            QStringLiteral("Stream connection timed out"));
        break;

    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        failure = DomainFailure::unavailable(
            QStringLiteral("Network error: %1").arg(
                m_reply ? m_reply->errorString() : QStringLiteral("unknown")));
        break;

    case QNetworkReply::AuthenticationRequiredError:
        failure = DomainFailure::unauthorized(
            QStringLiteral("Authentication required for stream"));
        break;

    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
        failure = DomainFailure::unauthorized(
            QStringLiteral("Access denied: %1").arg(
                m_reply ? m_reply->errorString() : QStringLiteral("unknown")));
        break;

    default: {
        // For HTTP-level errors, delegate to the outbound adapter's mapFailure
        // so provider-specific error messages are preserved.
        if (m_reply && m_outbound) {
            int httpStatus = m_reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray body = m_reply->readAll();
            failure = m_outbound->mapFailure(httpStatus, body);
        } else {
            failure = DomainFailure::internal(
                QStringLiteral("Stream network error (%1): %2")
                    .arg(static_cast<int>(code))
                    .arg(m_reply ? m_reply->errorString()
                                : QStringLiteral("unknown")));
        }
        break;
    }
    }

    LOG_ERROR(QStringLiteral("StreamSession error [%1]: %2")
                  .arg(failure.code, failure.message));

    m_finished = true;
    emit error(failure);
}

bool StreamSession::parseSseEvents()
{
    bool processed = false;

    while (true) {
        // SSE events are delimited by double newlines.
        // We check for "\r\n\r\n" first (longer delimiter) to avoid partial
        // matches, then fall back to "\n\n".
        int delimPos = -1;
        int delimLen = 0;

        int crlfPos = m_sseBuffer.indexOf("\r\n\r\n");
        int lfPos = m_sseBuffer.indexOf("\n\n");

        if (crlfPos >= 0 && (lfPos < 0 || crlfPos <= lfPos)) {
            delimPos = crlfPos;
            delimLen = 4;
        } else if (lfPos >= 0) {
            delimPos = lfPos;
            delimLen = 2;
        }

        if (delimPos < 0) {
            // No complete event block yet; wait for more data.
            break;
        }

        QByteArray block = m_sseBuffer.left(delimPos);
        m_sseBuffer.remove(0, delimPos + delimLen);

        // Reset pending state for this event block
        m_pendingEventType.clear();
        m_pendingDataLines.clear();

        // Parse each line in the block
        QList<QByteArray> lines = block.split('\n');
        for (const QByteArray& rawLine : lines) {
            // Strip any trailing \r left over from \r\n splitting
            QByteArray line = rawLine;
            if (line.endsWith('\r')) {
                line.chop(1);
            }

            if (line.isEmpty()) {
                // Empty line within a block; skip.
                continue;
            }

            if (line.startsWith(':')) {
                // SSE comment (heartbeat/keepalive). Skip.
                continue;
            }

            if (line.startsWith("event:")) {
                QByteArray value = line.mid(6).trimmed();
                m_pendingEventType = QString::fromUtf8(value);
            } else if (line.startsWith("data:")) {
                QByteArray value = line.mid(5).trimmed();
                m_pendingDataLines.append(value);
            } else if (line.startsWith("id:")) {
                // SSE id field; we do not track reconnection IDs.
            } else if (line.startsWith("retry:")) {
                // SSE retry field; we do not handle auto-reconnect.
            }
            // Unknown field names are ignored per SSE spec.
        }

        // Flush the accumulated event from this block
        flushPendingEvent();
        processed = true;
    }

    return processed;
}

void StreamSession::flushPendingEvent()
{
    if (m_pendingDataLines.isEmpty()) {
        // No data lines means nothing to emit (event-only lines are meaningless
        // without data in our protocol).
        m_pendingEventType.clear();
        return;
    }

    // Join all data lines with newline (SSE spec: multiple "data:" lines are
    // concatenated with U+000A between them).
    QByteArray data;
    for (int i = 0; i < m_pendingDataLines.size(); ++i) {
        if (i > 0) data.append('\n');
        data.append(m_pendingDataLines.at(i));
    }

    QString eventType = m_pendingEventType;
    m_pendingEventType.clear();
    m_pendingDataLines.clear();

    // Check for the SSE stream termination sentinel
    if (data == "[DONE]") {
        if (!m_finished) {
            m_finished = true;
            emit finished();
        }
        return;
    }

    // Skip empty data payloads
    if (data.isEmpty()) {
        return;
    }

    if (!m_outbound) {
        return;
    }

    // Build a ProviderChunk and delegate parsing to the outbound adapter
    ProviderChunk chunk;
    chunk.type = eventType;
    chunk.data = data;
    chunk.adapterHint = m_adapterHint;

    Result<StreamFrame> result = m_outbound->parseChunk(chunk);

    if (result.has_value()) {
        emit frameReady(result.value());
    } else {
        LOG_WARNING(QStringLiteral("StreamSession: chunk parse error: %1")
                        .arg(result.error().message));
        emit error(result.error());
    }
}
