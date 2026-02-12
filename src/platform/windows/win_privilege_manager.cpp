#include "win_privilege_manager.h"
#include "core/log_manager.h"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

bool WinPrivilegeManager::isRunningAsAdmin()
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID administratorsGroup = nullptr;

    BOOL result = AllocateAndInitializeSid(
        &ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &administratorsGroup);

    if (result) {
        if (!CheckTokenMembership(nullptr, administratorsGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(administratorsGroup);
    }

    LOG_DEBUG(QString("Running as administrator: %1").arg(isAdmin ? "yes" : "no"));
    return isAdmin != FALSE;
#else
    LOG_WARNING("WinPrivilegeManager::isRunningAsAdmin called on non-Windows platform");
    return false;
#endif
}

bool WinPrivilegeManager::restartAsAdmin(const QString& exePath)
{
#ifdef Q_OS_WIN
    LOG_INFO(QString("Requesting administrator elevation for: %1").arg(exePath));

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(exePath.utf16());
    const QString args = QCoreApplication::arguments().mid(1).join(QStringLiteral(" "));
    std::wstring wArgs = args.toStdWString();
    if (!wArgs.empty()) {
        sei.lpParameters = wArgs.c_str();
    }
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            LOG_WARNING("User cancelled the UAC elevation prompt");
        } else {
            LOG_ERROR(QString("ShellExecuteExW failed with error code: %1").arg(error));
        }
        return false;
    }

    LOG_INFO("Administrator elevation request succeeded");
    return true;
#else
    Q_UNUSED(exePath)
    LOG_WARNING("WinPrivilegeManager::restartAsAdmin called on non-Windows platform");
    return false;
#endif
}
