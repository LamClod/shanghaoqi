/**
 * @file linux_cert_manager.cpp
 * @brief Linux 证书管理器实现文件
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "linux_cert_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QRegularExpression>
#include <vector>

// ============================================================================
// 构造函数
// ============================================================================

LinuxCertManager::LinuxCertManager(const QString& dataDir, QObject* parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_opensslPath(findOpenSSLPath())
    , m_distroType(detectDistroType())
    , m_logCallback(nullptr)
{
}

// ============================================================================
// 日志辅助方法
// ============================================================================

void LinuxCertManager::emitLog(const std::string& message, LogLevel level)
{
    if (m_logCallback) {
        m_logCallback(message, level);
    }
    emit logMessage(QString::fromStdString(message), level);
}

void LinuxCertManager::setLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

// ============================================================================
// Linux 发行版检测
// ============================================================================

QString LinuxCertManager::detectDistroType() const
{
    // 检查 /etc/os-release 文件
    QFile osRelease("/etc/os-release");
    if (osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(osRelease.readAll());
        osRelease.close();
        
        // Debian 系列: Debian, Ubuntu, Linux Mint, etc.
        if (content.contains("debian", Qt::CaseInsensitive) ||
            content.contains("ubuntu", Qt::CaseInsensitive) ||
            content.contains("mint", Qt::CaseInsensitive)) {
            return "debian";
        }
        
        // RHEL 系列: RHEL, CentOS, Fedora, Rocky, Alma, etc.
        if (content.contains("rhel", Qt::CaseInsensitive) ||
            content.contains("centos", Qt::CaseInsensitive) ||
            content.contains("fedora", Qt::CaseInsensitive) ||
            content.contains("rocky", Qt::CaseInsensitive) ||
            content.contains("alma", Qt::CaseInsensitive)) {
            return "rhel";
        }
    }
    
    // 通过检查命令是否存在来判断
    if (QFile::exists("/usr/sbin/update-ca-certificates")) {
        return "debian";
    }
    if (QFile::exists("/usr/bin/update-ca-trust")) {
        return "rhel";
    }
    
    return "unknown";
}

QString LinuxCertManager::getSystemCertPath() const
{
    if (m_distroType == "debian") {
        return "/usr/local/share/ca-certificates/shanghaoqi-ca.crt";
    } else if (m_distroType == "rhel") {
        return "/etc/pki/ca-trust/source/anchors/shanghaoqi-ca.crt";
    }
    return "/usr/local/share/ca-certificates/shanghaoqi-ca.crt";
}

QString LinuxCertManager::getUpdateCertCommand() const
{
    if (m_distroType == "debian") {
        return "update-ca-certificates";
    } else if (m_distroType == "rhel") {
        return "update-ca-trust";
    }
    return "update-ca-certificates";
}

// ============================================================================
// OpenSSL 路径查找
// ============================================================================

QString LinuxCertManager::findOpenSSLPath() const
{
    // Linux 上 OpenSSL 通常在 PATH 中
    QProcess process;
    process.start("which", {"openssl"});
    process.waitForFinished(5000);
    
    if (process.exitCode() == 0) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    
    // 常见路径
    QStringList paths = {"/usr/bin/openssl", "/usr/local/bin/openssl", "/bin/openssl"};
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

std::string LinuxCertManager::caCertPath() const
{
    return (m_dataDir + "/ca.crt").toStdString();
}

std::string LinuxCertManager::caKeyPath() const
{
    return (m_dataDir + "/ca.key").toStdString();
}

std::string LinuxCertManager::serverCertPath() const
{
    return (m_dataDir + "/server.crt").toStdString();
}

std::string LinuxCertManager::serverKeyPath() const
{
    return (m_dataDir + "/server.key").toStdString();
}

// ============================================================================
// 状态查询
// ============================================================================

bool LinuxCertManager::caCertExists() const
{
    QString caPath = QString::fromStdString(caCertPath());
    QString keyPath = QString::fromStdString(caKeyPath());
    return QFile::exists(caPath) && QFile::exists(keyPath);
}

bool LinuxCertManager::serverCertExists() const
{
    QString certPath = QString::fromStdString(serverCertPath());
    QString keyPath = QString::fromStdString(serverKeyPath());
    return QFile::exists(certPath) && QFile::exists(keyPath);
}

bool LinuxCertManager::allCertsExist() const
{
    return caCertExists() && serverCertExists();
}

bool LinuxCertManager::isCACertInstalled()
{
    QString systemCertPath = getSystemCertPath();
    return QFile::exists(systemCertPath);
}

// ============================================================================
// OpenSSL 命令执行
// ============================================================================

bool LinuxCertManager::ensureDataDirExists()
{
    QDir dir(m_dataDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emitLog("无法创建数据目录: " + m_dataDir.toStdString(), LogLevel::Error);
            return false;
        }
        emitLog("创建数据目录: " + m_dataDir.toStdString(), LogLevel::Info);
    }
    return true;
}

OperationResult LinuxCertManager::runOpenSSL(const QStringList& args, const QString& errorMessage)
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
    
    QString stdOut = QString::fromUtf8(process.readAllStandardOutput());
    QString stdErr = QString::fromUtf8(process.readAllStandardError());
    
    if (process.exitCode() != 0) {
        if (stdErr.contains("Signature ok") && stdErr.contains("subject=")) {
            emitLog("证书签署成功", LogLevel::Info);
            return OperationResult::success();
        }
        
        std::string msg = errorMessage.toStdString() + "\n错误输出: " + stdErr.toStdString();
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    if (!stdOut.isEmpty()) {
        emitLog(stdOut.trimmed().toStdString(), LogLevel::Info);
    }
    
    return OperationResult::success();
}

// ============================================================================
// OpenSSL 配置文件创建
// ============================================================================

bool LinuxCertManager::createConfigFiles()
{
    const QString opensslCnf = R"([ req ]
default_bits        = 2048
default_md          = sha256
distinguished_name  = req_distinguished_name
attributes          = req_attributes

[ req_distinguished_name ]
countryName                 = Country Name (2 letter code)
countryName_min             = 2
countryName_max             = 2
stateOrProvinceName         = State or Province Name (full name)
localityName                = Locality Name (eg, city)
0.organizationName          = Organization Name (eg, company)
organizationalUnitName      = Organizational Unit Name (eg, section)
commonName                  = Common Name (eg, fully qualified host name)
commonName_max              = 64
emailAddress                = Email Address
emailAddress_max            = 64

[ req_attributes ]
challengePassword           = A challenge password
challengePassword_min       = 4
challengePassword_max       = 20
)";

    const QString v3CaCnf = R"([ v3_ca ]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer
basicConstraints = critical, CA:TRUE, pathlen:3
keyUsage = critical, cRLSign, keyCertSign
nsCertType = sslCA, emailCA
)";

    const QString v3ReqCnf = R"([ v3_req ]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
subjectAltName = @alt_names
)";

    QString domainCnf = "[ v3_req ]\n"
                        "basicConstraints = CA:FALSE\n"
                        "keyUsage = nonRepudiation, digitalSignature, keyEncipherment\n"
                        "subjectAltName = @alt_names\n\n"
                        "[alt_names]\n";
    
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
    configFiles["v3_req.cnf"] = v3ReqCnf;
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
            emitLog("创建配置文件: " + filePath.toStdString(), LogLevel::Info);
        }
    }

    return true;
}

// ============================================================================
// CA 证书生成
// ============================================================================

OperationResult LinuxCertManager::generateCAKey()
{
    emitLog("正在生成CA私钥 (ca.key)...", LogLevel::Info);
    
    QStringList args;
    args << "genrsa" << "-out" << QString::fromStdString(caKeyPath()) << "2048";
    
    auto result = runOpenSSL(args, "生成CA私钥失败");
    if (result.ok) {
        emitLog("CA私钥已生成: ca.key", LogLevel::Info);
    }
    return result;
}

OperationResult LinuxCertManager::generateCACert(const std::string& commonName)
{
    emitLog("开始生成CA证书...", LogLevel::Info);
    
    if (caCertExists()) {
        emitLog("CA证书已存在，跳过生成", LogLevel::Info);
        return OperationResult::success("CA证书已存在");
    }
    
    if (!ensureDataDirExists()) {
        return OperationResult::failure("无法创建数据目录", ErrorCode::CertGenerationFailed);
    }
    
    if (!createConfigFiles()) {
        return OperationResult::failure("无法创建配置文件", ErrorCode::CertGenerationFailed);
    }
    
    auto keyResult = generateCAKey();
    if (!keyResult.ok) {
        return keyResult;
    }
    
    emitLog("正在生成CA证书 (ca.crt)...", LogLevel::Info);
    
    QString opensslCnfPath = m_dataDir + "/openssl.cnf";
    QString v3CaCnfPath = m_dataDir + "/v3_ca.cnf";
    
    QFile opensslFile(opensslCnfPath);
    QFile v3CaFile(v3CaCnfPath);
    
    if (!opensslFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !v3CaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return OperationResult::failure("无法读取配置文件", ErrorCode::CertGenerationFailed);
    }
    
    QString combinedConfig = QString::fromUtf8(opensslFile.readAll()) + "\n" + 
                             QString::fromUtf8(v3CaFile.readAll());
    opensslFile.close();
    v3CaFile.close();
    
    QString tempConfigPath = m_dataDir + "/temp_ca.cnf";
    QFile tempFile(tempConfigPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::failure("无法创建临时配置文件", ErrorCode::CertGenerationFailed);
    }
    tempFile.write(combinedConfig.toUtf8());
    tempFile.close();
    
    QString subject = QString("/C=CN/ST=X/L=X/O=X/OU=X/CN=%1").arg(QString::fromStdString(commonName));
    emitLog("使用证书信息: " + subject.toStdString(), LogLevel::Info);
    
    QStringList args;
    args << "req" << "-new" << "-x509"
         << "-extensions" << "v3_ca"
         << "-days" << QString::number(CA_VALIDITY_DAYS)
         << "-key" << QString::fromStdString(caKeyPath())
         << "-out" << QString::fromStdString(caCertPath())
         << "-config" << tempConfigPath
         << "-subj" << subject;
    
    auto result = runOpenSSL(args, "生成CA证书失败");
    
    QFile::remove(tempConfigPath);
    
    if (result.ok) {
        emitLog("CA证书已生成: ca.crt", LogLevel::Info);
    }
    
    return result;
}

// ============================================================================
// 服务器证书生成
// ============================================================================

OperationResult LinuxCertManager::generateServerKey()
{
    emitLog("正在生成服务器私钥 (server.key)...", LogLevel::Info);
    
    QString keyPath = QString::fromStdString(serverKeyPath());
    
    QStringList args;
    args << "genrsa" << "-out" << keyPath << "2048";
    
    auto result = runOpenSSL(args, "生成私钥 server.key 失败");
    if (!result.ok) {
        return result;
    }
    
    emitLog("正在将私钥转换为PKCS#8格式...", LogLevel::Info);
    
    QString pk8Path = keyPath + ".pk8";
    QStringList pk8Args;
    pk8Args << "pkcs8" << "-topk8" << "-nocrypt"
            << "-in" << keyPath
            << "-out" << pk8Path;
    
    result = runOpenSSL(pk8Args, "将私钥转换为PKCS#8格式失败");
    if (!result.ok) {
        return result;
    }
    
    QFile::remove(keyPath);
    QFile::rename(pk8Path, keyPath);
    
    emitLog("服务器私钥 server.key 处理完成", LogLevel::Info);
    return OperationResult::success();
}

OperationResult LinuxCertManager::generateCSR()
{
    emitLog("正在生成证书签名请求 (CSR) server.csr...", LogLevel::Info);
    
    QString opensslCnfPath = m_dataDir + "/openssl.cnf";
    QString v3ReqCnfPath = m_dataDir + "/v3_req.cnf";
    QString domainCnfPath = m_dataDir + "/server.cnf";
    QString domainSubjPath = m_dataDir + "/server.subj";
    
    QFile opensslFile(opensslCnfPath);
    QFile v3ReqFile(v3ReqCnfPath);
    QFile domainFile(domainCnfPath);
    QFile subjFile(domainSubjPath);
    
    if (!opensslFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !v3ReqFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !domainFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !subjFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return OperationResult::failure("无法读取配置文件", ErrorCode::CertGenerationFailed);
    }
    
    QString combinedConfig = QString::fromUtf8(opensslFile.readAll()) + "\n" +
                             QString::fromUtf8(v3ReqFile.readAll()) + "\n" +
                             QString::fromUtf8(domainFile.readAll());
    QString subjectInfo = QString::fromUtf8(subjFile.readAll()).trimmed();
    
    opensslFile.close();
    v3ReqFile.close();
    domainFile.close();
    subjFile.close();
    
    if (subjectInfo.isEmpty()) {
        return OperationResult::failure("主题文件为空", ErrorCode::CertGenerationFailed);
    }
    
    emitLog("使用主题信息: " + subjectInfo.toStdString(), LogLevel::Info);
    
    QString tempConfigPath = m_dataDir + "/temp_server.cnf";
    QFile tempFile(tempConfigPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::failure("无法创建临时配置文件", ErrorCode::CertGenerationFailed);
    }
    tempFile.write(combinedConfig.toUtf8());
    tempFile.close();
    
    QString csrPath = m_dataDir + "/server.csr";
    
    QStringList args;
    args << "req" << "-reqexts" << "v3_req"
         << "-sha256" << "-new"
         << "-key" << QString::fromStdString(serverKeyPath())
         << "-out" << csrPath
         << "-config" << tempConfigPath
         << "-subj" << subjectInfo;
    
    auto result = runOpenSSL(args, "生成CSR server.csr 失败");
    
    if (result.ok) {
        emitLog("CSR server.csr 生成成功", LogLevel::Info);
    }
    
    return result;
}

OperationResult LinuxCertManager::signServerCert()
{
    emitLog("正在使用CA签署证书 server.crt...", LogLevel::Info);
    
    QString csrPath = m_dataDir + "/server.csr";
    QString tempConfigPath = m_dataDir + "/temp_server.cnf";
    QString serialPath = m_dataDir + "/ca.srl";
    
    if (!QFile::exists(serialPath)) {
        QFile serialFile(serialPath);
        if (serialFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            serialFile.write("01");
            serialFile.close();
        }
    }
    
    QStringList args;
    args << "x509" << "-req"
         << "-extensions" << "v3_req"
         << "-days" << QString::number(SERVER_VALIDITY_DAYS)
         << "-sha256"
         << "-in" << csrPath
         << "-CA" << QString::fromStdString(caCertPath())
         << "-CAkey" << QString::fromStdString(caKeyPath())
         << "-CAserial" << serialPath
         << "-out" << QString::fromStdString(serverCertPath())
         << "-extfile" << tempConfigPath;
    
    auto result = runOpenSSL(args, "签署证书 server.crt 失败");
    
    QFile::remove(tempConfigPath);
    QFile::remove(csrPath);
    
    if (!result.ok) {
        return result;
    }
    
    QString certPath = QString::fromStdString(serverCertPath());
    QFileInfo certInfo(certPath);
    if (!certInfo.exists()) {
        return OperationResult::failure(
            "证书文件 " + certPath.toStdString() + " 未生成",
            ErrorCode::CertGenerationFailed
        );
    }
    
    if (certInfo.size() == 0) {
        return OperationResult::failure(
            "证书文件 " + certPath.toStdString() + " 为空文件",
            ErrorCode::CertGenerationFailed
        );
    }
    
    emitLog("证书 server.crt 生成成功 (大小: " + std::to_string(certInfo.size()) + " bytes)", LogLevel::Info);
    
    return OperationResult::success();
}

OperationResult LinuxCertManager::generateServerCert()
{
    emitLog("开始为多域名生成服务器证书...", LogLevel::Info);
    
    if (serverCertExists()) {
        emitLog("服务器证书已存在，跳过生成", LogLevel::Info);
        return OperationResult::success("服务器证书已存在");
    }
    
    if (!caCertExists()) {
        return OperationResult::failure(
            "CA证书不存在，请先生成CA证书",
            ErrorCode::CertGenerationFailed
        );
    }
    
    if (!ensureDataDirExists()) {
        return OperationResult::failure("无法创建数据目录", ErrorCode::CertGenerationFailed);
    }
    
    if (!createConfigFiles()) {
        return OperationResult::failure("无法创建配置文件", ErrorCode::CertGenerationFailed);
    }
    
    auto keyResult = generateServerKey();
    if (!keyResult.ok) {
        return keyResult;
    }
    
    auto csrResult = generateCSR();
    if (!csrResult.ok) {
        return csrResult;
    }
    
    auto signResult = signServerCert();
    if (!signResult.ok) {
        return signResult;
    }
    
    emitLog("=== 服务器证书生成完成 ===", LogLevel::Info);
    return OperationResult::success();
}

OperationResult LinuxCertManager::generateAllCerts(const std::string& commonName)
{
    emitLog("============================================================", LogLevel::Info);
    emitLog("证书管理器 - 一键生成CA证书和服务器证书", LogLevel::Info);
    emitLog("============================================================", LogLevel::Info);
    
    QProcess process;
    process.setProgram(m_opensslPath);
    process.setArguments({"version"});
    process.start();
    
    if (!process.waitForFinished(5000)) {
        std::string msg = "OpenSSL命令执行失败。请确保OpenSSL已安装。";
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    if (process.exitCode() == 0) {
        QString version = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        emitLog("检测到OpenSSL: " + version.toStdString(), LogLevel::Info);
    } else {
        std::string msg = "未找到OpenSSL。请安装OpenSSL: sudo apt install openssl";
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    auto caResult = generateCACert(commonName);
    if (!caResult.ok) {
        return caResult;
    }
    
    auto serverResult = generateServerCert();
    if (!serverResult.ok) {
        return serverResult;
    }
    
    emitLog("============================================================", LogLevel::Info);
    emitLog("证书生成完成！", LogLevel::Info);
    emitLog("============================================================", LogLevel::Info);
    emitLog("CA 证书: " + caCertPath(), LogLevel::Info);
    emitLog("CA 私钥: " + caKeyPath() + " (请妥善保管)", LogLevel::Info);
    emitLog("服务器证书: " + serverCertPath(), LogLevel::Info);
    emitLog("服务器私钥: " + serverKeyPath() + " (请妥善保管)", LogLevel::Info);
    
    return OperationResult::success("所有证书生成完成");
}

// ============================================================================
// 证书信息获取
// ============================================================================

std::string LinuxCertManager::getCertThumbprint(const std::string& certPath)
{
    QProcess process;
    process.setProgram(m_opensslPath);
    process.setArguments({
        "x509", "-in", QString::fromStdString(certPath),
        "-noout", "-fingerprint", "-sha1"
    });
    process.start();
    process.waitForFinished(5000);
    
    if (process.exitCode() != 0) {
        return std::string();
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    // 输出格式: SHA1 Fingerprint=XX:XX:XX:...
    int pos = output.indexOf('=');
    if (pos != -1) {
        QString fingerprint = output.mid(pos + 1);
        fingerprint.remove(':');
        return fingerprint.toStdString();
    }
    
    return std::string();
}

std::string LinuxCertManager::getCertCommonName(const std::string& certPath)
{
    QProcess process;
    process.setProgram(m_opensslPath);
    process.setArguments({
        "x509", "-in", QString::fromStdString(certPath),
        "-noout", "-subject"
    });
    process.start();
    process.waitForFinished(5000);
    
    if (process.exitCode() != 0) {
        return std::string();
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    // 输出格式: subject=CN = xxx, ...
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

OperationResult LinuxCertManager::installCACert()
{
    emitLog("正在安装CA证书到系统信任存储...", LogLevel::Info);
    
    if (!caCertExists()) {
        return OperationResult::failure(
            "CA证书不存在，请先生成CA证书",
            ErrorCode::CertInstallFailed
        );
    }
    
    if (isCACertInstalled()) {
        emitLog("CA证书已安装，跳过安装", LogLevel::Info);
        return OperationResult::success("CA证书已安装");
    }
    
    QString systemCertPath = getSystemCertPath();
    QString updateCommand = getUpdateCertCommand();
    
    emitLog("检测到发行版类型: " + m_distroType.toStdString(), LogLevel::Info);
    emitLog("证书安装路径: " + systemCertPath.toStdString(), LogLevel::Info);
    
    // 复制证书文件到系统目录（需要 root 权限）
    QProcess copyProcess;
    copyProcess.start("sudo", {"cp", QString::fromStdString(caCertPath()), systemCertPath});
    copyProcess.waitForFinished(30000);
    
    if (copyProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(copyProcess.readAllStandardError());
        return OperationResult::failure(
            "复制证书失败，需要 root 权限: " + error.toStdString(),
            ErrorCode::CertInstallFailed,
            {{"needsElevation", "true"}}
        );
    }
    
    // 更新系统证书存储
    QProcess updateProcess;
    updateProcess.start("sudo", {updateCommand});
    updateProcess.waitForFinished(60000);
    
    if (updateProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(updateProcess.readAllStandardError());
        return OperationResult::failure(
            "更新证书存储失败: " + error.toStdString(),
            ErrorCode::CertInstallFailed
        );
    }
    
    emitLog("CA证书安装成功", LogLevel::Info);
    return OperationResult::success("CA证书安装成功");
}

OperationResult LinuxCertManager::uninstallCACert()
{
    emitLog("正在从系统信任存储卸载CA证书...", LogLevel::Info);
    
    QString systemCertPath = getSystemCertPath();
    QString updateCommand = getUpdateCertCommand();
    
    if (!QFile::exists(systemCertPath)) {
        emitLog("CA证书未安装，无需卸载", LogLevel::Info);
        return OperationResult::success("CA证书未安装");
    }
    
    // 删除证书文件（需要 root 权限）
    QProcess removeProcess;
    removeProcess.start("sudo", {"rm", "-f", systemCertPath});
    removeProcess.waitForFinished(30000);
    
    if (removeProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(removeProcess.readAllStandardError());
        return OperationResult::failure(
            "删除证书失败，需要 root 权限: " + error.toStdString(),
            ErrorCode::CertInstallFailed,
            {{"needsElevation", "true"}}
        );
    }
    
    // 更新系统证书存储
    QProcess updateProcess;
    updateProcess.start("sudo", {updateCommand});
    updateProcess.waitForFinished(60000);
    
    if (updateProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(updateProcess.readAllStandardError());
        return OperationResult::failure(
            "更新证书存储失败: " + error.toStdString(),
            ErrorCode::CertInstallFailed
        );
    }
    
    emitLog("CA证书卸载成功", LogLevel::Info);
    return OperationResult::success("CA证书卸载成功");
}
