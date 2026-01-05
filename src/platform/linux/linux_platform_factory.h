/**
 * @file linux_platform_factory.h
 * @brief Linux 平台工厂头文件
 * 
 * 本文件定义了 Linux 平台的具体工厂实现类。
 * 
 * @details
 * 功能概述：
 * 
 * LinuxPlatformFactory 是 IPlatformFactory 接口的 Linux 平台实现，
 * 负责创建 Linux 特定的服务实例。
 * 
 * 创建的服务：
 * 
 * 1. LinuxConfigManager
 *    - 使用 libsecret 加密敏感数据
 *    - YAML 格式配置存储
 * 
 * 2. LinuxCertManager
 *    - 使用 OpenSSL 生成证书
 *    - 使用 update-ca-certificates 安装证书
 * 
 * 3. LinuxPrivilegeManager
 *    - 使用 pkexec/sudo 进行权限提升
 *    - 检测 root 权限
 * 
 * 4. LinuxHostsManager
 *    - 管理 /etc/hosts 文件
 *    - 支持 UTF-8 编码
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef LINUX_PLATFORM_FACTORY_H
#define LINUX_PLATFORM_FACTORY_H

#include "../platform_factory.h"

/**
 * @brief Linux 平台工厂实现
 * 
 * 创建 Linux 平台特定的服务实例：
 * - LinuxConfigManager (libsecret)
 * - LinuxCertManager (OpenSSL + update-ca-certificates)
 * - LinuxPrivilegeManager (pkexec/sudo)
 * - LinuxHostsManager
 */
class LinuxPlatformFactory : public IPlatformFactory {
public:
    LinuxPlatformFactory() = default;
    ~LinuxPlatformFactory() override = default;
    
    // ========== IPlatformFactory 接口实现 ==========
    
    IConfigManagerPtr createConfigManager(const std::string& configPath) override;
    ICertManagerPtr createCertManager(const std::string& dataDir) override;
    IPrivilegeManagerPtr createPrivilegeManager() override;
    IHostsManagerPtr createHostsManager(const std::string& backupDir) override;
    
    [[nodiscard]] std::string platformName() const override;
    [[nodiscard]] bool isSupported() const override;
};

#endif // LINUX_PLATFORM_FACTORY_H
