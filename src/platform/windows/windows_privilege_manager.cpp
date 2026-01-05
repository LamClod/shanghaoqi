/**
 * @file windows_privilege_manager.cpp
 * @brief Windows 权限管理器实现
 * 
 * 实现 IPrivilegeManager 接口，使用 Windows UAC 实现权限检测和提升。
 * 
 * @details
 * 实现细节：
 * 
 * 权限检测（isRunningAsAdmin）：
 * 1. 创建管理员组的 SID（SECURITY_BUILTIN_DOMAIN_RID + DOMAIN_ALIAS_RID_ADMINS）
 * 2. 使用 CheckTokenMembership 检查当前进程令牌
 * 3. 释放 SID 资源
 * 
 * 权限提升（restartAsAdmin）：
 * 1. 构造 SHELLEXECUTEINFO 结构
 * 2. 设置 lpVerb 为 "runas" 触发 UAC
 * 3. 调用 ShellExecuteExW 启动新进程
 * 4. 成功后退出当前进程
 * 
 * 命令执行（executeAsAdmin）：
 * 1. 类似 restartAsAdmin，但设置 nShow 为 SW_HIDE
 * 2. 使用 WaitForSingleObject 等待命令完成
 * 3. 关闭进程句柄
 * 
 * @note 所有 Windows API 调用使用 Unicode 版本（W 后缀）
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "windows_privilege_manager.h"

// Qt 核心头文件
#include <QCoreApplication>  // 用于退出应用程序
#include <QString>           // Qt 字符串类型
#include <QStringList>       // Qt 字符串列表

// Windows 平台特定头文件
#ifdef Q_OS_WIN
#include <windows.h>         // Windows API 基础定义
#include <shellapi.h>        // ShellExecuteEx 函数
#endif

// ============================================================================
// 权限检测
// ============================================================================

/**
 * @brief 检测当前进程是否以管理员权限运行
 * 
 * 使用 Windows Security API 检测当前进程令牌是否包含管理员组成员资格。
 * 
 * @return true 当前进程以管理员权限运行
 * @return false 当前进程以普通用户权限运行
 * 
 * @details
 * 实现步骤：
 * 1. 使用 SECURITY_NT_AUTHORITY 创建 NT 权限标识
 * 2. 调用 AllocateAndInitializeSid 创建管理员组 SID
 *    - SECURITY_BUILTIN_DOMAIN_RID: 内置域标识符
 *    - DOMAIN_ALIAS_RID_ADMINS: 管理员组别名 RID
 * 3. 调用 CheckTokenMembership 检查当前令牌
 * 4. 释放 SID 资源
 */
bool WindowsPrivilegeManager::isRunningAsAdmin() const
{
#ifdef Q_OS_WIN
    // 管理员状态标志，默认为 FALSE
    BOOL isAdmin = FALSE;
    
    // 管理员组 SID 指针，初始化为空
    PSID administratorsGroup = nullptr;
    
    // 创建 NT 权限标识符
    // SECURITY_NT_AUTHORITY = {0,0,0,0,0,5}，表示 NT 权限
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    // 分配并初始化管理员组的 SID
    // SID 格式: S-1-5-32-544 (内置管理员组)
    if (AllocateAndInitializeSid(
            &ntAuthority,                   // SID 权限标识符
            2,                              // 子权限数量（2个）
            SECURITY_BUILTIN_DOMAIN_RID,    // 第一个子权限：内置域 (32)
            DOMAIN_ALIAS_RID_ADMINS,        // 第二个子权限：管理员组 (544)
            0, 0, 0, 0, 0, 0,               // 剩余 6 个子权限未使用
            &administratorsGroup)) {        // 输出：分配的 SID
        
        // 检查当前进程令牌是否包含管理员组成员资格
        // 第一个参数为 nullptr 表示使用当前进程的主令牌
        if (!CheckTokenMembership(nullptr, administratorsGroup, &isAdmin)) {
            // 检查失败，假设不是管理员
            isAdmin = FALSE;
        }
        
        // 释放分配的 SID 内存
        FreeSid(administratorsGroup);
    }
    
    // 将 BOOL 转换为 bool 返回
    return isAdmin == TRUE;
#else
    // 非 Windows 平台默认返回 true（假设有权限）
    // 这样可以避免在其他平台上阻止功能
    return true;
#endif
}

/**
 * @brief 检测当前用户是否属于管理员组
 * 
 * 在 Windows 上，此方法与 isRunningAsAdmin 功能相同。
 * 
 * @return true 用户属于管理员组
 * @return false 用户不属于管理员组
 * 
 * @note 由于 UAC 的存在，用户可能属于管理员组但当前进程未提权
 */
bool WindowsPrivilegeManager::isUserAdmin() const
{
    // 在 Windows 上，isRunningAsAdmin 已经检查了用户是否属于管理员组
    // 如果需要区分"用户是管理员但未提权"的情况，需要使用不同的检测方法
    return isRunningAsAdmin();
}

// ============================================================================
// 权限提升（使用 std::string 和 std::vector<std::string>）
// ============================================================================

/**
 * @brief 以管理员权限重启应用程序
 * 
 * 使用 ShellExecuteEx 的 "runas" 动词触发 UAC 对话框，
 * 成功后退出当前进程。
 * 
 * @param executablePath 可执行文件路径
 * @param arguments 命令行参数列表
 * @return true 成功启动管理员进程（当前进程将退出）
 * @return false 启动失败（用户可能取消了 UAC）
 * 
 * @details
 * 实现步骤：
 * 1. 将 std::string 参数转换为 QString（便于 UTF-16 转换）
 * 2. 构造 SHELLEXECUTEINFOW 结构
 * 3. 设置 lpVerb 为 "runas" 触发 UAC
 * 4. 调用 ShellExecuteExW 启动新进程
 * 5. 成功后调用 QCoreApplication::quit() 退出当前进程
 */
bool WindowsPrivilegeManager::restartAsAdmin(const std::string& executablePath,
                                              const std::vector<std::string>& arguments)
{
#ifdef Q_OS_WIN
    // 将 std::string 转换为 QString，便于后续 UTF-16 转换
    QString qExePath = QString::fromStdString(executablePath);
    
    // 将参数列表转换为 QStringList
    QStringList qArgs;
    for (const std::string& arg : arguments) {
        qArgs << QString::fromStdString(arg);
    }
    
    // 将参数列表合并为单个字符串，用空格分隔
    QString args = qArgs.join(' ');
    
    // 初始化 SHELLEXECUTEINFOW 结构
    // 使用 W 后缀版本（Unicode）以支持中文路径
    SHELLEXECUTEINFOW sei = { sizeof(sei) };  // cbSize 必须设置为结构大小
    
    // 设置动词为 "runas"，这会触发 UAC 提权对话框
    sei.lpVerb = L"runas";
    
    // 设置要执行的文件路径（UTF-16 编码）
    sei.lpFile = reinterpret_cast<LPCWSTR>(qExePath.utf16());
    
    // 设置命令行参数（如果有的话）
    sei.lpParameters = args.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(args.utf16());
    
    // 父窗口句柄，nullptr 表示使用桌面窗口
    sei.hwnd = nullptr;
    
    // 窗口显示方式：正常显示
    sei.nShow = SW_NORMAL;
    
    // 标志位：不关闭进程句柄（虽然这里不需要等待）
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    // 执行提权启动
    if (ShellExecuteExW(&sei)) {
        // 成功启动管理员进程
        // 退出当前（非管理员）进程
        QCoreApplication::quit();
        return true;
    }
    
    // 启动失败
    // 常见原因：用户点击了 UAC 对话框的"否"按钮
    return false;
#else
    // 非 Windows 平台，标记参数未使用以避免编译警告
    Q_UNUSED(executablePath)
    Q_UNUSED(arguments)
    return false;
#endif
}

/**
 * @brief 以管理员权限执行命令
 * 
 * 使用 ShellExecuteEx 的 "runas" 动词以管理员权限执行指定命令，
 * 并等待命令执行完成。
 * 
 * @param command 要执行的命令或程序路径
 * @param arguments 命令行参数列表
 * @return true 命令执行成功
 * @return false 执行失败（用户取消 UAC 或命令执行出错）
 * 
 * @details
 * 与 restartAsAdmin 的区别：
 * - 不退出当前进程
 * - 隐藏命令窗口（SW_HIDE）
 * - 等待命令执行完成
 */
bool WindowsPrivilegeManager::executeAsAdmin(const std::string& command,
                                              const std::vector<std::string>& arguments)
{
#ifdef Q_OS_WIN
    // 将 std::string 转换为 QString
    QString qCommand = QString::fromStdString(command);
    
    // 将参数列表转换为 QStringList
    QStringList qArgs;
    for (const std::string& arg : arguments) {
        qArgs << QString::fromStdString(arg);
    }
    
    // 合并参数为单个字符串
    QString args = qArgs.join(' ');
    
    // 初始化 SHELLEXECUTEINFOW 结构
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    
    // 设置 "runas" 动词触发 UAC
    sei.lpVerb = L"runas";
    
    // 设置要执行的命令
    sei.lpFile = reinterpret_cast<LPCWSTR>(qCommand.utf16());
    
    // 设置命令参数
    sei.lpParameters = args.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(args.utf16());
    
    // 父窗口句柄
    sei.hwnd = nullptr;
    
    // 隐藏命令窗口，避免弹出黑色控制台窗口
    sei.nShow = SW_HIDE;
    
    // 设置标志位：不关闭进程句柄，以便后续等待
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    
    // 执行命令
    if (ShellExecuteExW(&sei)) {
        // 等待命令执行完成
        if (sei.hProcess) {
            // INFINITE 表示无限等待，直到进程结束
            WaitForSingleObject(sei.hProcess, INFINITE);
            
            // 关闭进程句柄，释放系统资源
            CloseHandle(sei.hProcess);
        }
        return true;
    }
    
    // 执行失败
    return false;
#else
    // 非 Windows 平台
    Q_UNUSED(command)
    Q_UNUSED(arguments)
    return false;
#endif
}

// ============================================================================
// 平台信息（返回 std::string）
// ============================================================================

/**
 * @brief 获取平台名称
 * @return 平台名称字符串 "Windows"
 */
std::string WindowsPrivilegeManager::platformName() const
{
    return "Windows";
}

/**
 * @brief 获取权限提升方式描述
 * @return 提权方式描述 "UAC (User Account Control)"
 * 
 * @details
 * UAC（用户账户控制）是 Windows Vista 及以后版本引入的安全功能，
 * 用于防止未经授权的系统更改。当应用程序需要管理员权限时，
 * UAC 会弹出对话框请求用户确认。
 */
std::string WindowsPrivilegeManager::elevationMethod() const
{
    return "UAC (User Account Control)";
}
