/**
 * @file i_config_manager.h
 * @brief 配置管理器接口定义
 * 
 * 本文件定义了跨平台的配置管理抽象接口。
 * 
 * @details
 * 设计理念：
 * 
 * 本接口采用纯 C++ 设计，不依赖任何外部框架。
 * 各平台需要实现此接口以提供配置存储和加密功能。
 * 
 * 功能模块：
 * 
 * 1. 配置加载和保存
 *    - 从文件加载配置
 *    - 保存配置到文件
 *    - 支持异步保存
 * 
 * 2. 敏感数据加密存储
 *    - API 密钥等敏感信息加密存储
 *    - 加载时自动解密
 *    - 保存时自动加密
 * 
 * 3. 配置项的增删改查
 *    - 添加新配置
 *    - 更新现有配置
 *    - 删除配置
 *    - 查询配置
 * 
 * 4. 配置变更通知
 *    - 回调函数通知
 *    - 便于 UI 层响应变化
 * 
 * 平台实现差异：
 * 
 * | 平台    | 加密方式      |
 * |---------|--------------|
 * | Windows | DPAPI        |
 * | macOS   | Keychain     |
 * | Linux   | libsecret    |
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef I_CONFIG_MANAGER_H
#define I_CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "../../core/operation_result.h"
#include "../../core/interfaces/i_log_manager.h"
#include "../../core/interfaces/i_network_manager.h"

/**
 * @brief 应用配置结构体
 * 
 * 包含所有代理配置项的集合。
 * 使用 ProxyConfigItem（来自 i_network_manager.h）作为配置项类型。
 */
struct AppConfig {
    std::vector<ProxyConfigItem> proxyConfigs;  ///< 代理配置列表
    
    bool operator==(const AppConfig& other) const {
        return proxyConfigs == other.proxyConfigs;
    }
    
    bool operator!=(const AppConfig& other) const {
        return !(*this == other);
    }
    
    /**
     * @brief 获取所有有效的配置项
     * 
     * @return 有效配置项列表
     */
    std::vector<ProxyConfigItem> getValidConfigs() const {
        std::vector<ProxyConfigItem> valid;
        for (const auto& cfg : proxyConfigs) {
            if (cfg.isValid()) valid.push_back(cfg);
        }
        return valid;
    }
    
    bool isEmpty() const { return proxyConfigs.empty(); }
    size_t count() const { return proxyConfigs.size(); }
};

/**
 * @brief 配置变更回调类型
 */
using ConfigChangedCallback = std::function<void()>;

/**
 * @brief 配置管理器接口
 * 
 * 纯抽象接口，定义配置存储和加密的标准操作。
 */
class IConfigManager {
public:
    virtual ~IConfigManager() = default;
    
    // 禁止拷贝
    IConfigManager(const IConfigManager&) = delete;
    IConfigManager& operator=(const IConfigManager&) = delete;
    
    // ========== 配置加载和保存 ==========
    
    /**
     * @brief 加载配置
     * 
     * 从配置文件加载配置，自动解密敏感字段。
     * 
     * @return 操作结果
     */
    virtual OperationResult load() = 0;
    
    /**
     * @brief 保存配置
     * 
     * 将配置保存到文件，自动加密敏感字段。
     * 
     * @return 操作结果
     */
    virtual OperationResult save() = 0;
    
    /**
     * @brief 异步保存配置
     * 
     * 在后台线程保存配置，不阻塞调用线程。
     */
    virtual void saveAsync() = 0;
    
    // ========== 配置访问 ==========
    
    /**
     * @brief 获取当前配置
     * 
     * @return 当前配置的常量引用
     */
    [[nodiscard]] virtual const AppConfig& getConfig() const = 0;
    
    /**
     * @brief 设置配置
     * 
     * @param config 新的配置
     */
    virtual void setConfig(const AppConfig& config) = 0;
    
    // ========== 配置项操作 ==========
    
    /**
     * @brief 添加代理配置
     * 
     * @param config 要添加的配置项
     * @return 操作结果
     */
    virtual OperationResult addProxyConfig(const ProxyConfigItem& config) = 0;
    
    /**
     * @brief 更新代理配置
     * 
     * @param index 配置项索引
     * @param config 新的配置项
     * @return 操作结果
     */
    virtual OperationResult updateProxyConfig(size_t index, const ProxyConfigItem& config) = 0;
    
    /**
     * @brief 删除代理配置
     * 
     * @param index 配置项索引
     * @return 操作结果
     */
    virtual OperationResult removeProxyConfig(size_t index) = 0;
    
    /**
     * @brief 获取指定索引的配置项
     * 
     * @param index 配置项索引
     * @return 配置项指针，如果索引无效则返回 nullptr
     */
    [[nodiscard]] virtual const ProxyConfigItem* getProxyConfig(size_t index) const = 0;
    
    // ========== 属性访问 ==========
    
    /**
     * @brief 获取配置文件路径
     * 
     * @return 配置文件的完整路径
     */
    [[nodiscard]] virtual std::string configPath() const = 0;
    
    /**
     * @brief 获取配置项数量
     * 
     * @return 配置项数量
     */
    [[nodiscard]] virtual size_t configCount() const = 0;
    
    // ========== 回调设置 ==========
    
    /**
     * @brief 设置日志回调
     * 
     * @param callback 日志回调函数
     */
    virtual void setLogCallback(LogCallback callback) = 0;
    
    /**
     * @brief 设置配置变更回调
     * 
     * @param callback 配置变更回调函数
     */
    virtual void setConfigChangedCallback(ConfigChangedCallback callback) = 0;
    
    // ========== 静态方法 ==========
    
    /**
     * @brief 验证代理配置
     * 
     * 检查配置项是否包含所有必填字段。
     * 
     * @param config 要验证的配置项
     * @return 操作结果
     */
    static OperationResult validateProxyConfig(const ProxyConfigItem& config) {
        std::string missingFields;
        
        auto trim = [](const std::string& s) {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };
        
        if (trim(config.name).empty()) {
            if (!missingFields.empty()) missingFields += ", ";
            missingFields += "配置名称";
        }
        if (trim(config.localUrl).empty()) {
            if (!missingFields.empty()) missingFields += ", ";
            missingFields += "本地URL";
        }
        if (trim(config.mappedUrl).empty()) {
            if (!missingFields.empty()) missingFields += ", ";
            missingFields += "映射URL";
        }
        
        if (!missingFields.empty()) {
            return OperationResult::failure(
                "缺少必填字段: " + missingFields,
                ErrorCode::ConfigInvalid
            );
        }
        return OperationResult::success();
    }

protected:
    IConfigManager() = default;
};

/// 配置管理器智能指针类型
using IConfigManagerPtr = std::unique_ptr<IConfigManager>;

#endif // I_CONFIG_MANAGER_H
