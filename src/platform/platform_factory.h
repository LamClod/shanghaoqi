#pragma once
#include "interfaces/i_cert_manager.h"
#include "interfaces/i_hosts_manager.h"
#include "interfaces/i_privilege_manager.h"
#include <memory>

class PlatformFactory {
public:
    static std::unique_ptr<PlatformFactory> create();
    virtual ~PlatformFactory() = default;
    virtual ICertManagerPtr createCertManager() = 0;
    virtual IHostsManagerPtr createHostsManager() = 0;
    virtual IPrivilegeManagerPtr createPrivilegeManager() = 0;
};
