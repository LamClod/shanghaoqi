/**
 * @file macos_platform_factory.cpp
 * @brief macOS 平台工厂实现文件
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "macos_platform_factory.h"
#include "macos_config_manager.h"
#include "macos_cert_manager.h"
#include "macos_privilege_manager.h"
#include "macos_hosts_manager.h"

#include <QString>

// ============================================================================
// 服务创建方法
// ============================================================================

IConfigManagerPtr MacOSPlatformFactory::createConfigManager(const std::string& configPath)
{
    return std::make_unique<MacOSConfigManager>(QString::fromStdString(configPath), nullptr);
}

ICertManagerPtr MacOSPlatformFactory::createCertManager(const std::string& dataDir)
{
    return std::make_unique<MacOSCertManager>(QString::fromStdString(dataDir), nullptr);
}

IPrivilegeManagerPtr MacOSPlatformFactory::createPrivilegeManager()
{
    return std::make_unique<MacOSPrivilegeManager>();
}

IHostsManagerPtr MacOSPlatformFactory::createHostsManager(const std::string& backupDir)
{
    return std::make_unique<MacOSHostsManager>(QString::fromStdString(backupDir), nullptr);
}

// ============================================================================
// 平台信息
// ============================================================================

std::string MacOSPlatformFactory::platformName() const
{
    return "macOS";
}

bool MacOSPlatformFactory::isSupported() const
{
#if defined(__APPLE__) && defined(__MACH__)
    return true;
#else
    return false;
#endif
}
