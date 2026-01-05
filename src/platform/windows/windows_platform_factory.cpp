/**
 * @file windows_platform_factory.cpp
 * @brief Windows 平台工厂实现文件
 * 
 * 本文件实现了 WindowsPlatformFactory 类的所有方法。
 * 
 * @details
 * 实现内容：
 * 
 * 1. 服务创建方法
 *    - createConfigManager(): 创建 WindowsConfigManager 实例
 *    - createCertManager(): 创建 WindowsCertManager 实例
 *    - createPrivilegeManager(): 创建 WindowsPrivilegeManager 实例
 *    - createHostsManager(): 创建 WindowsHostsManager 实例
 * 
 * 2. 平台信息方法
 *    - platformName(): 返回 "Windows"
 *    - isSupported(): 在 Windows 平台返回 true
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "windows_platform_factory.h"
#include "windows_config_manager.h"
#include "windows_cert_manager.h"
#include "windows_privilege_manager.h"
#include "windows_hosts_manager.h"

#include <QString>

// ============================================================================
// 服务创建方法
// ============================================================================

IConfigManagerPtr WindowsPlatformFactory::createConfigManager(const std::string& configPath)
{
    return std::make_unique<WindowsConfigManager>(QString::fromStdString(configPath), nullptr);
}

ICertManagerPtr WindowsPlatformFactory::createCertManager(const std::string& dataDir)
{
    return std::make_unique<WindowsCertManager>(QString::fromStdString(dataDir), nullptr);
}

IPrivilegeManagerPtr WindowsPlatformFactory::createPrivilegeManager()
{
    return std::make_unique<WindowsPrivilegeManager>();
}

IHostsManagerPtr WindowsPlatformFactory::createHostsManager(const std::string& backupDir)
{
    return std::make_unique<WindowsHostsManager>(QString::fromStdString(backupDir), nullptr);
}

// ============================================================================
// 平台信息
// ============================================================================

std::string WindowsPlatformFactory::platformName() const
{
    return "Windows";
}

bool WindowsPlatformFactory::isSupported() const
{
#if defined(_WIN32) || defined(_WIN64)
    return true;
#else
    return false;
#endif
}
