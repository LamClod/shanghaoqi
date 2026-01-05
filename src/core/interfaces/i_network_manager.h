/**
 * @file i_network_manager.h
 * @brief 网络管理器接口定义
 * 
 * 本文件定义了网络代理服务的纯抽象接口，为不同平台的网络实现提供统一规范。
 * 
 * @details
 * 设计理念：
 * 
 * 本接口采用纯 C++ 设计，不依赖任何外部框架，确保接口的可移植性。
 * 定义了代理服务器的核心功能，包括服务器控制、配置管理和回调机制。
 * 
 * 功能模块：
 * 
 * 1. 本地 HTTPS 代理服务器
 *    - 监听指定端口
 *    - 处理 SSL/TLS 加密连接
 *    - 支持自签名证书
 * 
 * 2. 请求拦截和转发
 *    - 拦截客户端请求
 *    - 转发到上游服务器
 *    - 返回响应给客户端
 * 
 * 3. 流式响应处理
 *    - 支持 SSE（Server-Sent Events）
 *    - 支持流模式转换
 * 
 * 4. 模型名称映射
 *    - 本地模型名到远程模型名的映射
 *    - 支持多个代理配置
 * 
 * 5. 认证转换
 *    - 本地认证密钥验证
 *    - 远程 API 密钥注入
 * 
 * 配置结构：
 * 
 * - ProxyConfigItem: 单个代理配置项
 * - RuntimeOptions: 运行时选项
 * - ProxyServerConfig: 完整的服务器配置
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef I_NETWORK_MANAGER_H
#define I_NETWORK_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "i_log_manager.h"

/**
 * @brief 流模式枚举
 */
enum class StreamMode {
    FollowClient = 0,   ///< 跟随客户端
    ForceOn = 1,        ///< 强制开启
    ForceOff = 2        ///< 强制关闭
};

/**
 * @brief 代理配置项
 */
struct ProxyConfigItem {
    std::string name;              ///< 配置名称
    std::string localUrl;          ///< 本地URL
    std::string mappedUrl;         ///< 映射URL
    std::string localModelName;    ///< 本地模型名称
    std::string mappedModelName;   ///< 映射模型名称
    std::string authKey;           ///< 认证密钥
    std::string apiKey;            ///< API密钥
    
    bool isValid() const {
        return !name.empty() && !localUrl.empty() && !mappedUrl.empty();
    }
    
    bool operator==(const ProxyConfigItem& other) const {
        return name == other.name &&
               localUrl == other.localUrl &&
               mappedUrl == other.mappedUrl &&
               localModelName == other.localModelName &&
               mappedModelName == other.mappedModelName &&
               authKey == other.authKey &&
               apiKey == other.apiKey;
    }
};

/**
 * @brief 运行时选项
 */
struct RuntimeOptions {
    bool debugMode = false;
    bool disableSslStrict = false;
    bool enableHttp2 = true;
    bool enableConnectionPool = true;
    StreamMode upstreamStreamMode = StreamMode::FollowClient;
    StreamMode downstreamStreamMode = StreamMode::FollowClient;
    int proxyPort = 443;
    int connectionPoolSize = 10;
    int requestTimeout = 120000;      ///< 请求超时（毫秒）
    int connectionTimeout = 30000;    ///< 连接超时（毫秒）
    int chunkSize = 20;               ///< 流式响应分块大小（字符数）
    
    bool operator==(const RuntimeOptions& other) const {
        return debugMode == other.debugMode &&
               disableSslStrict == other.disableSslStrict &&
               enableHttp2 == other.enableHttp2 &&
               enableConnectionPool == other.enableConnectionPool &&
               upstreamStreamMode == other.upstreamStreamMode &&
               downstreamStreamMode == other.downstreamStreamMode &&
               proxyPort == other.proxyPort &&
               connectionPoolSize == other.connectionPoolSize &&
               requestTimeout == other.requestTimeout &&
               connectionTimeout == other.connectionTimeout &&
               chunkSize == other.chunkSize;
    }
};

/**
 * @brief 代理服务器配置
 */
struct ProxyServerConfig {
    std::vector<ProxyConfigItem> proxyConfigs;  ///< 代理配置列表
    RuntimeOptions options;                      ///< 运行时选项
    std::string certPath;                        ///< 服务器证书路径
    std::string keyPath;                         ///< 服务器私钥路径
    
    bool isValid() const {
        return !proxyConfigs.empty() &&
               !certPath.empty() &&
               !keyPath.empty() &&
               options.proxyPort > 0 &&
               options.proxyPort <= 65535;
    }
    
    int getProxyPort() const {
        return options.proxyPort;
    }
};

/**
 * @brief 状态变化回调
 */
using StatusCallback = std::function<void(bool running)>;

/**
 * @brief 网络管理器接口
 * 
 * 纯抽象接口，定义网络代理服务的标准操作。
 */
class INetworkManager {
public:
    virtual ~INetworkManager() = default;
    
    // 禁止拷贝
    INetworkManager(const INetworkManager&) = delete;
    INetworkManager& operator=(const INetworkManager&) = delete;
    
    // ========== 服务器控制 ==========
    
    /**
     * @brief 启动代理服务器
     * 
     * @param config 代理配置
     * @return true 启动成功
     */
    virtual bool start(const ProxyServerConfig& config) = 0;
    
    /**
     * @brief 停止代理服务器
     */
    virtual void stop() = 0;
    
    /**
     * @brief 检查服务器是否正在运行
     */
    [[nodiscard]] virtual bool isRunning() const = 0;
    
    // ========== 配置访问 ==========
    
    /**
     * @brief 获取当前配置
     */
    [[nodiscard]] virtual const ProxyServerConfig& config() const = 0;
    
    // ========== 回调设置 ==========
    
    /**
     * @brief 设置日志回调
     */
    virtual void setLogCallback(LogCallback callback) = 0;
    
    /**
     * @brief 设置状态变化回调
     */
    virtual void setStatusCallback(StatusCallback callback) = 0;
    
    // ========== 常量 ==========
    
    static constexpr int MAX_HEADER_SIZE = 16384;       ///< 最大请求头大小 (16KB)
    static constexpr int MAX_BODY_SIZE = 10485760;      ///< 最大请求体大小 (10MB)
    static constexpr int MAX_DEBUG_OUTPUT_LENGTH = 2000; ///< 最大调试输出长度

protected:
    INetworkManager() = default;
};

/// 网络管理器智能指针类型
using INetworkManagerPtr = std::unique_ptr<INetworkManager>;

#endif // I_NETWORK_MANAGER_H
