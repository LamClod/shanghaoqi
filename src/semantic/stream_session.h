#pragma once
#include "ports.h"
#include <QObject>
#include <QNetworkReply>

class StreamSession : public QObject {
    Q_OBJECT
public:
    explicit StreamSession(QNetworkReply* reply,
                           IOutboundAdapter* outbound,
                           const QString& adapterHint,
                           QObject* parent = nullptr);
    ~StreamSession() override;

    void abort();

signals:
    void frameReady(const StreamFrame& frame);
    void finished();
    void error(const DomainFailure& failure);

private slots:
    void onReadyRead();
    void onReplyFinished();
    void onReplyError(QNetworkReply::NetworkError code);

private:
    QNetworkReply* m_reply;
    IOutboundAdapter* m_outbound;
    QByteArray m_sseBuffer;
    QString m_pendingEventType;
    QList<QByteArray> m_pendingDataLines;
    bool m_finished = false;
    QString m_adapterHint;

    bool parseSseEvents();
    void flushPendingEvent();
};
