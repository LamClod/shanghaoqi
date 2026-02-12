#include "win_cert_manager.h"
#include "core/log_manager.h"

#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSslCertificate>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTextStream>
#include <QRegularExpression>

QString WinCertManager::findOpenSsl() const
{
    QStringList candidates = {
        "openssl",
        "C:/Program Files/OpenSSL-Win64/bin/openssl.exe",
        "C:/Program Files/OpenSSL/bin/openssl.exe",
        "C:/Program Files (x86)/OpenSSL-Win32/bin/openssl.exe",
        QCoreApplication::applicationDirPath() + "/openssl.exe"
    };

    for (const auto& candidate : candidates) {
        QProcess proc;
        proc.start(candidate, {"version"});
        if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
            return candidate;
        }
    }

    LOG_WARNING("OpenSSL not found in common paths, falling back to PATH lookup");
    return "openssl";
}

bool WinCertManager::runProcess(const QString& program, const QStringList& args, int timeoutMs) const
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

QString WinCertManager::runProcessOutput(const QString& program, const QStringList& args, int timeoutMs) const
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

bool WinCertManager::generateCaCert(const QString& certPath, const QString& keyPath)
{
    LOG_INFO(QString("Generating CA certificate: cert=%1, key=%2").arg(certPath, keyPath));

    QFileInfo certInfo(certPath);
    QDir().mkpath(certInfo.absolutePath());

    QFileInfo keyInfo(keyPath);
    QDir().mkpath(keyInfo.absolutePath());

    QString openssl = findOpenSsl();

    QStringList args = {
        "req", "-x509", "-new", "-nodes",
        "-newkey", "rsa:2048",
        "-keyout", QDir::toNativeSeparators(keyPath),
        "-out", QDir::toNativeSeparators(certPath),
        "-days", "3650",
        "-subj", "/CN=ShangHaoQi CA/O=ShangHaoQi/OU=Development"
    };

    if (!runProcess(openssl, args)) {
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

bool WinCertManager::generateServerCertForDomains(const QString& caCertPath,
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

    const QString openssl = findOpenSsl();

    QStringList keyArgs = {
        "genrsa", "-out", QDir::toNativeSeparators(outKeyPath), "2048"
    };
    if (!runProcess(openssl, keyArgs)) {
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
        "-key", QDir::toNativeSeparators(outKeyPath),
        "-out", QDir::toNativeSeparators(csrPath),
        "-config", QDir::toNativeSeparators(sanConfigPath)
    };
    if (!runProcess(openssl, csrArgs)) {
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
        "-in", QDir::toNativeSeparators(csrPath),
        "-CA", QDir::toNativeSeparators(caCertPath),
        "-CAkey", QDir::toNativeSeparators(caKeyPath),
        "-CAcreateserial",
        "-out", QDir::toNativeSeparators(outCertPath),
        "-days", "365",
        "-sha256",
        "-extfile", QDir::toNativeSeparators(extPath)
    };
    const bool signOk = runProcess(openssl, signArgs);

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

bool WinCertManager::generateServerCert(const QString& caCertPath, const QString& caKeyPath,
                                         const QString& domain,
                                         const QString& outCertPath, const QString& outKeyPath)
{
    return generateServerCertForDomains(caCertPath,
                                        caKeyPath,
                                        QStringList{domain},
                                        outCertPath,
                                        outKeyPath);
}

bool WinCertManager::installCaCert(const QString& certPath)
{
    LOG_INFO(QString("Installing CA certificate to Windows Root store: %1").arg(certPath));

    if (!QFile::exists(certPath)) {
        LOG_ERROR(QString("Certificate file does not exist: %1").arg(certPath));
        return false;
    }

    QStringList args = {
        "-addstore", "Root", QDir::toNativeSeparators(certPath)
    };

    if (!runProcess("certutil", args)) {
        LOG_ERROR("Failed to install CA certificate (certutil -addstore). Administrator privileges may be required.");
        return false;
    }

    LOG_INFO("CA certificate installed to Windows Root store successfully");
    return true;
}

bool WinCertManager::uninstallCaCert(const QString& certPath)
{
    LOG_INFO(QString("Uninstalling CA certificate from Windows Root store: %1").arg(certPath));

    QString fingerprint = getCertFingerprint(certPath);
    if (fingerprint.isEmpty()) {
        LOG_ERROR("Cannot uninstall: failed to get certificate fingerprint");
        return false;
    }

    // certutil expects the hash without colons
    QString cleanFingerprint = fingerprint;
    cleanFingerprint.remove(':');

    QStringList args = {
        "-delstore", "Root", cleanFingerprint
    };

    if (!runProcess("certutil", args)) {
        LOG_ERROR("Failed to uninstall CA certificate (certutil -delstore). Administrator privileges may be required.");
        return false;
    }

    LOG_INFO("CA certificate uninstalled from Windows Root store successfully");
    return true;
}

bool WinCertManager::isCaCertInstalled(const QString& certPath)
{
    LOG_DEBUG(QString("Checking if CA certificate is installed: %1").arg(certPath));

    QString fingerprint = getCertFingerprint(certPath);
    if (fingerprint.isEmpty()) {
        LOG_WARNING("Cannot check installation: failed to get certificate fingerprint");
        return false;
    }

    QString output = runProcessOutput("certutil", {"-store", "Root"});
    if (output.isEmpty()) {
        LOG_WARNING("Failed to query Windows Root certificate store");
        return false;
    }

    // Normalize: remove colons from fingerprint and compare case-insensitively
    QString normalizedFingerprint = fingerprint;
    normalizedFingerprint.remove(':');
    normalizedFingerprint = normalizedFingerprint.toLower();

    QString normalizedOutput = output.toLower().remove(' ');

    bool found = normalizedOutput.contains(normalizedFingerprint);
    LOG_DEBUG(QString("CA certificate installed: %1").arg(found ? "yes" : "no"));
    return found;
}

QString WinCertManager::getCertFingerprint(const QString& certPath)
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

QDateTime WinCertManager::getCertExpiry(const QString& certPath)
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
