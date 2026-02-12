#include "platform_factory.h"

#ifdef Q_OS_WIN
#include "windows/win_cert_manager.h"
#include "windows/win_hosts_manager.h"
#include "windows/win_privilege_manager.h"

class WindowsPlatformFactory : public PlatformFactory {
public:
    ICertManagerPtr createCertManager() override {
        return std::make_unique<WinCertManager>();
    }
    IHostsManagerPtr createHostsManager() override {
        return std::make_unique<WinHostsManager>();
    }
    IPrivilegeManagerPtr createPrivilegeManager() override {
        return std::make_unique<WinPrivilegeManager>();
    }
};

std::unique_ptr<PlatformFactory> PlatformFactory::create() {
    return std::make_unique<WindowsPlatformFactory>();
}

#elif defined(Q_OS_MACOS)
#include "macos/mac_cert_manager.h"
#include "macos/mac_hosts_manager.h"
#include "macos/mac_privilege_manager.h"

class MacPlatformFactory : public PlatformFactory {
public:
    ICertManagerPtr createCertManager() override {
        return std::make_unique<MacCertManager>();
    }
    IHostsManagerPtr createHostsManager() override {
        return std::make_unique<MacHostsManager>();
    }
    IPrivilegeManagerPtr createPrivilegeManager() override {
        return std::make_unique<MacPrivilegeManager>();
    }
};

std::unique_ptr<PlatformFactory> PlatformFactory::create() {
    return std::make_unique<MacPlatformFactory>();
}

#else
#include "linux/linux_cert_manager.h"
#include "linux/linux_hosts_manager.h"
#include "linux/linux_privilege_manager.h"

class LinuxPlatformFactory : public PlatformFactory {
public:
    ICertManagerPtr createCertManager() override {
        return std::make_unique<LinuxCertManager>();
    }
    IHostsManagerPtr createHostsManager() override {
        return std::make_unique<LinuxHostsManager>();
    }
    IPrivilegeManagerPtr createPrivilegeManager() override {
        return std::make_unique<LinuxPrivilegeManager>();
    }
};

std::unique_ptr<PlatformFactory> PlatformFactory::create() {
    return std::make_unique<LinuxPlatformFactory>();
}
#endif
