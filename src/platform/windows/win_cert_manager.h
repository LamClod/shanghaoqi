#pragma once
#include "platform/interfaces/i_cert_manager.h"

class WinCertManager : public ICertManager {
public:
    bool generateCaCert(const QString& certPath, const QString& keyPath) override;
    bool generateServerCertForDomains(const QString& caCertPath, const QString& caKeyPath,
                                      const QStringList& domains,
                                      const QString& outCertPath, const QString& outKeyPath) override;
    bool generateServerCert(const QString& caCertPath, const QString& caKeyPath,
                             const QString& domain,
                             const QString& outCertPath, const QString& outKeyPath) override;
    bool installCaCert(const QString& certPath) override;
    bool uninstallCaCert(const QString& certPath) override;
    bool isCaCertInstalled(const QString& certPath) override;
    QString getCertFingerprint(const QString& certPath) override;
    QDateTime getCertExpiry(const QString& certPath) override;

private:
    QString findOpenSsl() const;
    bool runProcess(const QString& program, const QStringList& args, int timeoutMs = 30000) const;
    QString runProcessOutput(const QString& program, const QStringList& args, int timeoutMs = 30000) const;
};
