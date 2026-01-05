/**
 * @file linux_platform_factory.cpp
 * @brief Linux 平台工厂实现文件
 * 
 * 本文件实现了 LinuxPlatformFactory 类的所有方法。
 * 
 * @details
 * 实现内容：
 * 
 * 1. 服务创建方法
 *    - createConfigManager(): 创建 LinuxConfigManager 实例
 *    - createCertManager(): 创建 LinuxCertManager 实例
 *    - createPrivilegeManager(): 创建 LinuxPrivilegeManager 实例
 *    - createHostsManager(): 创建 LinuxHostsManager 实例
 * 
 * 2. 平台信息方法
 *    - platformName(): 返回 "Linux"
 *    - isSupported(): 在 Linux 平台返回 true
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "linux_platform_factory.h"
#include "linux_config_manager.h"
#include "linux_cert_manager.h"
#include "linux_privilege_manager.h"
#include "linux_hosts_manager.h"

#include <QString>

// ============================================================================
// 服务创建方法
// ============================================================================

IConfigManagerPtr LinuxPlatformFactory::createConfigManager(const std::string& configPath)
{
    return std::make_unique<LinuxConfigManager>(QString::fromStdString(configPath), nullptr);
}

ICertManagerPtr LinuxPlatformFactory::createCertManager(const std::string& dataDir)
{
    return std::make_unique<LinuxCertManager>(QString::fromStdString(dataDir), nullptr);
}

IPrivilegeManagerPtr LinuxPlatformFactory::createPrivilegeManager()
{
    return std::make_unique<LinuxPrivilegeManager>();
}

IHostsManagerPtr LinuxPlatformFactory::createHostsManager(const std::string& backupDir)
{
    return std::make_unique<LinuxHostsManager>(QString::fromStdString(backupDir), nullptr);
}

// ============================================================================
// 平台信息
// ============================================================================

std::string LinuxPlatformFactory::platformName() const
{
    return "Linux";
}

bool LinuxPlatformFactory::isSupported() const
{
#if defined(__linux__)
    return true;
#else
    return false;
#endif
}
