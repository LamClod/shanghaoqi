#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantList>

class ConfigStore;
class ProxyServer;
class ICertManager;
class IHostsManager;
class IPrivilegeManager;

class Bootstrap : public QObject {
    Q_OBJECT

public:
    explicit Bootstrap(QObject* parent = nullptr);

    void setConfig(ConfigStore* config) { m_config = config; }
    void setProxy(ProxyServer* proxy) { m_proxy = proxy; }
    void setCertManager(ICertManager* mgr) { m_certManager = mgr; }
    void setHostsManager(IHostsManager* mgr) { m_hostsManager = mgr; }
    void setPrivilegeManager(IPrivilegeManager* mgr) { m_privilegeManager = mgr; }

    void startAll();
    void stopAll();

    void generateCerts();
    void installCaCert();
    void modifyHosts();
    void restoreHosts();
    void startProxy();
    void stopProxy();

    void fetchModelList(int groupIndex);
    void testConfig(int groupIndex);
    void testAllConfigs();

    bool isProxyRunning() const;

signals:
    void stepProgress(const QString& step, bool success, const QString& message);
    void proxyStatusChanged(bool running);
    void modelListReady(int groupIndex, const QStringList& models);
    void testResult(int groupIndex, bool success, int httpStatus, const QString& error);
    void testAllDone(const QVariantList& results);

private:
    enum class CertAction { None, Generate, Reinstall, CleanAndRegen };
    CertAction decideCertAction();

    ConfigStore* m_config = nullptr;
    ProxyServer* m_proxy = nullptr;
    ICertManager* m_certManager = nullptr;
    IHostsManager* m_hostsManager = nullptr;
    IPrivilegeManager* m_privilegeManager = nullptr;
    bool m_hostsModified = false;
    QStringList m_hijackDomains;  // auto-derived from config groups at startAll()
};
