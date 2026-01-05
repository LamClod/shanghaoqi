/**
 * @file windows_privilege_manager.h
 * @brief Windows 权限管理器
 * 
 * 实现 IPrivilegeManager 接口，提供 Windows 平台的权限管理功能。
 * 
 * @details
 * 功能概述：
 * - 检测当前进程是否以管理员权限运行
 * - 检测当前用户是否属于管理员组
 * - 使用 UAC（用户账户控制）触发权限提升
 * - 支持以管理员权限重启应用程序
 * - 支持以管理员权限执行单个命令
 * 
 * 技术实现：
 * - 使用 Windows Security API 检测权限
 * - 使用 AllocateAndInitializeSid 创建管理员组 SID
 * - 使用 CheckTokenMembership 验证令牌成员资格
 * - 使用 ShellExecuteEx 的 "runas" 动词触发 UAC
 * 
 * 使用示例：
 * @code
 * WindowsPrivilegeManager manager;
 * 
 * // 检测权限
 * if (!manager.isRunningAsAdmin()) {
 *     // 需要提权
 *     manager.restartAsAdmin(QCoreApplication::applicationFilePath().toStdString());
 * }
 * 
 * // 以管理员权限执行命令
 * manager.executeAsAdmin("netsh", {"advfirewall", "set", "allprofiles", "state", "on"});
 * @endcode
 * 
 * @note UAC 对话框需要用户确认，用户可能取消操作
 * @warning 频繁触发 UAC 可能影响用户体验
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef WINDOWS_PRIVILEGE_MANAGER_H
#define WINDOWS_PRIVILEGE_MANAGER_H

#include "../interfaces/i_privilege_manager.h"

/**
 * @brief Windows 权限管理器实现
 * 
 * 实现 IPrivilegeManager 接口，提供 Windows 平台的权限管理功能。
 * 
 * @details
 * 技术实现：
 * - 使用 Windows Security API 检测管理员权限
 * - 使用 AllocateAndInitializeSid 创建管理员组 SID
 * - 使用 CheckTokenMembership 验证令牌成员资格
 * - 使用 ShellExecuteEx 的 "runas" 动词触发 UAC 提权
 * 
 * @note 所有 Windows API 调用使用 Unicode 版本
 */
class WindowsPrivilegeManager : public IPrivilegeManager {
public:
    WindowsPrivilegeManager() = default;
    ~WindowsPrivilegeManager() override = default;
    
    // ========== IPrivilegeManager 接口实现 ==========
    
    [[nodiscard]] bool isRunningAsAdmin() const override;
    [[nodiscard]] bool isUserAdmin() const override;
    bool restartAsAdmin(const std::string& executablePath,
                        const std::vector<std::string>& arguments = {}) override;
    bool executeAsAdmin(const std::string& command,
                        const std::vector<std::string>& arguments = {}) override;
    [[nodiscard]] std::string platformName() const override;
    [[nodiscard]] std::string elevationMethod() const override;
};

#endif // WINDOWS_PRIVILEGE_MANAGER_H
