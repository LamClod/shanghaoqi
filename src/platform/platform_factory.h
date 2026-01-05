/**
 * @file platform_factory.h
 * @brief 平台服务抽象工厂头文件
 * 
 * 本文件定义了跨平台服务的抽象工厂接口和工厂创建方法。
 * 
 * @details
 * 设计模式：
 * 
 * 本模块采用抽象工厂模式，统一创建一组平台相关的服务实例。
 * 每个支持的平台（Windows/macOS/Linux）都有对应的具体工厂实现。
 * 
 * 设计原则：
 * 
 * 1. 抽象工厂模式
 *    - IPlatformFactory 定义抽象工厂接口
 *    - 各平台实现具体工厂类
 *    - 创建一组相关的平台服务
 * 
 * 2. 依赖倒置原则
 *    - 高层模块依赖抽象接口
 *    - 不直接依赖具体实现
 *    - 便于测试和扩展
 * 
 * 3. 接口隔离
 *    - 纯 C++ 接口，零框架依赖
 *    - 接口简洁，职责单一
 * 
 * 服务类型：
 * 
 * - IConfigManager: 配置管理器
 * - ICertManager: 证书管理器
 * - IPrivilegeManager: 权限管理器
 * - IHostsManager: Hosts 文件管理器
 * 
 * 使用方式：
 * @code
 * // 创建平台工厂
 * auto factory = PlatformFactory::create();
 * 
 * // 创建各种服务
 * auto configManager = factory->createConfigManager("/path/to/config.yaml");
 * auto certManager = factory->createCertManager("/path/to/data");
 * auto privilegeManager = factory->createPrivilegeManager();
 * auto hostsManager = factory->createHostsManager("/path/to/backup");
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef PLATFORM_FACTORY_H
#define PLATFORM_FACTORY_H

#include <memory>
#include <string>

#include "interfaces/i_config_manager.h"
#include "interfaces/i_cert_manager.h"
#include "interfaces/i_privilege_manager.h"
#include "interfaces/i_hosts_manager.h"

/**
 * @brief 平台服务抽象工厂接口
 * 
 * 每个支持的平台（Windows/macOS/Linux）都有对应的具体工厂实现。
 */
class IPlatformFactory {
public:
    virtual ~IPlatformFactory() = default;
    
    // 禁止拷贝
    IPlatformFactory(const IPlatformFactory&) = delete;
    IPlatformFactory& operator=(const IPlatformFactory&) = delete;
    
    // ========== 服务创建方法 ==========
    
    /**
     * @brief 创建配置管理器
     * 
     * @param configPath 配置文件路径
     * @return 平台特定的配置管理器实例
     */
    virtual IConfigManagerPtr createConfigManager(const std::string& configPath) = 0;
    
    /**
     * @brief 创建证书管理器
     * 
     * @param dataDir 证书文件存储目录
     * @return 平台特定的证书管理器实例
     */
    virtual ICertManagerPtr createCertManager(const std::string& dataDir) = 0;
    
    /**
     * @brief 创建权限管理器
     * 
     * @return 平台特定的权限管理器实例
     */
    virtual IPrivilegeManagerPtr createPrivilegeManager() = 0;
    
    /**
     * @brief 创建 Hosts 文件管理器
     * 
     * @param backupDir 备份文件存储目录
     * @return 平台特定的 Hosts 管理器实例
     */
    virtual IHostsManagerPtr createHostsManager(const std::string& backupDir) = 0;
    
    // ========== 平台信息 ==========
    
    /**
     * @brief 获取平台名称
     * 
     * @return 平台标识字符串，如 "Windows", "macOS", "Linux"
     */
    [[nodiscard]] virtual std::string platformName() const = 0;
    
    /**
     * @brief 检查当前平台是否支持
     * 
     * @return true 表示当前平台有完整的实现
     */
    [[nodiscard]] virtual bool isSupported() const = 0;

protected:
    IPlatformFactory() = default;
};

/// 平台工厂智能指针类型
using IPlatformFactoryPtr = std::unique_ptr<IPlatformFactory>;


/**
 * @brief 平台工厂创建器
 * 
 * 静态工厂类，根据编译时平台自动选择并创建对应的平台工厂。
 * 这是获取平台服务的唯一入口点。
 */
class PlatformFactory {
public:
    /**
     * @brief 创建当前平台的工厂实例
     * 
     * 根据编译时的平台宏自动选择对应的工厂实现。
     * 
     * @return 平台工厂实例
     * @throws std::runtime_error 如果平台不支持
     */
    static IPlatformFactoryPtr create();
    
    /**
     * @brief 获取当前平台名称
     * 
     * @return 平台标识字符串
     */
    static std::string currentPlatform();
    
    /**
     * @brief 检查当前平台是否支持
     * 
     * @return true 表示当前平台有对应的工厂实现
     */
    static bool isPlatformSupported();
    
private:
    PlatformFactory() = delete;
};

#endif // PLATFORM_FACTORY_H
