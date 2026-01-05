/**
 * @file i_privilege_manager.h
 * @brief 权限管理器接口定义
 * 
 * 本文件定义了跨平台的系统权限检测和提升抽象接口。
 * 
 * @details
 * 功能概述：
 * 
 * 权限管理器负责检测当前进程的权限级别，以及在需要时提升权限。
 * 本应用需要管理员权限来修改 hosts 文件和安装根证书。
 * 
 * 平台实现：
 * 
 * | 平台    | 提权方式                    |
 * |---------|----------------------------|
 * | Windows | UAC (User Account Control) |
 * | macOS   | Authorization Services / sudo |
 * | Linux   | pkexec / sudo              |
 * 
 * 功能模块：
 * 
 * 1. 权限检测
 *    - isRunningAsAdmin(): 检查当前进程是否有管理员权限
 *    - isUserAdmin(): 检查当前用户是否属于管理员组
 * 
 * 2. 权限提升
 *    - restartAsAdmin(): 以管理员权限重新启动应用
 *    - executeAsAdmin(): 以管理员权限执行单个命令
 * 
 * 3. 平台信息
 *    - platformName(): 获取当前平台名称
 *    - elevationMethod(): 获取权限提升方式描述
 * 
 * @note 权限提升通常会导致当前进程退出并以新权限重启
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef I_PRIVILEGE_MANAGER_H
#define I_PRIVILEGE_MANAGER_H

#include <string>
#include <vector>
#include <memory>

/**
 * @brief 权限管理器接口
 * 
 * 纯抽象接口，定义系统权限管理的标准操作。
 * 用于检测当前进程权限级别，以及在需要时提升权限。
 * 不依赖任何 GUI 框架。
 * 
 * @note 权限提升通常会导致当前进程退出并以新权限重启
 */
class IPrivilegeManager {
public:
    virtual ~IPrivilegeManager() = default;
    
    // 禁止拷贝
    IPrivilegeManager(const IPrivilegeManager&) = delete;
    IPrivilegeManager& operator=(const IPrivilegeManager&) = delete;
    
    // ========== 权限检测 ==========
    
    /**
     * @brief 检查当前进程是否具有管理员/root 权限
     * 
     * @return true 表示当前进程以管理员权限运行
     */
    [[nodiscard]] virtual bool isRunningAsAdmin() const = 0;
    
    /**
     * @brief 检查当前用户是否属于管理员组
     * 
     * 即使进程未以管理员权限运行，用户也可能属于管理员组。
     * 这种情况下可以通过 UAC/sudo 提升权限。
     * 
     * @return true 表示用户属于管理员组
     */
    [[nodiscard]] virtual bool isUserAdmin() const = 0;
    
    // ========== 权限提升 ==========
    
    /**
     * @brief 以管理员权限重新启动应用程序
     * 
     * 请求系统以提升的权限重新启动当前应用程序。
     * 成功后当前进程应该退出，新进程将以管理员权限运行。
     * 
     * @param executablePath 可执行文件的完整路径
     * @param arguments 命令行参数（可选）
     * @return true 表示成功启动了提权进程
     * 
     * @note Windows: 触发 UAC 对话框
     * @note macOS/Linux: 可能弹出密码输入对话框
     */
    virtual bool restartAsAdmin(const std::string& executablePath, 
                                const std::vector<std::string>& arguments = {}) = 0;
    
    /**
     * @brief 以管理员权限执行单个命令
     * 
     * 在不重启整个应用的情况下，以管理员权限执行特定命令。
     * 
     * @param command 要执行的命令
     * @param arguments 命令参数
     * @return true 表示命令执行成功
     */
    virtual bool executeAsAdmin(const std::string& command,
                                const std::vector<std::string>& arguments = {}) = 0;
    
    // ========== 平台信息 ==========
    
    /**
     * @brief 获取当前平台名称
     * 
     * @return 平台标识字符串，如 "Windows", "macOS", "Linux"
     */
    [[nodiscard]] virtual std::string platformName() const = 0;
    
    /**
     * @brief 获取权限提升方式描述
     * 
     * 返回当前平台使用的权限提升机制描述。
     * 
     * @return 描述字符串，如 "UAC", "sudo", "pkexec"
     */
    [[nodiscard]] virtual std::string elevationMethod() const = 0;

protected:
    IPrivilegeManager() = default;
};

/// 权限管理器智能指针类型
using IPrivilegeManagerPtr = std::unique_ptr<IPrivilegeManager>;

#endif // I_PRIVILEGE_MANAGER_H
