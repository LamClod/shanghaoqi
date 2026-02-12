#include "mac_cert_manager.h"
#include "core/log_manager.h"

#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSslCertificate>
#include <QTemporaryFile>
#include <QTextStream>
#include <QRegularExpression>

bool MacCertManager::runProcess(const QString& program, const QStringList& args, int timeoutMs) const
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(5000)) {
        LOG_ERROR(QString("Failed to start process: %1").arg(program));
        return false;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        LOG_ERROR(QString("Process timed out: %1 %2").arg(program, args.join(' ')));
        proc.kill();
        return false;
    }
    if (proc.exitCode() != 0) {
        QString stdErr = QString::fromLocal8Bit(proc.readAllStandardError());
        LOG_ERROR(QString("Process failed (exit %1): %2 %3 | stderr: %4")
                      .arg(proc.exitCode())
                      .arg(program, args.join(' '), stdErr));
        return false;
    }
    return true;
}

QString MacCertManager::runProcessOutput(const QString& program, const QStringList& args, int timeoutMs) const
{
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(5000)) {
        LOG_ERROR(QString("Failed to start process: %1").arg(program));
        return QString();
    }
    if (!proc.waitForFinished(timeoutMs)) {
        LOG_ERROR(QString("Process timed out: %1 %2").arg(program, args.join(' ')));
        proc.kill();
        return QString();
    }
    return QString::fromLocal8Bit(proc.readAllStandardOutput());
}

bool MacCertManager::generateCaCert(const QString& certPath, const QString& keyPath)
{
    LOG_INFO(QString("Generating CA certificate: cert=%1, key=%2").arg(certPath, keyPath));

    QFileInfo certInfo(certPath);
    QDir().mkpath(certInfo.absolutePath());

    QFileInfo keyInfo(keyPath);
    QDir().mkpath(keyInfo.absolutePath());

    QStringList args = {
        "req", "-x509", "-new", "-nodes",
        "-newkey", "rsa:2048",
        "-keyout", keyPath,
        "-out", certPath,
        "-days", "3650",
        "-subj", "/CN=ShangHaoQi CA/O=ShangHaoQi/OU=Development"
    };

    if (!runProcess("openssl", args)) {
        LOG_ERROR("Failed to generate CA certificate with openssl");
        return false;
    }

    if (!QFile::exists(certPath) || !QFile::exists(keyPath)) {
        LOG_ERROR("CA certificate files were not created");
        return false;
    }

    LOG_INFO("CA certificate generated successfully");
    return true;
}

bool MacCertManager::generateServerCertForDomains(const QString& caCertPath,
                                                  const QString& caKeyPath,
                                                  const QStringList& domains,
                                                  const QString& outCertPath,
                                                  const QString& outKeyPath)
{
    QStringList normalizedDomains;
    for (const QString& domain : domains) {
        const QString d = domain.trimmed();
        if (!d.isEmpty() && !normalizedDomains.contains(d, Qt::CaseInsensitive)) {
            normalizedDomains.append(d);
        }
    }

    if (normalizedDomains.isEmpty()) {
        LOG_ERROR("Cannot generate server certificate: no valid domains provided");
        return false;
    }

    const QString primaryDomain = normalizedDomains.first();
    LOG_INFO(QString("Generating multi-domain server certificate; primary=%1, SAN count=%2")
                 .arg(primaryDomain)
                 .arg(normalizedDomains.size()));

    QFileInfo outCertInfo(outCertPath);
    QDir().mkpath(outCertInfo.absolutePath());

    QFileInfo outKeyInfo(outKeyPath);
    QDir().mkpath(outKeyInfo.absolutePath());

    QStringList keyArgs = {
        "genrsa", "-out", outKeyPath, "2048"
    };
    if (!runProcess("openssl", keyArgs)) {
        LOG_ERROR("Failed to generate server private key");
        return false;
    }

    QTemporaryFile sanConfig;
    sanConfig.setAutoRemove(false);
    if (!sanConfig.open()) {
        LOG_ERROR("Failed to create temporary SAN config file");
        return false;
    }

    const QString sanConfigPath = sanConfig.fileName();
    {
        QTextStream out(&sanConfig);
        out << "[req]\n"
            << "default_bits = 2048\n"
            << "prompt = no\n"
            << "default_md = sha256\n"
            << "distinguished_name = dn\n"
            << "req_extensions = v3_req\n"
            << "\n"
            << "[dn]\n"
            << "CN = " << primaryDomain << "\n"
            << "O = ShangHaoQi\n"
            << "OU = Development\n"
            << "\n"
            << "[v3_req]\n"
            << "subjectAltName = @alt_names\n"
            << "basicConstraints = CA:FALSE\n"
            << "keyUsage = digitalSignature, keyEncipherment\n"
            << "extendedKeyUsage = serverAuth\n"
            << "\n"
            << "[alt_names]\n";

        int sanIndex = 1;
        for (const QString& domain : normalizedDomains) {
            out << "DNS." << sanIndex++ << " = " << domain << "\n";
            if (!domain.startsWith("*.", Qt::CaseInsensitive)) {
                out << "DNS." << sanIndex++ << " = *." << domain << "\n";
            }
        }
    }
    sanConfig.close();

    const QString csrPath = outCertInfo.absolutePath() + "/temp_server.csr";
    QStringList csrArgs = {
        "req", "-new",
        "-key", outKeyPath,
        "-out", csrPath,
        "-config", sanConfigPath
    };
    if (!runProcess("openssl", csrArgs)) {
        LOG_ERROR("Failed to generate server CSR");
        QFile::remove(sanConfigPath);
        QFile::remove(csrPath);
        return false;
    }

    QTemporaryFile extFile;
    extFile.setAutoRemove(false);
    if (!extFile.open()) {
        LOG_ERROR("Failed to create temporary extension file");
        QFile::remove(sanConfigPath);
        QFile::remove(csrPath);
        return false;
    }

    const QString extPath = extFile.fileName();
    {
        QStringList sanEntries;
        for (const QString& domain : normalizedDomains) {
            sanEntries << QStringLiteral("DNS:%1").arg(domain);
            if (!domain.startsWith("*.", Qt::CaseInsensitive)) {
                sanEntries << QStringLiteral("DNS:*.%1").arg(domain);
            }
        }

        QTextStream out(&extFile);
        out << "subjectAltName = " << sanEntries.join(',') << "\n"
            << "basicConstraints = CA:FALSE\n"
            << "keyUsage = digitalSignature, keyEncipherment\n"
            << "extendedKeyUsage = serverAuth\n";
    }
    extFile.close();

    QStringList signArgs = {
        "x509", "-req",
        "-in", csrPath,
        "-CA", caCertPath,
        "-CAkey", caKeyPath,
        "-CAcreateserial",
        "-out", outCertPath,
        "-days", "365",
        "-sha256",
        "-extfile", extPath
    };
    bool signOk = runProcess("openssl", signArgs);

    QFile::remove(sanConfigPath);
    QFile::remove(csrPath);
    QFile::remove(extPath);

    QString srlPath = caCertPath;
    srlPath.replace(QRegularExpression("\\.pem$|\\.crt$"), ".srl");
    if (srlPath != caCertPath) {
        QFile::remove(srlPath);
    }

    if (!signOk) {
        LOG_ERROR("Failed to sign multi-domain server certificate with CA");
        return false;
    }

    if (!QFile::exists(outCertPath) || !QFile::exists(outKeyPath)) {
        LOG_ERROR("Server certificate files were not created");
        return false;
    }

    LOG_INFO(QString("Server certificate generated successfully for %1 domains")
                 .arg(normalizedDomains.size()));
    return true;
}

bool MacCertManager::generateServerCert(const QString& caCertPath, const QString& caKeyPath,
                                         const QString& domain,
                                         const QString& outCertPath, const QString& outKeyPath)
{
    return generateServerCertForDomains(caCertPath,
                                        caKeyPath,
                                        QStringList{domain},
                                        outCertPath,
                                        outKeyPath);
}

bool MacCertManager::installCaCert(const QString& certPath)
{
    LOG_INFO(QString("Installing CA certificate to macOS System Keychain: %1").arg(certPath));

    if (!QFile::exists(certPath)) {
        LOG_ERROR(QString("Certificate file does not exist: %1").arg(certPath));
        return false;
    }

    QStringList args = {
        "add-trusted-cert",
        "-d",
        "-r", "trustRoot",
        "-k", "/Library/Keychains/System.keychain",
        certPath
    };

    if (!runProcess("security", args)) {
        LOG_ERROR("Failed to install CA certificate to System Keychain. Root privileges may be required.");
        return false;
    }

    LOG_INFO("CA certificate installed to macOS System Keychain successfully");
    return true;
}

bool MacCertManager::uninstallCaCert(const QString& certPath)
{
    LOG_INFO(QString("Uninstalling CA certificate from macOS System Keychain: %1").arg(certPath));

    if (!QFile::exists(certPath)) {
        LOG_ERROR(QString("Certificate file does not exist: %1").arg(certPath));
        return false;
    }

    QStringList args = {
        "remove-trusted-cert",
        "-d",
        certPath
    };

    if (!runProcess("security", args)) {
        LOG_ERROR("Failed to uninstall CA certificate from System Keychain. Root privileges may be required.");
        return false;
    }

    LOG_INFO("CA certificate uninstalled from macOS System Keychain successfully");
    return true;
}

bool MacCertManager::isCaCertInstalled(const QString& certPath)
{
    LOG_DEBUG(QString("Checking if CA certificate is installed: %1").arg(certPath));

    Q_UNUSED(certPath)

    QStringList args = {
        "find-certificate",
        "-c", "ShangHaoQi CA",
        "/Library/Keychains/System.keychain"
    };

    QString output = runProcessOutput("security", args);

    bool found = !output.isEmpty() && output.contains("ShangHaoQi CA");
    LOG_DEBUG(QString("CA certificate installed: %1").arg(found ? "yes" : "no"));
    return found;
}

QString MacCertManager::getCertFingerprint(const QString& certPath)
{
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open certificate file: %1").arg(certPath));
        return QString();
    }

    QByteArray certData = certFile.readAll();
    certFile.close();

    QList<QSslCertificate> certs = QSslCertificate::fromData(certData, QSsl::Pem);
    if (certs.isEmpty()) {
        LOG_ERROR(QString("Failed to parse certificate: %1").arg(certPath));
        return QString();
    }

    QByteArray digest = certs.first().digest(QCryptographicHash::Sha256);
    QString fingerprint = digest.toHex(':').toUpper();

    LOG_DEBUG(QString("Certificate fingerprint (SHA256): %1").arg(fingerprint));
    return fingerprint;
}

QDateTime MacCertManager::getCertExpiry(const QString& certPath)
{
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open certificate file: %1").arg(certPath));
        return QDateTime();
    }

    QByteArray certData = certFile.readAll();
    certFile.close();

    QList<QSslCertificate> certs = QSslCertificate::fromData(certData, QSsl::Pem);
    if (certs.isEmpty()) {
        LOG_ERROR(QString("Failed to parse certificate: %1").arg(certPath));
        return QDateTime();
    }

    QDateTime expiry = certs.first().expiryDate();
    LOG_DEBUG(QString("Certificate expiry: %1").arg(expiry.toString(Qt::ISODate)));
    return expiry;
}
