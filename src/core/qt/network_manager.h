/**
 * @file network_manager.h
 * @brief 网络管理器头文件
 * 
 * 本文件定义了网络代理服务器的核心实现类，提供 HTTPS 代理功能。
 * 
 * @details
 * 功能概述：
 * 
 * 网络管理器是应用程序的核心组件，负责：
 * - 本地 HTTPS 代理服务器的启动和停止
 * - SSL/TLS 加密连接处理
 * - HTTP 请求解析和转发
 * - 流式响应（SSE）处理
 * - 连接池管理
 * - 模型名称映射
 * - 认证密钥转换
 * 
 * 架构设计：
 * 
 * 1. 代理服务器
 *    - 使用 QSslServer 监听 HTTPS 连接
 *    - 支持自签名证书
 *    - 处理客户端请求并转发到上游服务器
 * 
 * 2. 连接池
 *    - 复用 QNetworkAccessManager 实例
 *    - 减少连接建立开销
 *    - 可配置池大小
 * 
 * 3. 请求处理流程
 *    - 解析 HTTP 请求
 *    - 验证认证信息
 *    - 转换请求参数（模型名称、API 密钥等）
 *    - 转发到上游服务器
 *    - 处理响应并返回给客户端
 * 
 * 4. 流式响应处理
 *    - 支持 SSE（Server-Sent Events）
 *    - 支持流式到非流式转换
 *    - 支持非流式到流式转换
 * 
 * 支持的 API 端点：
 * 
 * - GET /v1/models: 获取模型列表
 * - POST /v1/chat/completions: 聊天补全（OpenAI 格式）
 * - POST /v1/messages: 消息接口（Anthropic 格式）
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see INetworkManager 网络管理器接口
 * @see ProxyServerConfig 代理服务器配置
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "../interfaces/i_network_manager.h"
#include "fluxfix_wrapper.h"

#include <QObject>
#include <QSslServer>
#include <QSslSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QQueue>
#include <memory>
#include <unordered_map>

/**
 * @brief HTTP 请求信息结构体
 * 
 * 封装解析后的 HTTP 请求信息，供内部处理使用。
 * 
 * @details
 * 结构体成员：
 * 
 * - method: HTTP 方法（GET、POST、PUT、DELETE 等）
 * - path: 请求路径（如 /v1/chat/completions）
 * - httpVersion: HTTP 版本（如 HTTP/1.1）
 * - headers: 请求头映射，键名统一转为小写
 * - body: 请求体原始数据
 * 
 * 辅助方法：
 * 
 * - getHeader(): 获取指定请求头的值
 * - hasHeader(): 检查是否存在指定请求头
 * - shouldKeepAlive(): 判断是否应保持连接
 * 
 * @note 请求头的键名在存储时会转换为小写，查询时也应使用小写
 */
struct HttpRequest {
    QString method;
    QString path;
    QString httpVersion;
    QMap<QString, QString> headers;
    QByteArray body;
    
    QString getHeader(const QString& name) const {
        return headers.value(name.toLower());
    }
    
    bool hasHeader(const QString& name) const {
        return headers.contains(name.toLower());
    }
    
    bool shouldKeepAlive() const {
        QString connection = getHeader("connection").toLower();
        if (httpVersion.contains("1.1")) {
            return connection != "close";
        }
        return connection == "keep-alive";
    }
};

/**
 * @brief 连接池管理器
 * 
 * 管理 QNetworkAccessManager 实例的对象池，提高网络请求性能。
 * 
 * @details
 * 设计目的：
 * 
 * 创建 QNetworkAccessManager 实例有一定开销，频繁创建销毁会影响性能。
 * 连接池通过复用实例来减少这种开销，同时限制最大实例数量避免资源耗尽。
 * 
 * 工作原理：
 * 
 * 1. acquire() 获取实例
 *    - 优先从池中取出空闲实例
 *    - 池为空时创建新实例
 *    - 记录实例为"使用中"状态
 * 
 * 2. release() 归还实例
 *    - 如果池未满，将实例放回池中
 *    - 如果池已满，直接销毁实例
 *    - 清除实例的"使用中"状态
 * 
 * 3. clear() 清空池
 *    - 销毁所有空闲实例
 *    - 销毁所有使用中的实例
 * 
 * 线程安全：
 * 
 * 所有操作都通过互斥锁保护，支持多线程并发访问。
 * 
 * @note 禁止拷贝和赋值操作
 */
class ConnectionPool {
public:
    explicit ConnectionPool(int maxSize = 10);
    ~ConnectionPool();
    
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    
    QNetworkAccessManager* acquire(bool enableHttp2 = true);
    void release(QNetworkAccessManager* manager);
    void clear();
    int size() const;
    
private:
    QQueue<QNetworkAccessManager*> m_pool;
    QSet<QNetworkAccessManager*> m_inUse;
    int m_maxSize;
    mutable QMutex m_mutex;
};

/**
 * @brief 网络管理器实现类
 * 
 * 实现 INetworkManager 接口，提供完整的 HTTPS 代理服务器功能。
 * 
 * @details
 * 类设计说明：
 * 
 * 本类同时继承 QObject 和 INetworkManager，结合了 Qt 的信号槽机制
 * 和纯 C++ 接口的设计优势。
 * 
 * 主要功能：
 * 
 * 1. 代理服务器生命周期管理
 *    - start(): 启动代理服务器
 *    - stop(): 停止代理服务器
 *    - isRunning(): 查询运行状态
 * 
 * 2. 请求处理
 *    - 解析 HTTP 请求
 *    - 验证认证信息
 *    - 转发请求到上游服务器
 *    - 处理响应并返回
 * 
 * 3. 流式响应处理
 *    - 支持 SSE 流式响应
 *    - 支持流模式转换
 * 
 * 4. 回调和信号
 *    - 日志回调
 *    - 状态变化回调
 *    - Qt 信号通知
 * 
 * 静态工具方法：
 * 
 * - isPortInUse(): 检查端口是否被占用
 * 
 * @note 禁止拷贝和赋值操作
 * @note 启动前需要配置有效的证书路径
 * 
 * @see INetworkManager 网络管理器接口
 * @see ProxyServerConfig 代理服务器配置
 */
class NetworkManager : public QObject, public INetworkManager {
    Q_OBJECT
    
public:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager() override;
    
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // ========== INetworkManager 接口实现 ==========
    
    bool start(const ProxyServerConfig& config) override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] const ProxyServerConfig& config() const override { return m_config; }
    
    void setLogCallback(LogCallback callback) override;
    void setStatusCallback(StatusCallback callback) override;
    
    // ========== 静态工具方法（Qt 实现特有）==========
    
    /**
     * @brief 检查端口是否被占用
     * 
     * 使用 Qt 的 QTcpServer 检测端口占用情况。
     * 
     * @param port 端口号
     * @return true 端口已被占用
     */
    static bool isPortInUse(int port);

signals:
    /**
     * @brief Qt 信号：日志消息
     * 
     * 当有新的日志消息时发出此信号。
     * 
     * @param message 日志消息内容
     * @param level 日志级别
     */
    void logMessage(const QString& message, LogLevel level);
    
    /**
     * @brief Qt 信号：状态变化
     * 
     * 当代理服务器启动或停止时发出此信号。
     * 
     * @param running true 表示正在运行，false 表示已停止
     */
    void statusChanged(bool running);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSslErrors(const QList<QSslError>& errors);

private:
    // 请求处理
    HttpRequest parseHttpRequest(const QByteArray& data);
    void handleRequest(QSslSocket* socket, const HttpRequest& request);
    void handleModelsRequest(QSslSocket* socket, const HttpRequest& request, 
                            const ProxyConfigItem& config);
    void handleChatCompletionsRequest(QSslSocket* socket, const HttpRequest& request, 
                                      const ProxyConfigItem& config);
    void handleUnknownRequest(QSslSocket* socket, const HttpRequest& request);
    
    // 认证
    bool verifyAuth(const QString& authHeader, const QString& apiKeyHeader, 
                   const std::string& expectedAuthKey);
    void sendUnauthorizedResponse(QSslSocket* socket);
    
    // 请求转发
    void forwardRequest(QSslSocket* clientSocket, const HttpRequest& request, 
                       const ProxyConfigItem& config);
    QByteArray transformRequestBody(const QByteArray& body, const ProxyConfigItem& config);
    
    // 响应处理
    void handleStreamingResponse(QSslSocket* clientSocket, QNetworkReply* reply);
    void handleNonStreamingResponse(QSslSocket* clientSocket, QNetworkReply* reply);
    void handleNonStreamToStreamResponse(QSslSocket* clientSocket, QNetworkReply* reply);
    void forwardErrorResponse(QSslSocket* clientSocket, QNetworkReply* reply);
    QByteArray convertStreamToNonStream(const QByteArray& streamData);
    QByteArray normalizeNonStreamResponse(const QByteArray& responseData);
    
    // HTTP响应
    void sendHttpResponse(QSslSocket* socket, int statusCode, const QString& statusText,
                         const QByteArray& body, const QString& contentType = "application/json");
    void sendHttpResponseHeaders(QSslSocket* socket, int statusCode, const QString& statusText,
                                const QMap<QString, QString>& headers);
    void sendSseChunk(QSslSocket* socket, const QByteArray& data);
    
    // 日志
    void logDebug(const QString& message);
    void logRequestDebug(const HttpRequest& request);
    void logResponseDebug(int statusCode, const QByteArray& body);
    void emitLog(const std::string& message, LogLevel level);
    
    // SSL
    bool configureSsl();

    // 成员变量
    QSslServer* m_server;
    std::unique_ptr<ConnectionPool> m_connectionPool;
    ProxyServerConfig m_config;
    
    QMap<QSslSocket*, QByteArray> m_pendingData;
    QMap<QNetworkReply*, QSslSocket*> m_replyToSocket;
    QSet<QNetworkReply*> m_streamingHeadersSent;
    QMap<QNetworkReply*, QByteArray> m_sseBuffer;
    std::unordered_map<QNetworkReply*, std::unique_ptr<FluxFixSseHandler>> m_sseHandlers;
    
    mutable QMutex m_mutex;
    LogCallback m_logCallback;
    StatusCallback m_statusCallback;
};

#endif // NETWORK_MANAGER_H
