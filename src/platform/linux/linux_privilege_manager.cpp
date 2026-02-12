#include "linux_privilege_manager.h"
#include "core/log_manager.h"

#include <QProcess>
#include <QFile>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

bool LinuxPrivilegeManager::isRunningAsAdmin()
{
#ifdef Q_OS_LINUX
    bool isRoot = (geteuid() == 0);
    LOG_DEBUG(QString("Running as root: %1 (euid=%2)").arg(isRoot ? "yes" : "no").arg(geteuid()));
    return isRoot;
#else
    LOG_WARNING("LinuxPrivilegeManager::isRunningAsAdmin called on non-Linux platform");
    return false;
#endif
}

bool LinuxPrivilegeManager::restartAsAdmin(const QString& exePath)
{
#ifdef Q_OS_LINUX
    LOG_INFO(QString("Requesting root elevation for: %1").arg(exePath));

    // Determine the best privilege escalation tool available
    QString escalationTool;
    QStringList escalationArgs;

    if (QFile::exists("/usr/bin/pkexec")) {
        // PolicyKit agent - works on most modern Linux desktops
        escalationTool = "pkexec";
        escalationArgs = {exePath};
    } else if (QFile::exists("/usr/bin/kdesudo")) {
        // KDE sudo dialog
        escalationTool = "kdesudo";
        escalationArgs = {exePath};
    } else if (QFile::exists("/usr/bin/gksudo")) {
        // GNOME sudo dialog (deprecated but still available on some systems)
        escalationTool = "gksudo";
        escalationArgs = {exePath};
    } else if (QFile::exists("/usr/bin/sudo")) {
        // Fallback to terminal sudo (may not work without a terminal)
        escalationTool = "sudo";
        escalationArgs = {exePath};
    } else {
        LOG_ERROR("No privilege escalation tool found (pkexec, kdesudo, gksudo, or sudo)");
        return false;
    }

    LOG_INFO(QString("Using escalation tool: %1").arg(escalationTool));

    QProcess proc;
    proc.start(escalationTool, escalationArgs);

    if (!proc.waitForStarted(5000)) {
        LOG_ERROR(QString("Failed to start %1").arg(escalationTool));
        return false;
    }

    // We don't wait for it to finish since it's restarting the application
    // Detach the process so it outlives the current process
    LOG_INFO("Administrator elevation request initiated");
    return true;
#else
    Q_UNUSED(exePath)
    LOG_WARNING("LinuxPrivilegeManager::restartAsAdmin called on non-Linux platform");
    return false;
#endif
}
