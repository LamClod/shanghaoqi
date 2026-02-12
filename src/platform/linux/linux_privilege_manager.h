#pragma once
#include "platform/interfaces/i_privilege_manager.h"

class LinuxPrivilegeManager : public IPrivilegeManager {
public:
    bool isRunningAsAdmin() override;
    bool restartAsAdmin(const QString& exePath) override;
};
