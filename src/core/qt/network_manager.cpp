/**
 * @file network_manager.cpp
 * @brief 网络管理器实现文件
 * 
 * 本文件实现了 NetworkManager 类的所有方法，提供完整的 HTTPS 代理服务器功能。
 * 
 * @details
 * 实现模块：
 * 
 * 1. 静态方法
 *    - isPortInUse(): 端口占用检测
 * 
 * 2. 连接池实现
 *    - ConnectionPool 类的完整实现
 *    - 实例获取、归还、清空操作
 * 
 * 3. 服务器生命周期
 *    - start(): 启动服务器，配置 SSL，开始监听
 *    - stop(): 停止服务器，清理资源
 *    - configureSsl(): SSL 证书配置
 * 
 * 4. 连接处理
 *    - onNewConnection(): 新连接处理
 *    - onSocketReadyRead(): 数据接收处理
 *    - onSocketDisconnected(): 断开连接处理
 *    - onSslErrors(): SSL 错误处理
 * 
 * 5. 请求处理
 *    - parseHttpRequest(): HTTP 请求解析
 *    - handleRequest(): 请求路由分发
 *    - handleModelsRequest(): 模型列表请求
 *    - handleChatCompletionsRequest(): 聊天补全请求
 *    - handleUnknownRequest(): 未知请求处理
 * 
 * 6. 请求转发
 *    - forwardRequest(): 转发请求到上游
 *    - transformRequestBody(): 请求体转换
 *    - verifyAuth(): 认证验证
 * 
 * 7. 响应处理
 *    - handleStreamingResponse(): 流式响应处理
 *    - handleNonStreamingResponse(): 非流式响应处理
 *    - handleNonStreamToStreamResponse(): 非流式转流式
 *    - convertStreamToNonStream(): 流式转非流式
 *    - forwardErrorResponse(): 错误响应转发
 * 
 * 8. HTTP 响应发送
 *    - sendHttpResponse(): 发送完整响应
 *    - sendHttpResponseHeaders(): 发送响应头
 *    - sendSseChunk(): 发送 SSE 数据块
 * 
 * 9. 日志辅助
 *    - logDebug(): 调试日志
 *    - logRequestDebug(): 请求调试信息
 *    - logResponseDebug(): 响应调试信息
 *    - emitLog(): 发送日志
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "network_manager.h"
#include "fluxfix_wrapper.h"
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpServer>
#include <QDateTime>
#include <QUrl>
#include <QNetworkRequest>
#include <QSharedPointer>

// ============================================================================
// NetworkManager 静态方法实现
// ============================================================================

bool NetworkManager::isPortInUse(int port)
{
    QTcpServer testServer;
    bool available = testServer.listen(QHostAddress::Any, port);
    if (available) {
        testServer.close();
    }
    return !available;
}

// ============================================================================
// ConnectionPool 实现
// ============================================================================

ConnectionPool::ConnectionPool(int maxSize) : m_maxSize(maxSize) {}

ConnectionPool::~ConnectionPool() { clear(); }

QNetworkAccessManager* ConnectionPool::acquire(bool enableHttp2)
{
    Q_UNUSED(enableHttp2)
    QMutexLocker locker(&m_mutex);
    
    if (!m_pool.isEmpty()) {
        auto* manager = m_pool.dequeue();
        m_inUse.insert(manager);
        return manager;
    }
    
    auto* manager = new QNetworkAccessManager();
    m_inUse.insert(manager);
    return manager;
}

void ConnectionPool::release(QNetworkAccessManager* manager)
{
    if (!manager) return;
    
    QMutexLocker locker(&m_mutex);
    
    if (!m_inUse.contains(manager)) {
        delete manager;
        return;
    }
    
    m_inUse.remove(manager);
    
    if (m_pool.size() < m_maxSize) {
        m_pool.enqueue(manager);
    } else {
        delete manager;
    }
}

void ConnectionPool::clear()
{
    QMutexLocker locker(&m_mutex);
    
    while (!m_pool.isEmpty()) {
        delete m_pool.dequeue();
    }
    
    for (auto* manager : m_inUse) {
        delete manager;
    }
    m_inUse.clear();
}

int ConnectionPool::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_pool.size();
}

// ============================================================================
// NetworkManager 构造与析构
// ============================================================================

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_connectionPool(nullptr)
    , m_logCallback(nullptr)
    , m_statusCallback(nullptr)
{
}

NetworkManager::~NetworkManager()
{
    stop();
}

// ============================================================================
// 服务器生命周期
// ============================================================================

bool NetworkManager::start(const ProxyServerConfig& config)
{
    try {
        if (isRunning()) {
            emitLog("代理服务器已在运行", LogLevel::Warning);
            return false;
        }
        
        if (!config.isValid()) {
            emitLog("代理配置无效", LogLevel::Error);
            return false;
        }
        
        int proxyPort = config.getProxyPort();
        if (isPortInUse(proxyPort)) {
            emitLog("端口 " + std::to_string(proxyPort) + " 已被占用", LogLevel::Error);
            return false;
        }
        
        m_config = config;
        m_connectionPool = std::make_unique<ConnectionPool>(config.options.connectionPoolSize);
        
        emitLog("连接池已创建，大小: " + std::to_string(config.options.connectionPoolSize), LogLevel::Info);
        
        m_server = new QSslServer(this);
        
        if (!configureSsl()) {
            emitLog("SSL配置失败", LogLevel::Error);
            delete m_server;
            m_server = nullptr;
            m_connectionPool.reset();
            return false;
        }
        
        connect(m_server, &QSslServer::pendingConnectionAvailable,
                this, &NetworkManager::onNewConnection);
        
        if (!m_server->listen(QHostAddress::Any, proxyPort)) {
            emitLog("无法监听端口 " + std::to_string(proxyPort), LogLevel::Error);
            delete m_server;
            m_server = nullptr;
            m_connectionPool.reset();
            return false;
        }
    } catch (const std::exception& e) {
        emitLog(std::string("启动代理服务器时发生异常: ") + e.what(), LogLevel::Error);
        if (m_server) {
            delete m_server;
            m_server = nullptr;
        }
        m_connectionPool.reset();
        return false;
    } catch (...) {
        emitLog("启动代理服务器时发生未知异常", LogLevel::Error);
        if (m_server) {
            delete m_server;
            m_server = nullptr;
        }
        m_connectionPool.reset();
        return false;
    }
    
    emitLog("代理服务器已启动，监听端口 " + std::to_string(m_config.getProxyPort()), LogLevel::Info);
    
    if (m_statusCallback) m_statusCallback(true);
    emit statusChanged(true);
    
    return true;
}

void NetworkManager::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    
    if (m_connectionPool) {
        m_connectionPool->clear();
        m_connectionPool.reset();
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_pendingData.clear();
        m_replyToSocket.clear();
        m_streamingHeadersSent.clear();
        m_sseBuffer.clear();
        m_sseHandlers.clear();
    }
    
    emitLog("代理服务器已停止", LogLevel::Info);
    
    if (m_statusCallback) m_statusCallback(false);
    emit statusChanged(false);
}

bool NetworkManager::isRunning() const
{
    return m_server && m_server->isListening();
}

void NetworkManager::setLogCallback(LogCallback callback)
{
    m_logCallback = std::move(callback);
}

void NetworkManager::setStatusCallback(StatusCallback callback)
{
    m_statusCallback = std::move(callback);
}

bool NetworkManager::configureSsl()
{
    QFile certFile(QString::fromStdString(m_config.certPath));
    if (!certFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QSslCertificate certificate(&certFile, QSsl::Pem);
    certFile.close();
    
    if (certificate.isNull()) {
        return false;
    }
    
    QFile keyFile(QString::fromStdString(m_config.keyPath));
    if (!keyFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QSslKey privateKey(&keyFile, QSsl::Rsa, QSsl::Pem);
    keyFile.close();
    
    if (privateKey.isNull()) {
        return false;
    }
    
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setLocalCertificate(certificate);
    sslConfig.setPrivateKey(privateKey);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    
    m_server->setSslConfiguration(sslConfig);
    return true;
}

void NetworkManager::emitLog(const std::string& message, LogLevel level)
{
    if (m_logCallback) {
        m_logCallback(message, level);
    }
    emit logMessage(QString::fromStdString(message), level);
}


// ============================================================================
// 连接处理
// ============================================================================

void NetworkManager::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        auto* socket = qobject_cast<QSslSocket*>(m_server->nextPendingConnection());
        if (!socket) continue;
        
        connect(socket, &QSslSocket::readyRead, this, &NetworkManager::onSocketReadyRead);
        connect(socket, &QSslSocket::disconnected, this, &NetworkManager::onSocketDisconnected);
        connect(socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
                this, &NetworkManager::onSslErrors);
        
        {
            QMutexLocker locker(&m_mutex);
            m_pendingData[socket] = QByteArray();
        }
    }
}

void NetworkManager::onSocketReadyRead()
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (!socket) return;
    
    QByteArray data = socket->readAll();
    
    QMutexLocker locker(&m_mutex);
    m_pendingData[socket].append(data);
    
    if (m_pendingData[socket].size() > MAX_HEADER_SIZE + MAX_BODY_SIZE) {
        locker.unlock();
        socket->disconnectFromHost();
        return;
    }
    
    QByteArray& buffer = m_pendingData[socket];
    
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        if (buffer.size() > MAX_HEADER_SIZE) {
            locker.unlock();
            socket->disconnectFromHost();
        }
        return;
    }
    
    HttpRequest request = parseHttpRequest(buffer);
    
    QString contentLengthStr = request.getHeader("content-length");
    int contentLength = 0;
    
    if (!contentLengthStr.isEmpty()) {
        bool ok = false;
        contentLength = contentLengthStr.toInt(&ok);
        if (!ok || contentLength < 0 || contentLength > MAX_BODY_SIZE) {
            locker.unlock();
            socket->disconnectFromHost();
            return;
        }
    }
    
    int totalExpected = headerEnd + 4 + contentLength;
    if (buffer.size() < totalExpected) {
        return;
    }
    
    request.body = buffer.mid(headerEnd + 4, contentLength);
    buffer = buffer.mid(totalExpected);
    locker.unlock();
    
    handleRequest(socket, request);
}

void NetworkManager::onSocketDisconnected()
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (!socket) return;
    
    QList<QNetworkReply*> repliesToAbort;
    
    {
        QMutexLocker locker(&m_mutex);
        m_pendingData.remove(socket);
        
        QList<QNetworkReply*> repliesToRemove;
        for (auto it = m_replyToSocket.begin(); it != m_replyToSocket.end(); ++it) {
            if (it.value() == socket) {
                repliesToRemove.append(it.key());
            }
        }
        
        for (auto* reply : repliesToRemove) {
            m_replyToSocket.remove(reply);
            m_streamingHeadersSent.remove(reply);
            m_sseBuffer.remove(reply);
            
            // 删除 SSE 处理器
            m_sseHandlers.erase(reply);
            
            if (reply->isRunning()) {
                repliesToAbort.append(reply);
            }
        }
    }
    
    for (auto* reply : repliesToAbort) {
        reply->abort();
    }
    
    socket->deleteLater();
}

void NetworkManager::onSslErrors(const QList<QSslError>& errors)
{
    auto* socket = qobject_cast<QSslSocket*>(sender());
    if (socket) {
        Q_UNUSED(errors)
        socket->ignoreSslErrors();
    }
}

// ============================================================================
// 请求解析与处理
// ============================================================================

HttpRequest NetworkManager::parseHttpRequest(const QByteArray& data)
{
    HttpRequest request;
    QString dataStr = QString::fromUtf8(data);
    QStringList lines = dataStr.split("\r\n");
    
    if (lines.isEmpty()) return request;
    
    QStringList requestLine = lines[0].split(' ');
    if (requestLine.size() >= 3) {
        request.method = requestLine[0];
        request.path = requestLine[1];
        request.httpVersion = requestLine[2];
    }
    
    for (int i = 1; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.isEmpty()) break;
        
        int colonPos = line.indexOf(':');
        if (colonPos > 0) {
            QString key = line.left(colonPos).trimmed().toLower();
            QString value = line.mid(colonPos + 1).trimmed();
            request.headers[key] = value;
        }
    }
    
    return request;
}

void NetworkManager::handleRequest(QSslSocket* socket, const HttpRequest& request)
{
    logRequestDebug(request);
    
    QString host = request.getHeader("host");
    
    const ProxyConfigItem* matchedConfig = nullptr;
    for (const auto& config : m_config.proxyConfigs) {
        if (QString::fromStdString(config.localUrl) == host) {
            matchedConfig = &config;
            break;
        }
    }
    
    if (!matchedConfig) {
        handleUnknownRequest(socket, request);
        return;
    }
    
    QString authHeader = request.getHeader("authorization");
    QString apiKeyHeader = request.getHeader("x-api-key");
    
    if (!verifyAuth(authHeader, apiKeyHeader, matchedConfig->authKey)) {
        sendUnauthorizedResponse(socket);
        return;
    }
    
    if (request.path == "/v1/models" && request.method == "GET") {
        handleModelsRequest(socket, request, *matchedConfig);
    } else if ((request.path == "/v1/chat/completions" || request.path == "/v1/messages") 
               && request.method == "POST") {
        handleChatCompletionsRequest(socket, request, *matchedConfig);
    } else {
        handleUnknownRequest(socket, request);
    }
}

bool NetworkManager::verifyAuth(const QString& authHeader, const QString& apiKeyHeader, 
                               const std::string& expectedAuthKey)
{
    if (expectedAuthKey.empty()) {
        return true;
    }
    
    QString expected = QString::fromStdString(expectedAuthKey);
    
    if (authHeader == QStringLiteral("Bearer %1").arg(expected)) {
        return true;
    }
    
    if (apiKeyHeader == expected) {
        return true;
    }
    
    return false;
}

void NetworkManager::sendUnauthorizedResponse(QSslSocket* socket)
{
    QJsonObject errorObj;
    errorObj["error"] = QJsonObject{
        {"message", "Unauthorized"},
        {"type", "authentication_error"},
        {"code", "invalid_api_key"}
    };
    
    sendHttpResponse(socket, 401, "Unauthorized", 
                    QJsonDocument(errorObj).toJson(QJsonDocument::Compact));
}

void NetworkManager::handleModelsRequest(QSslSocket* socket, const HttpRequest& request, 
                                         const ProxyConfigItem& config)
{
    emitLog("处理 GET /v1/models 请求", LogLevel::Info);
    forwardRequest(socket, request, config);
}

void NetworkManager::handleChatCompletionsRequest(QSslSocket* socket, const HttpRequest& request, 
                                                  const ProxyConfigItem& config)
{
    emitLog("处理 POST " + request.path.toStdString() + " 请求", LogLevel::Info);
    forwardRequest(socket, request, config);
}

void NetworkManager::handleUnknownRequest(QSslSocket* socket, const HttpRequest& request)
{
    QJsonObject errorObj;
    errorObj["error"] = QJsonObject{
        {"message", QStringLiteral("Unknown endpoint: %1 %2").arg(request.method, request.path)},
        {"type", "invalid_request_error"},
        {"code", "unknown_endpoint"}
    };
    
    sendHttpResponse(socket, 404, "Not Found", 
                    QJsonDocument(errorObj).toJson(QJsonDocument::Compact));
}


// ============================================================================
// 请求转发
// ============================================================================

QByteArray NetworkManager::transformRequestBody(const QByteArray& body, const ProxyConfigItem& config)
{
    if (body.isEmpty()) return body;
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        return body;
    }
    
    QJsonObject obj = doc.object();
    
    if (obj.contains("model") && !config.mappedModelName.empty()) {
        obj["model"] = QString::fromStdString(config.mappedModelName);
    }
    
    bool originalStream = obj.value("stream").toBool(false);
    bool upstreamStream = originalStream;
    
    switch (m_config.options.upstreamStreamMode) {
        case StreamMode::ForceOn:  upstreamStream = true; break;
        case StreamMode::ForceOff: upstreamStream = false; break;
        default: break;
    }
    
    if (upstreamStream != originalStream) {
        obj["stream"] = upstreamStream;
    }
    
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

void NetworkManager::forwardRequest(QSslSocket* clientSocket, const HttpRequest& request, 
                                   const ProxyConfigItem& config)
{
    QString host = request.getHeader("host");
    QString targetUrl = QStringLiteral("https://%1%2").arg(host, request.path);
    
    QUrl mappedUrlObj(QString::fromStdString(config.mappedUrl));
    QString mappedHost = mappedUrlObj.host();
    if (mappedHost.isEmpty()) {
        mappedHost = QString::fromStdString(config.mappedUrl);
    }
    
    QString localUrl = QString::fromStdString(config.localUrl);
    if (!localUrl.isEmpty() && !mappedHost.isEmpty()) {
        targetUrl.replace(localUrl, mappedHost);
    }
    
    QUrl url(targetUrl);
    QNetworkRequest networkRequest(url);
    
    networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, 
                               m_config.options.enableHttp2);
    
    // 设置请求超时
    networkRequest.setTransferTimeout(m_config.options.requestTimeout);
    
    if (m_config.options.disableSslStrict) {
        QSslConfiguration sslConfig = networkRequest.sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        networkRequest.setSslConfiguration(sslConfig);
    }
    
    QString apiKey = QString::fromStdString(config.apiKey);
    
    for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
        QString key = it.key();
        QString value = it.value();
        
        if (key == "content-length") continue;
        
        if (!localUrl.isEmpty() && !mappedHost.isEmpty()) {
            value.replace(localUrl, mappedHost);
        }
        
        if (!apiKey.isEmpty()) {
            if (key == "authorization") {
                value = QStringLiteral("Bearer %1").arg(apiKey);
            } else if (key == "x-api-key") {
                value = apiKey;
            }
        }
        
        networkRequest.setRawHeader(key.toUtf8(), value.toUtf8());
    }
    
    if (request.path == "/v1/models" && !apiKey.isEmpty() && !request.hasHeader("authorization")) {
        networkRequest.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey).toUtf8());
    }
    
    if (!request.hasHeader("content-type")) {
        networkRequest.setRawHeader("Content-Type", "application/json");
    }
    
    QByteArray transformedBody = transformRequestBody(request.body, config);
    networkRequest.setHeader(QNetworkRequest::ContentLengthHeader, transformedBody.size());
    
    QJsonDocument originalDoc = QJsonDocument::fromJson(request.body);
    bool clientRequestedStream = originalDoc.object().value("stream").toBool(false);
    
    QJsonDocument transformedDoc = QJsonDocument::fromJson(transformedBody);
    bool upstreamStream = transformedDoc.object().value("stream").toBool(false);
    
    auto* manager = m_connectionPool->acquire(m_config.options.enableHttp2);
    
    QNetworkReply* reply = nullptr;
    if (request.method == "GET") {
        reply = manager->get(networkRequest);
    } else if (request.method == "POST") {
        reply = manager->post(networkRequest, transformedBody);
    } else if (request.method == "PUT") {
        reply = manager->put(networkRequest, transformedBody);
    } else if (request.method == "DELETE") {
        reply = manager->deleteResource(networkRequest);
    } else {
        reply = manager->post(networkRequest, transformedBody);
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_replyToSocket[reply] = clientSocket;
    }
    
    bool downstreamStream = clientRequestedStream;
    switch (m_config.options.downstreamStreamMode) {
        case StreamMode::ForceOn:  downstreamStream = true; break;
        case StreamMode::ForceOff: downstreamStream = false; break;
        default: break;
    }
    
    if (upstreamStream) {
        if (downstreamStream) {
            {
                QMutexLocker locker(&m_mutex);
                m_sseHandlers[reply] = std::make_unique<FluxFixSseHandler>();
            }
            
            connect(reply, &QNetworkReply::readyRead, this, [this, clientSocket, reply]() {
                handleStreamingResponse(clientSocket, reply);
            });
        } else {
            // 使用 QSharedPointer 避免内存泄漏
            auto buffer = QSharedPointer<QByteArray>::create();
            connect(reply, &QNetworkReply::readyRead, this, [buffer, reply]() {
                buffer->append(reply->readAll());
            });
            connect(reply, &QNetworkReply::finished, this, [this, clientSocket, reply, buffer]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray fullResponse = convertStreamToNonStream(*buffer);
                    sendHttpResponse(clientSocket, 200, "OK", fullResponse);
                }
            });
        }
    }

    connect(reply, &QNetworkReply::finished, this, 
            [this, clientSocket, reply, manager, upstreamStream, downstreamStream]() {
        
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray remainingData = reply->readAll();
        
        if (reply->error() != QNetworkReply::NoError) {
            QMutexLocker locker(&m_mutex);
            bool headersSent = m_streamingHeadersSent.contains(reply);
            locker.unlock();
            
            if (headersSent && clientSocket && 
                clientSocket->state() == QAbstractSocket::ConnectedState) {
                clientSocket->write("0\r\n\r\n");
                clientSocket->flush();
            } else {
                forwardErrorResponse(clientSocket, reply);
            }
        } else if (upstreamStream && downstreamStream) {
            if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
                QMutexLocker locker(&m_mutex);
                
                if (!remainingData.isEmpty()) {
                    m_sseBuffer[reply].append(remainingData);
                }
                
                std::vector<char> flushData;
                auto handlerIt = m_sseHandlers.find(reply);
                if (handlerIt != m_sseHandlers.end() && handlerIt->second) {
                    flushData = handlerIt->second->flush();
                }
                
                QByteArray& buffer = m_sseBuffer[reply];
                locker.unlock();
                
                if (!buffer.isEmpty()) {
                    QString chunkSize = QString::number(buffer.size(), 16);
                    clientSocket->write(chunkSize.toUtf8() + "\r\n");
                    clientSocket->write(buffer);
                    clientSocket->write("\r\n");
                    clientSocket->flush();
                }
                
                if (!flushData.empty()) {
                    QByteArray flushBytes(flushData.data(), static_cast<int>(flushData.size()));
                    QString chunkSize = QString::number(flushBytes.size(), 16);
                    clientSocket->write(chunkSize.toUtf8() + "\r\n");
                    clientSocket->write(flushBytes);
                    clientSocket->write("\r\n");
                    clientSocket->flush();
                }
                
                clientSocket->write("0\r\n\r\n");
                clientSocket->flush();
            }
        } else if (!upstreamStream) {
            if (downstreamStream) {
                handleNonStreamToStreamResponse(clientSocket, reply);
            } else {
                if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
                    sendHttpResponse(clientSocket, statusCode, "OK", remainingData);
                }
            }
        }
        
        {
            QMutexLocker locker(&m_mutex);
            m_replyToSocket.remove(reply);
            m_streamingHeadersSent.remove(reply);
            m_sseBuffer.remove(reply);
            m_sseHandlers.erase(reply);
        }
        
        reply->deleteLater();
        m_connectionPool->release(manager);
    });
}


// ============================================================================
// 响应处理
// ============================================================================

/**
 * @brief 处理流式响应
 * 
 * 接收上游服务器的流式响应数据，通过 SSE 处理器处理后转发给客户端。
 * 使用 chunked transfer encoding 发送数据。
 * 
 * @param clientSocket 客户端 SSL 套接字
 * @param reply 上游服务器的网络响应
 */
void NetworkManager::handleStreamingResponse(QSslSocket* clientSocket, QNetworkReply* reply)
{
    if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QByteArray data = reply->readAll();
    if (data.isEmpty()) return;
    
    QMutexLocker locker(&m_mutex);
    
    // 首次响应时发送 HTTP 头
    if (!m_streamingHeadersSent.contains(reply)) {
        m_streamingHeadersSent.insert(reply);
        locker.unlock();
        
        QMap<QString, QString> headers;
        headers["Content-Type"] = "text/event-stream";
        headers["Cache-Control"] = "no-cache";
        headers["Connection"] = "keep-alive";
        headers["Transfer-Encoding"] = "chunked";
        
        sendHttpResponseHeaders(clientSocket, 200, "OK", headers);
        locker.relock();
    }
    
    // 获取 SSE 处理器
    auto handlerIt = m_sseHandlers.find(reply);
    FluxFixSseHandler* handler = (handlerIt != m_sseHandlers.end()) ? handlerIt->second.get() : nullptr;
    if (!handler) {
        locker.unlock();
        return;
    }
    
    // 解析 SSE 事件并转发
    m_sseBuffer[reply].append(data);
    QByteArray& buffer = m_sseBuffer[reply];
    
    while (true) {
        int eventEnd = buffer.indexOf("\n\n");
        if (eventEnd == -1) break;
        
        QByteArray eventData = buffer.left(eventEnd + 2);
        buffer = buffer.mid(eventEnd + 2);
        
        // 添加到聚合器
        handler->addEvent(eventData);
        
        // 转发原始事件
        locker.unlock();
        sendSseChunk(clientSocket, eventData);
        locker.relock();
    }
}

/**
 * @brief 处理非流式响应
 * 
 * 接收上游服务器的完整响应后，使用 FluxFix 库进行标准化处理，
 * 然后转发给客户端。
 * 
 * @param clientSocket 客户端 SSL 套接字
 * @param reply 上游服务器的网络响应
 */
void NetworkManager::handleNonStreamingResponse(QSslSocket* clientSocket, QNetworkReply* reply)
{
    if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray body = reply->readAll();
    
    logResponseDebug(statusCode, body);
    
    // 使用 FluxFix 进行响应标准化处理
    QByteArray normalizedBody = normalizeNonStreamResponse(body);
    
    sendHttpResponse(clientSocket, statusCode, "OK", normalizedBody);
}

/**
 * @brief 将非流式响应转换为流式响应
 * 
 * 使用 FluxFixSplitter 将完整响应拆分为 SSE 事件流发送给客户端。
 * 
 * @param clientSocket 客户端 SSL 套接字
 * @param reply 上游服务器的网络响应
 */
void NetworkManager::handleNonStreamToStreamResponse(QSslSocket* clientSocket, QNetworkReply* reply)
{
    if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QByteArray body = reply->readAll();
    
    // 解析 JSON 响应
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    
    QString id = "chatcmpl-converted";
    QString model = "unknown";
    QString content;
    
    if (parseError.error == QJsonParseError::NoError) {
        QJsonObject obj = doc.object();
        id = obj.value("id").toString(id);
        model = obj.value("model").toString(model);
        
        QJsonArray choices = obj.value("choices").toArray();
        if (!choices.isEmpty()) {
            content = choices[0].toObject()
                .value("message").toObject()
                .value("content").toString();
        }
    }
    
    // 发送 SSE 响应头
    QMap<QString, QString> headers;
    headers["Content-Type"] = "text/event-stream";
    headers["Cache-Control"] = "no-cache";
    headers["Connection"] = "keep-alive";
    headers["Transfer-Encoding"] = "chunked";
    
    sendHttpResponseHeaders(clientSocket, 200, "OK", headers);
    
    // 使用 FluxFixSplitter 拆分为流式数据块
    FluxFixSplitter splitter;
    size_t chunkSize = m_config.options.chunkSize > 0 ? m_config.options.chunkSize : 20;
    splitter.setChunkSize(chunkSize);
    
    auto chunks = splitter.splitToByteArrays(id, model, content);
    
    for (const auto& chunk : chunks) {
        sendSseChunk(clientSocket, chunk);
    }
    
    clientSocket->write("0\r\n\r\n");
    clientSocket->flush();
}

/**
 * @brief 转发错误响应
 * 
 * 将上游服务器的错误响应转发给客户端。
 * 
 * @param clientSocket 客户端 SSL 套接字
 * @param reply 上游服务器的网络响应
 */
void NetworkManager::forwardErrorResponse(QSslSocket* clientSocket, QNetworkReply* reply)
{
    if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode == 0) {
        statusCode = 502; // Bad Gateway
    }
    
    QByteArray errorBody = reply->readAll();
    
    if (errorBody.isEmpty()) {
        QJsonObject errorObj;
        errorObj["error"] = QJsonObject{
            {"message", reply->errorString()},
            {"type", "upstream_error"},
            {"code", QString::number(reply->error())}
        };
        errorBody = QJsonDocument(errorObj).toJson(QJsonDocument::Compact);
    }
    
    emitLog("上游错误: " + reply->errorString().toStdString(), LogLevel::Error);
    sendHttpResponse(clientSocket, statusCode, "Error", errorBody);
}

/**
 * @brief 将流式响应转换为非流式响应
 * 
 * 使用 FluxFixAggregator 将 SSE 流数据聚合为完整的 JSON 响应。
 * 
 * @param streamData 完整的 SSE 流数据
 * @return 合并后的 JSON 响应
 */
QByteArray NetworkManager::convertStreamToNonStream(const QByteArray& streamData)
{
    FluxFixAggregator aggregator;
    aggregator.addBytes(streamData);
    
    auto response = aggregator.finalize();
    if (!response) {
        // 返回空响应
        QJsonObject obj;
        obj["id"] = "chatcmpl-error";
        obj["object"] = "chat.completion";
        obj["model"] = "unknown";
        obj["choices"] = QJsonArray({QJsonObject{
            {"index", 0},
            {"message", QJsonObject{{"role", "assistant"}, {"content", ""}}},
            {"finish_reason", "stop"}
        }});
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }
    
    QJsonObject obj;
    obj["id"] = QString::fromStdString(response->id);
    obj["object"] = "chat.completion";
    obj["model"] = QString::fromStdString(response->model);
    obj["choices"] = QJsonArray({QJsonObject{
        {"index", 0},
        {"message", QJsonObject{
            {"role", "assistant"}, 
            {"content", QString::fromStdString(response->content)}
        }},
        {"finish_reason", QString::fromStdString(
            response->finishReason.empty() ? "stop" : response->finishReason)}
    }});
    
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

/**
 * @brief 标准化非流式响应
 * 
 * 对于聊天补全响应，直接返回；其他响应透传。
 * 
 * @param responseData 原始响应数据
 * @return 响应数据
 */
QByteArray NetworkManager::normalizeNonStreamResponse(const QByteArray& responseData)
{
    // 直接返回，FluxFix 库已在上游处理
    return responseData;
}

// ============================================================================
// HTTP 响应发送
// ============================================================================

/**
 * @brief 发送完整的 HTTP 响应
 * 
 * 发送包含状态行、头部和正文的完整 HTTP 响应。
 * 
 * @param socket 客户端套接字
 * @param statusCode HTTP 状态码
 * @param statusText 状态文本
 * @param body 响应正文
 * @param contentType 内容类型，默认为 application/json
 */
void NetworkManager::sendHttpResponse(QSslSocket* socket, int statusCode, 
                                      const QString& statusText, const QByteArray& body,
                                      const QString& contentType)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QByteArray response;
    response.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
    response.append(QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("Connection: close\r\n");
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("\r\n");
    response.append(body);
    
    socket->write(response);
    socket->flush();
}

/**
 * @brief 发送 HTTP 响应头
 * 
 * 仅发送 HTTP 响应的状态行和头部，用于流式响应。
 * 
 * @param socket 客户端套接字
 * @param statusCode HTTP 状态码
 * @param statusText 状态文本
 * @param headers 响应头映射
 */
void NetworkManager::sendHttpResponseHeaders(QSslSocket* socket, int statusCode, 
                                             const QString& statusText,
                                             const QMap<QString, QString>& headers)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QByteArray response;
    response.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        response.append(QStringLiteral("%1: %2\r\n").arg(it.key(), it.value()).toUtf8());
    }
    
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("\r\n");
    
    socket->write(response);
    socket->flush();
}

/**
 * @brief 发送 SSE 数据块
 * 
 * 使用 chunked transfer encoding 发送 SSE 数据块。
 * 
 * @param socket 客户端套接字
 * @param data SSE 事件数据
 */
void NetworkManager::sendSseChunk(QSslSocket* socket, const QByteArray& data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    if (data.isEmpty()) return;
    
    // Chunked encoding: 大小（十六进制）+ CRLF + 数据 + CRLF
    QString chunkSize = QString::number(data.size(), 16);
    socket->write(chunkSize.toUtf8() + "\r\n");
    socket->write(data);
    socket->write("\r\n");
    socket->flush();
}

// ============================================================================
// 日志辅助方法
// ============================================================================

/**
 * @brief 输出调试日志
 * 
 * 仅在调试模式下输出日志。
 * 
 * @param message 日志消息
 */
void NetworkManager::logDebug(const QString& message)
{
    if (m_config.options.debugMode) {
        emitLog(message.toStdString(), LogLevel::Debug);
    }
}

/**
 * @brief 输出请求调试信息
 * 
 * 在调试模式下输出 HTTP 请求的详细信息。
 * 
 * @param request HTTP 请求对象
 */
void NetworkManager::logRequestDebug(const HttpRequest& request)
{
    if (!m_config.options.debugMode) return;
    
    QString debugInfo = QStringLiteral(">>> 请求: %1 %2").arg(request.method, request.path);
    emitLog(debugInfo.toStdString(), LogLevel::Debug);
    
    // 输出请求头
    for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
        QString headerInfo = QStringLiteral("    %1: %2").arg(it.key(), it.value());
        emitLog(headerInfo.toStdString(), LogLevel::Debug);
    }
    
    // 输出请求体（截断）
    if (!request.body.isEmpty()) {
        QString bodyStr = QString::fromUtf8(request.body);
        if (bodyStr.length() > MAX_DEBUG_OUTPUT_LENGTH) {
            bodyStr = bodyStr.left(MAX_DEBUG_OUTPUT_LENGTH) + "... [截断]";
        }
        emitLog(("    Body: " + bodyStr).toStdString(), LogLevel::Debug);
    }
}

/**
 * @brief 输出响应调试信息
 * 
 * 在调试模式下输出 HTTP 响应的详细信息。
 * 
 * @param statusCode HTTP 状态码
 * @param body 响应正文
 */
void NetworkManager::logResponseDebug(int statusCode, const QByteArray& body)
{
    if (!m_config.options.debugMode) return;
    
    QString debugInfo = QStringLiteral("<<< 响应: %1").arg(statusCode);
    emitLog(debugInfo.toStdString(), LogLevel::Debug);
    
    if (!body.isEmpty()) {
        QString bodyStr = QString::fromUtf8(body);
        if (bodyStr.length() > MAX_DEBUG_OUTPUT_LENGTH) {
            bodyStr = bodyStr.left(MAX_DEBUG_OUTPUT_LENGTH) + "... [截断]";
        }
        emitLog(("    Body: " + bodyStr).toStdString(), LogLevel::Debug);
    }
}
