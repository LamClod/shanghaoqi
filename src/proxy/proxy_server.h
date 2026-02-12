#pragma once
#include "connection_pool.h"
#include "request_router.h"
#include "config/config_types.h"
#include <QObject>
#include <QSslServer>
#include <QSslSocket>
#include <QMap>

class Pipeline;
class PipelineStreamSession;

class ProxyServer : public QObject {
    Q_OBJECT
public:
    explicit ProxyServer(QObject* parent = nullptr);
    ~ProxyServer() override;

    bool start(const ProxyConfig& config);
    void stop();
    bool isRunning() const;
    void setPipeline(Pipeline* pipeline);
    ConnectionPool& connectionPool() { return m_connectionPool; }
    static bool isPortInUse(int port);

signals:
    void statusChanged(bool running);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    struct HttpRequest {
        QString method, path, httpVersion;
        QMap<QString, QString> headers;
        QByteArray body;
        bool complete = false;
        int contentLength = 0;
    };

    HttpRequest parseHttpRequest(const QByteArray& data);
    void handleRequest(QSslSocket* socket, const HttpRequest& request);
    bool handleModelsRequest(QSslSocket* socket, const HttpRequest& request);
    void sendHttpResponse(QSslSocket* socket, int status,
                          const QByteArray& body,
                          const QString& contentType = QStringLiteral("application/json"));
    void sendStreamResponse(QSslSocket* socket, PipelineStreamSession* session);
    QMap<QString, QString> buildMetadata(const HttpRequest& request,
                                         const Route& route) const;

    QSslServer* m_server = nullptr;
    ConnectionPool m_connectionPool;
    RequestRouter m_router;
    Pipeline* m_pipeline = nullptr;
    ProxyConfig m_config;
    QMap<QSslSocket*, QByteArray> m_pendingData;
    QMap<QSslSocket*, PipelineStreamSession*> m_activeSessions;
};
