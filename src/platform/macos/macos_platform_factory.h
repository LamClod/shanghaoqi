/**
 * @file macos_platform_factory.h
 * @brief macOS 平台工厂头文件
 * 
 * 本文件定义了 macOS 平台的具体工厂实现类。
 * 
 * @details
 * 功能概述：
 * 
 * MacOSPlatformFactory 是 IPlatformFactory 接口的 macOS 平台实现，
 * 负责创建 macOS 特定的服务实例。
 * 
 * 创建的服务：
 * 
 * 1. MacOSConfigManager
 *    - 使用 Keychain 加密敏感数据
 *    - YAML 格式配置存储
 * 
 * 2. MacOSCertManager
 *    - 使用 OpenSSL 生成证书
 *    - 使用 security 命令安装证书到 Keychain
 * 
 * 3. MacOSPrivilegeManager
 *    - 使用 osascript 进行权限提升
 *    - 检测 root 权限
 * 
 * 4. MacOSHostsManager
 *    - 管理 /etc/hosts 文件
 *    - 支持 UTF-8 编码
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef MACOS_PLATFORM_FACTORY_H
#define MACOS_PLATFORM_FACTORY_H

#include "../platform_factory.h"

/**
 * @brief macOS 平台工厂实现
 * 
 * 创建 macOS 平台特定的服务实例：
 * - MacOSConfigManager (Keychain)
 * - MacOSCertManager (OpenSSL + security)
 * - MacOSPrivilegeManager (osascript)
 * - MacOSHostsManager
 */
class MacOSPlatformFactory : public IPlatformFactory {
public:
    MacOSPlatformFactory() = default;
    ~MacOSPlatformFactory() override = default;
    
    // ========== IPlatformFactory 接口实现 ==========
    
    IConfigManagerPtr createConfigManager(const std::string& configPath) override;
    ICertManagerPtr createCertManager(const std::string& dataDir) override;
    IPrivilegeManagerPtr createPrivilegeManager() override;
    IHostsManagerPtr createHostsManager(const std::string& backupDir) override;
    
    [[nodiscard]] std::string platformName() const override;
    [[nodiscard]] bool isSupported() const override;
};

#endif // MACOS_PLATFORM_FACTORY_H
