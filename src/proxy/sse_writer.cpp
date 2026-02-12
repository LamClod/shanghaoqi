#include "sse_writer.h"
#include "core/log_manager.h"

void SseWriter::writeStreamHeader(QSslSocket* socket)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QStringLiteral("SseWriter: cannot write stream header, socket not connected"));
        return;
    }

    const QByteArray header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";

    socket->write(header);
    socket->flush();
}

QByteArray SseWriter::wrapChunked(const QByteArray& data)
{
    // HTTP/1.1 chunked transfer encoding:
    //   <hex-length>\r\n
    //   <data>\r\n
    QByteArray chunk;
    chunk.append(QByteArray::number(data.size(), 16));
    chunk.append("\r\n");
    chunk.append(data);
    chunk.append("\r\n");
    return chunk;
}

void SseWriter::sendChunk(QSslSocket* socket, const QByteArray& sseData)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QStringLiteral("SseWriter: cannot send chunk, socket not connected"));
        return;
    }

    QByteArray sseFrame;
    const bool alreadySse =
        sseData.startsWith("event:") ||
        sseData.startsWith("data:") ||
        sseData.startsWith("id:") ||
        sseData.startsWith("retry:") ||
        sseData.startsWith(":");

    if (alreadySse) {
        sseFrame = sseData;
    } else {
        sseFrame.append("data: ");
        sseFrame.append(sseData);
        sseFrame.append("\n\n");
    }

    socket->write(wrapChunked(sseFrame));
    socket->flush();
}

void SseWriter::sendDone(QSslSocket* socket)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QStringLiteral("SseWriter: cannot send done, socket not connected"));
        return;
    }

    QByteArray sseFrame = "data: [DONE]\n\n";
    socket->write(wrapChunked(sseFrame));
    socket->flush();
}

void SseWriter::sendTerminator(QSslSocket* socket)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        LOG_WARNING(QStringLiteral("SseWriter: cannot send terminator, socket not connected"));
        return;
    }

    // The zero-length chunk signals end of chunked transfer
    socket->write("0\r\n\r\n");
    socket->flush();
}
