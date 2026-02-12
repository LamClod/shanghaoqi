#pragma once
#include <QSslSocket>
#include <QByteArray>

class SseWriter {
public:
    static void writeStreamHeader(QSslSocket* socket);
    static void sendChunk(QSslSocket* socket, const QByteArray& sseData);
    static void sendDone(QSslSocket* socket);
    static void sendTerminator(QSslSocket* socket);

private:
    static QByteArray wrapChunked(const QByteArray& data);
};
