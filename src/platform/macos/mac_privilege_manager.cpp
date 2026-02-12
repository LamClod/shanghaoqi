#include "mac_privilege_manager.h"
#include "core/log_manager.h"

#include <QProcess>

#ifdef Q_OS_MACOS
#include <unistd.h>
#endif

bool MacPrivilegeManager::isRunningAsAdmin()
{
#ifdef Q_OS_MACOS
    bool isRoot = (geteuid() == 0);
    LOG_DEBUG(QString("Running as root: %1 (euid=%2)").arg(isRoot ? "yes" : "no").arg(geteuid()));
    return isRoot;
#else
    LOG_WARNING("MacPrivilegeManager::isRunningAsAdmin called on non-macOS platform");
    return false;
#endif
}

bool MacPrivilegeManager::restartAsAdmin(const QString& exePath)
{
#ifdef Q_OS_MACOS
    LOG_INFO(QString("Requesting administrator elevation via AppleScript for: %1").arg(exePath));

    // Use osascript to invoke AppleScript's "do shell script ... with administrator privileges"
    // This triggers the macOS authorization dialog for the user
    QString script = QString(
        "do shell script quoted form of \"%1\" with administrator privileges"
    ).arg(exePath);

    QProcess proc;
    proc.start("osascript", {"-e", script});

    if (!proc.waitForStarted(5000)) {
        LOG_ERROR("Failed to start osascript process");
        return false;
    }

    if (!proc.waitForFinished(60000)) {
        LOG_ERROR("osascript process timed out (user may not have responded to auth dialog)");
        proc.kill();
        return false;
    }

    if (proc.exitCode() != 0) {
        QString stdErr = QString::fromLocal8Bit(proc.readAllStandardError());
        if (stdErr.contains("User canceled") || stdErr.contains("user canceled")) {
            LOG_WARNING("User cancelled the authorization dialog");
        } else {
            LOG_ERROR(QString("osascript failed (exit %1): %2").arg(proc.exitCode()).arg(stdErr));
        }
        return false;
    }

    LOG_INFO("Administrator elevation request succeeded");
    return true;
#else
    Q_UNUSED(exePath)
    LOG_WARNING("MacPrivilegeManager::restartAsAdmin called on non-macOS platform");
    return false;
#endif
}
