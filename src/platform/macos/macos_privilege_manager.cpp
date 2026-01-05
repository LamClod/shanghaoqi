/**
 * @file macos_privilege_manager.cpp
 * @brief macOS 权限管理器实现
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "macos_privilege_manager.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QProcess>

#ifdef Q_OS_MACOS
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#endif

// ============================================================================
// 权限检测
// ============================================================================

bool MacOSPrivilegeManager::isRunningAsAdmin() const
{
#ifdef Q_OS_MACOS
    return geteuid() == 0;
#else
    return false;
#endif
}

bool MacOSPrivilegeManager::isUserAdmin() const
{
#ifdef Q_OS_MACOS
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    if (!pw) return false;
    
    int ngroups = 32;
    gid_t groups[32];
    if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups) == -1) {
        return false;
    }
    
    for (int i = 0; i < ngroups; i++) {
        struct group* gr = getgrgid(groups[i]);
        if (gr) {
            QString groupName = QString::fromLocal8Bit(gr->gr_name);
            if (groupName == "admin" || groupName == "wheel") {
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
// 权限提升
// ============================================================================

bool MacOSPrivilegeManager::restartAsAdmin(const std::string& executablePath,
                                            const std::vector<std::string>& arguments)
{
#ifdef Q_OS_MACOS
    QString qExePath = QString::fromStdString(executablePath);
    
    QStringList qArgs;
    for (const std::string& arg : arguments) {
        qArgs << QString::fromStdString(arg);
    }
    
    QString argsStr = qArgs.join("' '");
    if (!argsStr.isEmpty()) {
        argsStr = " '" + argsStr + "'";
    }
    
    // 使用 osascript 请求管理员权限
    QString script = QString(
        "do shell script \"'%1'%2\" with administrator privileges"
    ).arg(qExePath, argsStr);
    
    QProcess* process = new QProcess();
    process->setProgram("osascript");
    process->setArguments({"-e", script});
    
    bool started = process->startDetached();
    
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

bool MacOSPrivilegeManager::executeAsAdmin(const std::string& command,
                                            const std::vector<std::string>& arguments)
{
#ifdef Q_OS_MACOS
    QString qCommand = QString::fromStdString(command);
    
    QStringList qArgs;
    for (const std::string& arg : arguments) {
        qArgs << QString::fromStdString(arg);
    }
    
    QString argsStr = qArgs.join("' '");
    if (!argsStr.isEmpty()) {
        argsStr = " '" + argsStr + "'";
    }
    
    QString script = QString(
        "do shell script \"'%1'%2\" with administrator privileges"
    ).arg(qCommand, argsStr);
    
    QProcess process;
    process.start("osascript", {"-e", script});
    process.waitForFinished(-1);
    
    return process.exitCode() == 0;
#else
    Q_UNUSED(command)
    Q_UNUSED(arguments)
    return false;
#endif
}

// ============================================================================
// 平台信息
// ============================================================================

std::string MacOSPrivilegeManager::platformName() const
{
    return "macOS";
}

std::string MacOSPrivilegeManager::elevationMethod() const
{
    return "osascript (Authorization Services)";
}
