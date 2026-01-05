/**
 * @file windows_platform_factory.h
 * @brief Windows 平台工厂头文件
 * 
 * 本文件定义了 Windows 平台的具体工厂实现类。
 * 
 * @details
 * 功能概述：
 * 
 * WindowsPlatformFactory 是 IPlatformFactory 接口的 Windows 平台实现，
 * 负责创建 Windows 特定的服务实例。
 * 
 * 创建的服务：
 * 
 * 1. WindowsConfigManager
 *    - 使用 DPAPI 加密敏感数据
 *    - YAML 格式配置存储
 * 
 * 2. WindowsCertManager
 *    - 使用 OpenSSL 生成证书
 *    - 使用 CryptoAPI 安装证书
 * 
 * 3. WindowsPrivilegeManager
 *    - 使用 UAC 进行权限提升
 *    - 检测管理员权限
 * 
 * 4. WindowsHostsManager
 *    - 管理 Windows hosts 文件
 *    - 支持多种编码格式
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef WINDOWS_PLATFORM_FACTORY_H
#define WINDOWS_PLATFORM_FACTORY_H

#include "../platform_factory.h"

/**
 * @brief Windows 平台工厂实现
 * 
 * 创建 Windows 平台特定的服务实例：
 * - WindowsConfigManager (DPAPI)
 * - WindowsCertManager (OpenSSL + CryptoAPI)
 * - WindowsPrivilegeManager (UAC)
 * - WindowsHostsManager
 */
class WindowsPlatformFactory : public IPlatformFactory {
public:
    WindowsPlatformFactory() = default;
    ~WindowsPlatformFactory() override = default;
    
    // ========== IPlatformFactory 接口实现 ==========
    
    IConfigManagerPtr createConfigManager(const std::string& configPath) override;
    ICertManagerPtr createCertManager(const std::string& dataDir) override;
    IPrivilegeManagerPtr createPrivilegeManager() override;
    IHostsManagerPtr createHostsManager(const std::string& backupDir) override;
    
    [[nodiscard]] std::string platformName() const override;
    [[nodiscard]] bool isSupported() const override;
};

#endif // WINDOWS_PLATFORM_FACTORY_H
