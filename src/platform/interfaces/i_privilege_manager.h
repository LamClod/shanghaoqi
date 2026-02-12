#pragma once
#include <QString>
#include <memory>

class IPrivilegeManager {
public:
    virtual ~IPrivilegeManager() = default;
    virtual bool isRunningAsAdmin() = 0;
    virtual bool restartAsAdmin(const QString& exePath) = 0;
};

using IPrivilegeManagerPtr = std::unique_ptr<IPrivilegeManager>;
