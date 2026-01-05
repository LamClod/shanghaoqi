/**
 * @file macos_cert_manager.cpp
 * @brief macOS 证书管理器实现文件
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "macos_cert_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QRegularExpression>

// ============================================================================
// 构造函数
// ============================================================================

MacOSCertManager::MacOSCertManager(const QString& dataDir, QObject* parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_opensslPath(findOpenSSLPath())
    , m_logCallback(nullptr)
{
}

void MacOSCertManager::emitLog(const std::string& message, LogLevel level)
{
    if (m_logCallback) m_logCallback(message, level);
    emit logMessage(QString::fromStdString(message), level);
}

void MacOSCertManager::setLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

// ============================================================================
// OpenSSL 路径查找
// ============================================================================

QString MacOSCertManager::findOpenSSLPath() const
{
    // macOS 上检查 Homebrew 安装的 OpenSSL
    QStringList paths = {
        "/usr/local/opt/openssl/bin/openssl",  // Intel Mac Homebrew
        "/opt/homebrew/opt/openssl/bin/openssl", // Apple Silicon Homebrew
        "/usr/bin/openssl",
        "/usr/local/bin/openssl"
    };
    
    for (const QString& path : paths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return "openssl";
}

// ============================================================================
// 路径获取
// ============================================================================

std::string MacOSCertManager::caCertPath() const { return (m_dataDir + "/ca.crt").toStdString(); }
std::string MacOSCertManager::caKeyPath() const { return (m_dataDir + "/ca.key").toStdString(); }
std::string MacOSCertManager::serverCertPath() const { return (m_dataDir + "/server.crt").toStdString(); }
std::string MacOSCertManager::serverKeyPath() const { return (m_dataDir + "/server.key").toStdString(); }

// ============================================================================
// 状态查询
// ============================================================================

bool MacOSCertManager::caCertExists() const
{
    return QFile::exists(QString::fromStdString(caCertPath())) && 
           QFile::exists(QString::fromStdString(caKeyPath()));
}

bool MacOSCertManager::serverCertExists() const
{
    return QFile::exists(QString::fromStdString(serverCertPath())) && 
           QFile::exists(QString::fromStdString(serverKeyPath()));
}

bool MacOSCertManager::allCertsExist() const { return caCertExists() && serverCertExists(); }

bool MacOSCertManager::isCACertInstalled()
{
    if (!caCertExists()) return false;
    
    // 使用 security 命令检查证书是否在系统 Keychain 中
    QProcess process;
    process.start("security", {"find-certificate", "-c", "ShangHaoQi_CA", 
                               "/Library/Keychains/System.keychain"});
    process.waitForFinished(5000);
    
    return process.exitCode() == 0;
}

// ============================================================================
// OpenSSL 命令执行
// ============================================================================

bool MacOSCertManager::ensureDataDirExists()
{
    QDir dir(m_dataDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emitLog("无法创建数据目录: " + m_dataDir.toStdString(), LogLevel::Error);
            return false;
        }
    }
    return true;
}

OperationResult MacOSCertManager::runOpenSSL(const QStringList& args, const QString& errorMessage)
{
    QProcess process;
    process.setProgram(m_opensslPath);
    process.setArguments(args);
    process.setWorkingDirectory(m_dataDir);
    
    emitLog("执行命令: " + m_opensslPath.toStdString() + " " + args.join(" ").toStdString(), LogLevel::Info);
    
    process.start();
    if (!process.waitForStarted(5000)) {
        std::string msg = errorMessage.toStdString() + ": 无法启动OpenSSL进程";
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    if (!process.waitForFinished(30000)) {
        std::string msg = errorMessage.toStdString() + ": OpenSSL进程超时";
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    QString stdErr = QString::fromUtf8(process.readAllStandardError());
    
    if (process.exitCode() != 0) {
        if (stdErr.contains("Signature ok")) {
            return OperationResult::success();
        }
        std::string msg = errorMessage.toStdString() + "\n错误输出: " + stdErr.toStdString();
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    return OperationResult::success();
}

// ============================================================================
// 配置文件创建
// ============================================================================

bool MacOSCertManager::createConfigFiles()
{
    const QString opensslCnf = R"([ req ]
default_bits        = 2048
default_md          = sha256
distinguished_name  = req_distinguished_name
attributes          = req_attributes

[ req_distinguished_name ]
commonName                  = Common Name

[ req_attributes ]
challengePassword           = A challenge password
)";

    const QString v3CaCnf = R"([ v3_ca ]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer
basicConstraints = critical, CA:TRUE, pathlen:3
keyUsage = critical, cRLSign, keyCertSign
)";

    QString domainCnf = "[ v3_req ]\nbasicConstraints = CA:FALSE\n"
                        "keyUsage = nonRepudiation, digitalSignature, keyEncipherment\n"
                        "subjectAltName = @alt_names\n\n[alt_names]\n";
    
    int dnsIndex = 1;
    auto domains = hijackDomains();
    for (const std::string& domain : domains) {
        domainCnf += QString("DNS.%1 = %2\n").arg(dnsIndex++).arg(QString::fromStdString(domain));
    }

    QString firstDomain = domains.empty() ? "localhost" : QString::fromStdString(domains.front());
    QString domainSubj = QString("/C=CN/ST=State/L=City/O=Organization/OU=Unit/CN=%1").arg(firstDomain);

    QMap<QString, QString> configFiles;
    configFiles["openssl.cnf"] = opensslCnf;
    configFiles["v3_ca.cnf"] = v3CaCnf;
    configFiles["server.cnf"] = domainCnf;
    configFiles["server.subj"] = domainSubj;

    for (auto it = configFiles.constBegin(); it != configFiles.constEnd(); ++it) {
        QString filePath = m_dataDir + "/" + it.key();
        if (!QFile::exists(filePath)) {
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                emitLog("无法创建配置文件: " + filePath.toStdString(), LogLevel::Error);
                return false;
            }
            QTextStream out(&file);
            out << it.value();
            file.close();
        }
    }
    return true;
}

// ============================================================================
// 证书生成
// ============================================================================

OperationResult MacOSCertManager::generateCAKey()
{
    emitLog("正在生成CA私钥...", LogLevel::Info);
    QStringList args = {"genrsa", "-out", QString::fromStdString(caKeyPath()), "2048"};
    return runOpenSSL(args, "生成CA私钥失败");
}

OperationResult MacOSCertManager::generateCACert(const std::string& commonName)
{
    if (caCertExists()) {
        emitLog("CA证书已存在，跳过生成", LogLevel::Info);
        return OperationResult::success("CA证书已存在");
    }
    
    if (!ensureDataDirExists() || !createConfigFiles()) {
        return OperationResult::failure("无法创建配置", ErrorCode::CertGenerationFailed);
    }
    
    auto keyResult = generateCAKey();
    if (!keyResult.ok) return keyResult;
    
    QString tempConfigPath = m_dataDir + "/temp_ca.cnf";
    QFile opensslFile(m_dataDir + "/openssl.cnf");
    QFile v3CaFile(m_dataDir + "/v3_ca.cnf");
    
    if (!opensslFile.open(QIODevice::ReadOnly) || !v3CaFile.open(QIODevice::ReadOnly)) {
        return OperationResult::failure("无法读取配置文件", ErrorCode::CertGenerationFailed);
    }
    
    QFile tempFile(tempConfigPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        return OperationResult::failure("无法创建临时配置", ErrorCode::CertGenerationFailed);
    }
    tempFile.write(opensslFile.readAll() + "\n" + v3CaFile.readAll());
    tempFile.close();
    opensslFile.close();
    v3CaFile.close();
    
    QString subject = QString("/C=CN/ST=X/L=X/O=X/OU=X/CN=%1").arg(QString::fromStdString(commonName));
    
    QStringList args = {"req", "-new", "-x509", "-extensions", "v3_ca",
                        "-days", QString::number(CA_VALIDITY_DAYS),
                        "-key", QString::fromStdString(caKeyPath()),
                        "-out", QString::fromStdString(caCertPath()),
                        "-config", tempConfigPath, "-subj", subject};
    
    auto result = runOpenSSL(args, "生成CA证书失败");
    QFile::remove(tempConfigPath);
    
    if (result.ok) emitLog("CA证书已生成", LogLevel::Info);
    return result;
}

OperationResult MacOSCertManager::generateServerKey()
{
    QString keyPath = QString::fromStdString(serverKeyPath());
    QStringList args = {"genrsa", "-out", keyPath, "2048"};
    auto result = runOpenSSL(args, "生成服务器私钥失败");
    if (!result.ok) return result;
    
    // 转换为 PKCS#8 格式
    QString pk8Path = keyPath + ".pk8";
    QStringList pk8Args = {"pkcs8", "-topk8", "-nocrypt", "-in", keyPath, "-out", pk8Path};
    result = runOpenSSL(pk8Args, "转换私钥格式失败");
    if (!result.ok) return result;
    
    QFile::remove(keyPath);
    QFile::rename(pk8Path, keyPath);
    return OperationResult::success();
}

OperationResult MacOSCertManager::generateCSR()
{
    QString tempConfigPath = m_dataDir + "/temp_server.cnf";
    QFile opensslFile(m_dataDir + "/openssl.cnf");
    QFile domainFile(m_dataDir + "/server.cnf");
    QFile subjFile(m_dataDir + "/server.subj");
    
    if (!opensslFile.open(QIODevice::ReadOnly) || !domainFile.open(QIODevice::ReadOnly) ||
        !subjFile.open(QIODevice::ReadOnly)) {
        return OperationResult::failure("无法读取配置文件", ErrorCode::CertGenerationFailed);
    }
    
    QString subjectInfo = QString::fromUtf8(subjFile.readAll()).trimmed();
    
    QFile tempFile(tempConfigPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        return OperationResult::failure("无法创建临时配置", ErrorCode::CertGenerationFailed);
    }
    tempFile.write(opensslFile.readAll() + "\n" + domainFile.readAll());
    tempFile.close();
    opensslFile.close();
    domainFile.close();
    subjFile.close();
    
    QString csrPath = m_dataDir + "/server.csr";
    QStringList args = {"req", "-reqexts", "v3_req", "-sha256", "-new",
                        "-key", QString::fromStdString(serverKeyPath()),
                        "-out", csrPath, "-config", tempConfigPath, "-subj", subjectInfo};
    
    return runOpenSSL(args, "生成CSR失败");
}

OperationResult MacOSCertManager::signServerCert()
{
    QString csrPath = m_dataDir + "/server.csr";
    QString tempConfigPath = m_dataDir + "/temp_server.cnf";
    QString serialPath = m_dataDir + "/ca.srl";
    
    if (!QFile::exists(serialPath)) {
        QFile serialFile(serialPath);
        if (serialFile.open(QIODevice::WriteOnly)) {
            serialFile.write("01");
            serialFile.close();
        }
    }
    
    QStringList args = {"x509", "-req", "-extensions", "v3_req",
                        "-days", QString::number(SERVER_VALIDITY_DAYS), "-sha256",
                        "-in", csrPath,
                        "-CA", QString::fromStdString(caCertPath()),
                        "-CAkey", QString::fromStdString(caKeyPath()),
                        "-CAserial", serialPath,
                        "-out", QString::fromStdString(serverCertPath()),
                        "-extfile", tempConfigPath};
    
    auto result = runOpenSSL(args, "签署证书失败");
    QFile::remove(tempConfigPath);
    QFile::remove(csrPath);
    return result;
}

OperationResult MacOSCertManager::generateServerCert()
{
    if (serverCertExists()) {
        emitLog("服务器证书已存在，跳过生成", LogLevel::Info);
        return OperationResult::success("服务器证书已存在");
    }
    
    if (!caCertExists()) {
        return OperationResult::failure("CA证书不存在", ErrorCode::CertGenerationFailed);
    }
    
    if (!ensureDataDirExists() || !createConfigFiles()) {
        return OperationResult::failure("无法创建配置", ErrorCode::CertGenerationFailed);
    }
    
    auto keyResult = generateServerKey();
    if (!keyResult.ok) return keyResult;
    
    auto csrResult = generateCSR();
    if (!csrResult.ok) return csrResult;
    
    auto signResult = signServerCert();
    if (!signResult.ok) return signResult;
    
    emitLog("服务器证书生成完成", LogLevel::Info);
    return OperationResult::success();
}

OperationResult MacOSCertManager::generateAllCerts(const std::string& commonName)
{
    emitLog("开始生成所有证书...", LogLevel::Info);
    
    QProcess process;
    process.start(m_opensslPath, {"version"});
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        return OperationResult::failure("未找到OpenSSL，请安装: brew install openssl", 
                                        ErrorCode::CertGenerationFailed);
    }
    
    auto caResult = generateCACert(commonName);
    if (!caResult.ok) return caResult;
    
    auto serverResult = generateServerCert();
    if (!serverResult.ok) return serverResult;
    
    emitLog("所有证书生成完成", LogLevel::Info);
    return OperationResult::success("所有证书生成完成");
}

// ============================================================================
// 证书信息
// ============================================================================

std::string MacOSCertManager::getCertThumbprint(const std::string& certPath)
{
    QProcess process;
    process.start(m_opensslPath, {"x509", "-in", QString::fromStdString(certPath),
                                  "-noout", "-fingerprint", "-sha1"});
    process.waitForFinished(5000);
    
    if (process.exitCode() != 0) return std::string();
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    int pos = output.indexOf('=');
    if (pos != -1) {
        QString fingerprint = output.mid(pos + 1);
        fingerprint.remove(':');
        return fingerprint.toStdString();
    }
    return std::string();
}

std::string MacOSCertManager::getCertCommonName(const std::string& certPath)
{
    QProcess process;
    process.start(m_opensslPath, {"x509", "-in", QString::fromStdString(certPath),
                                  "-noout", "-subject"});
    process.waitForFinished(5000);
    
    if (process.exitCode() != 0) return std::string();
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QRegularExpression regex("CN\\s*=\\s*([^,/]+)");
    auto match = regex.match(output);
    if (match.hasMatch()) {
        return match.captured(1).trimmed().toStdString();
    }
    return std::string();
}

// ============================================================================
// 证书安装/卸载
// ============================================================================

OperationResult MacOSCertManager::installCACert()
{
    emitLog("正在安装CA证书到系统Keychain...", LogLevel::Info);
    
    if (!caCertExists()) {
        return OperationResult::failure("CA证书不存在", ErrorCode::CertInstallFailed);
    }
    
    if (isCACertInstalled()) {
        emitLog("CA证书已安装，跳过", LogLevel::Info);
        return OperationResult::success("CA证书已安装");
    }
    
    QString certPath = QString::fromStdString(caCertPath());
    
    // 使用 osascript 请求管理员权限并安装证书
    QString script = QString(
        "do shell script \"security add-trusted-cert -d -r trustRoot "
        "-k /Library/Keychains/System.keychain '%1'\" "
        "with administrator privileges"
    ).arg(certPath);
    
    QProcess process;
    process.start("osascript", {"-e", script});
    process.waitForFinished(60000);
    
    if (process.exitCode() != 0) {
        QString error = QString::fromUtf8(process.readAllStandardError());
        return OperationResult::failure(
            "安装证书失败: " + error.toStdString(),
            ErrorCode::CertInstallFailed,
            {{"needsElevation", "true"}}
        );
    }
    
    emitLog("CA证书安装成功", LogLevel::Info);
    return OperationResult::success("CA证书安装成功");
}

OperationResult MacOSCertManager::uninstallCACert()
{
    emitLog("正在从系统Keychain卸载CA证书...", LogLevel::Info);
    
    if (!isCACertInstalled()) {
        emitLog("CA证书未安装，无需卸载", LogLevel::Info);
        return OperationResult::success("CA证书未安装");
    }
    
    // 使用 osascript 请求管理员权限并删除证书
    QString script = QString(
        "do shell script \"security delete-certificate -c 'ShangHaoQi_CA' "
        "/Library/Keychains/System.keychain\" "
        "with administrator privileges"
    );
    
    QProcess process;
    process.start("osascript", {"-e", script});
    process.waitForFinished(60000);
    
    if (process.exitCode() != 0) {
        QString error = QString::fromUtf8(process.readAllStandardError());
        return OperationResult::failure(
            "卸载证书失败: " + error.toStdString(),
            ErrorCode::CertInstallFailed,
            {{"needsElevation", "true"}}
        );
    }
    
    emitLog("CA证书卸载成功", LogLevel::Info);
    return OperationResult::success("CA证书卸载成功");
}
