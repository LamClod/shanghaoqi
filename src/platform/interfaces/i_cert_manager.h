#pragma once
#include <QString>
#include <QDateTime>
#include <memory>

class ICertManager {
public:
    virtual ~ICertManager() = default;
    virtual bool generateCaCert(const QString& certPath, const QString& keyPath) = 0;
    virtual bool generateServerCertForDomains(const QString& caCertPath, const QString& caKeyPath,
                                              const QStringList& domains,
                                              const QString& outCertPath, const QString& outKeyPath) = 0;
    virtual bool generateServerCert(const QString& caCertPath, const QString& caKeyPath,
                                     const QString& domain,
                                     const QString& outCertPath, const QString& outKeyPath) = 0;
    virtual bool installCaCert(const QString& certPath) = 0;
    virtual bool uninstallCaCert(const QString& certPath) = 0;
    virtual bool isCaCertInstalled(const QString& certPath) = 0;
    virtual QString getCertFingerprint(const QString& certPath) = 0;
    virtual QDateTime getCertExpiry(const QString& certPath) = 0;
};

using ICertManagerPtr = std::unique_ptr<ICertManager>;
