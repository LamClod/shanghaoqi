/**
 * @file macos_privilege_manager.h
 * @brief macOS 权限管理器
 * 
 * 实现 IPrivilegeManager 接口，提供 macOS 平台的权限管理功能。
 * 
 * @details
 * 功能概述：
 * - 检测当前进程是否以 root 权限运行
 * - 检测当前用户是否属于 admin 组
 * - 使用 osascript 进行权限提升（图形化密码提示）
 * - 支持以 root 权限重启应用程序
 * - 支持以 root 权限执行单个命令
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef MACOS_PRIVILEGE_MANAGER_H
#define MACOS_PRIVILEGE_MANAGER_H

#include "../interfaces/i_privilege_manager.h"

/**
 * @brief macOS 权限管理器实现
 */
class MacOSPrivilegeManager : public IPrivilegeManager {
public:
    MacOSPrivilegeManager() = default;
    ~MacOSPrivilegeManager() override = default;
    
    [[nodiscard]] bool isRunningAsAdmin() const override;
    [[nodiscard]] bool isUserAdmin() const override;
    bool restartAsAdmin(const std::string& executablePath,
                        const std::vector<std::string>& arguments = {}) override;
    bool executeAsAdmin(const std::string& command,
                        const std::vector<std::string>& arguments = {}) override;
    [[nodiscard]] std::string platformName() const override;
    [[nodiscard]] std::string elevationMethod() const override;
};

#endif // MACOS_PRIVILEGE_MANAGER_H
