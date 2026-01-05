/**
 * @file linux_privilege_manager.h
 * @brief Linux 权限管理器
 * 
 * 实现 IPrivilegeManager 接口，提供 Linux 平台的权限管理功能。
 * 
 * @details
 * 功能概述：
 * - 检测当前进程是否以 root 权限运行
 * - 检测当前用户是否属于 sudo/wheel 组
 * - 使用 pkexec 或 sudo 进行权限提升
 * - 支持以 root 权限重启应用程序
 * - 支持以 root 权限执行单个命令
 * 
 * 技术实现：
 * - 使用 geteuid() 检测 root 权限
 * - 使用 getgroups() 检测用户组
 * - 优先使用 pkexec（图形化密码提示）
 * - 回退到 sudo（终端密码提示）
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef LINUX_PRIVILEGE_MANAGER_H
#define LINUX_PRIVILEGE_MANAGER_H

#include "../interfaces/i_privilege_manager.h"

/**
 * @brief Linux 权限管理器实现
 * 
 * 实现 IPrivilegeManager 接口，提供 Linux 平台的权限管理功能。
 * 
 * @details
 * 技术实现：
 * - 使用 geteuid() 检测 root 权限
 * - 优先使用 pkexec 进行图形化权限提升
 * - 回退到 sudo 进行终端权限提升
 */
class LinuxPrivilegeManager : public IPrivilegeManager {
public:
    LinuxPrivilegeManager() = default;
    ~LinuxPrivilegeManager() override = default;
    
    // ========== IPrivilegeManager 接口实现 ==========
    
    [[nodiscard]] bool isRunningAsAdmin() const override;
    [[nodiscard]] bool isUserAdmin() const override;
    bool restartAsAdmin(const std::string& executablePath,
                        const std::vector<std::string>& arguments = {}) override;
    bool executeAsAdmin(const std::string& command,
                        const std::vector<std::string>& arguments = {}) override;
    [[nodiscard]] std::string platformName() const override;
    [[nodiscard]] std::string elevationMethod() const override;

private:
    /**
     * @brief 检查 pkexec 是否可用
     */
    bool isPkexecAvailable() const;
    
    /**
     * @brief 检查 sudo 是否可用
     */
    bool isSudoAvailable() const;
};

#endif // LINUX_PRIVILEGE_MANAGER_H
