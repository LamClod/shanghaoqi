/**
 * @file linux_privilege_manager.cpp
 * @brief Linux 权限管理器实现
 * 
 * 实现 IPrivilegeManager 接口，使用 pkexec/sudo 实现权限检测和提升。
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "linux_privilege_manager.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFile>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#endif

// ============================================================================
// 权限检测
// ============================================================================

bool LinuxPrivilegeManager::isRunningAsAdmin() const
{
#ifdef Q_OS_LINUX
    // 检查有效用户 ID 是否为 0 (root)
    return geteuid() == 0;
#else
    return false;
#endif
}

bool LinuxPrivilegeManager::isUserAdmin() const
{
#ifdef Q_OS_LINUX
    // 检查用户是否属于 sudo 或 wheel 组
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (!pw) {
        return false;
    }
    
    // 获取用户所属的所有组
    int ngroups = 32;
    gid_t groups[32];
    if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups) == -1) {
        return false;
    }
    
    // 检查是否属于 sudo 或 wheel 组
    for (int i = 0; i < ngroups; i++) {
        struct group* gr = getgrgid(groups[i]);
        if (gr) {
            QString groupName = QString::fromLocal8Bit(gr->gr_name);
            if (groupName == "sudo" || groupName == "wheel" || groupName == "admin") {
                return true;
            }
        }
    }
    
    return false;
#else
    return false;
#endif
}

// ============================================================================
// 辅助方法
// ============================================================================

bool LinuxPrivilegeManager::isPkexecAvailable() const
{
    return QFile::exists("/usr/bin/pkexec");
}

bool LinuxPrivilegeManager::isSudoAvailable() const
{
    return QFile::exists("/usr/bin/sudo");
}

// ============================================================================
// 权限提升
// ============================================================================

bool LinuxPrivilegeManager::restartAsAdmin(const std::string& executablePath,
                                            const std::vector<std::string>& arguments)
{
#ifdef Q_OS_LINUX
    QString qExePath = QString::fromStdString(executablePath);
    
    QStringList qArgs;
    for (const std::string& arg : arguments) {
        qArgs << QString::fromStdString(arg);
    }
    
    QProcess* process = new QProcess();
    process->setProgram(qExePath);
    process->setArguments(qArgs);
    
    bool started = false;
    
    // 优先使用 pkexec（图形化密码提示）
    if (isPkexecAvailable()) {
        QStringList pkexecArgs;
        pkexecArgs << qExePath << qArgs;
        
        process->setProgram("pkexec");
        process->setArguments(pkexecArgs);
        started = process->startDetached();
    }
    
    // 回退到 sudo（需要终端）
    if (!started && isSudoAvailable()) {
        // 尝试使用图形化 sudo 前端
        QStringList sudoArgs;
        
        // 检查是否有图形化 sudo 前端
        if (QFile::exists("/usr/bin/gksudo")) {
            process->setProgram("gksudo");
            sudoArgs << qExePath << qArgs;
        } else if (QFile::exists("/usr/bin/kdesudo")) {
            process->setProgram("kdesudo");
            sudoArgs << qExePath << qArgs;
        } else {
            // 使用终端运行 sudo
            process->setProgram("x-terminal-emulator");
            sudoArgs << "-e" << "sudo" << qExePath << qArgs;
        }
        
        process->setArguments(sudoArgs);
        started = process->startDetached();
    }
    
    if (started) {
        QCoreApplication::quit();
        return true;
    }
    
    delete process;
    return false;
#else
    Q_UNUSED(executablePath)
    Q_UNUSED(arguments)
    return false;
#endif
}

bool LinuxPrivilegeManager::executeAsAdmin(const std::string& command,
                                            const std::vector<std::string>& arguments)
{
#ifdef Q_OS_LINUX
    QString qCommand = QString::fromStdString(command);
    
    QStringList qArgs;
    for (const std::string& arg : arguments) {
        qArgs << QString::fromStdString(arg);
    }
    
    QProcess process;
    
    // 优先使用 pkexec
    if (isPkexecAvailable()) {
        QStringList pkexecArgs;
        pkexecArgs << qCommand << qArgs;
        
        process.start("pkexec", pkexecArgs);
        process.waitForFinished(-1);
        
        return process.exitCode() == 0;
    }
    
    // 回退到 sudo
    if (isSudoAvailable()) {
        QStringList sudoArgs;
        sudoArgs << qCommand << qArgs;
        
        process.start("sudo", sudoArgs);
        process.waitForFinished(-1);
        
        return process.exitCode() == 0;
    }
    
    return false;
#else
    Q_UNUSED(command)
    Q_UNUSED(arguments)
    return false;
#endif
}

// ============================================================================
// 平台信息
// ============================================================================

std::string LinuxPrivilegeManager::platformName() const
{
    return "Linux";
}

std::string LinuxPrivilegeManager::elevationMethod() const
{
    if (isPkexecAvailable()) {
        return "pkexec (PolicyKit)";
    } else if (isSudoAvailable()) {
        return "sudo";
    }
    return "None available";
}
