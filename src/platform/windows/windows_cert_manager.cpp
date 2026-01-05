/**
 * @file windows_cert_manager.cpp
 * @brief Windows 证书管理器实现文件
 * 
 * 本文件实现了 WindowsCertManager 类的所有方法。
 * 
 * @details
 * 实现模块：
 * 
 * 1. OpenSSL 相关
 *    - findOpenSSLPath(): 查找 OpenSSL 可执行文件
 *    - runOpenSSL(): 执行 OpenSSL 命令
 *    - createConfigFiles(): 创建 OpenSSL 配置文件
 *    - generateCAKey(): 生成 CA 私钥
 *    - generateServerKey(): 生成服务器私钥
 *    - generateCSR(): 生成证书签名请求
 *    - signServerCert(): 使用 CA 签署服务器证书
 * 
 * 2. Windows CryptoAPI 相关
 *    - readCertFile(): 读取证书文件
 *    - pemToDer(): PEM 格式转 DER 格式
 *    - installToWindowsStore(): 安装证书到系统存储
 *    - uninstallFromWindowsStore(): 从系统存储卸载证书
 *    - isCertInWindowsStore(): 检查证书是否在系统存储中
 *    - getCertThumbprint(): 获取证书指纹
 *    - getCertCommonName(): 获取证书通用名称
 * 
 * 3. 公共接口实现
 *    - generateCACert(): 生成 CA 证书
 *    - generateServerCert(): 生成服务器证书
 *    - generateAllCerts(): 一键生成所有证书
 *    - installCACert(): 安装 CA 证书
 *    - uninstallCACert(): 卸载 CA 证书
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "windows_cert_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QRegularExpression>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

// ============================================================================
// 构造函数
// ============================================================================

WindowsCertManager::WindowsCertManager(const QString& dataDir, QObject* parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_opensslPath(findOpenSSLPath())
    , m_logCallback(nullptr)
{
}

// ============================================================================
// 日志辅助方法
// ============================================================================

void WindowsCertManager::emitLog(const std::string& message, LogLevel level)
{
    // 调用纯 C++ 回调
    if (m_logCallback) {
        m_logCallback(message, level);
    }
    // 同时发送 Qt 信号（用于 UI 集成）
    emit logMessage(QString::fromStdString(message), level);
}

void WindowsCertManager::setLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

// ============================================================================
// OpenSSL 路径查找
// ============================================================================

QString WindowsCertManager::findOpenSSLPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    
#ifdef Q_OS_WIN
    const QString opensslBinary = "openssl.exe";
#else
    const QString opensslBinary = "openssl";
#endif
    
    // 1. 检查应用程序同目录
    QString sameDir = appDir + "/" + opensslBinary;
    if (QFile::exists(sameDir)) {
        return sameDir;
    }
    
    // 2. 检查应用程序目录下的openssl子目录
    QString localOpenSSL = appDir + "/openssl/" + opensslBinary;
    if (QFile::exists(localOpenSSL)) {
        return localOpenSSL;
    }
    
    // 3. 检查相对路径（开发环境）
    QString devOpenSSL = appDir + "/../openssl/" + opensslBinary;
    if (QFile::exists(devOpenSSL)) {
        return QFileInfo(devOpenSSL).absoluteFilePath();
    }
    
    // 4. 使用系统PATH中的openssl
    return "openssl";
}

// ============================================================================
// 路径获取（返回 std::string）
// ============================================================================

std::string WindowsCertManager::caCertPath() const
{
    return (m_dataDir + "/ca.crt").toStdString();
}

std::string WindowsCertManager::caKeyPath() const
{
    return (m_dataDir + "/ca.key").toStdString();
}

std::string WindowsCertManager::serverCertPath() const
{
    return (m_dataDir + "/server.crt").toStdString();
}

std::string WindowsCertManager::serverKeyPath() const
{
    return (m_dataDir + "/server.key").toStdString();
}

// ============================================================================
// 状态查询
// ============================================================================

bool WindowsCertManager::caCertExists() const
{
    QString caPath = QString::fromStdString(caCertPath());
    QString keyPath = QString::fromStdString(caKeyPath());
    return QFile::exists(caPath) && QFile::exists(keyPath);
}

bool WindowsCertManager::serverCertExists() const
{
    QString certPath = QString::fromStdString(serverCertPath());
    QString keyPath = QString::fromStdString(serverKeyPath());
    return QFile::exists(certPath) && QFile::exists(keyPath);
}

bool WindowsCertManager::allCertsExist() const
{
    return caCertExists() && serverCertExists();
}

// ============================================================================
// OpenSSL 命令执行
// ============================================================================

bool WindowsCertManager::ensureDataDirExists()
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

OperationResult WindowsCertManager::runOpenSSL(const QStringList& args, const QString& errorMessage)
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
        // 特殊处理：证书签署时如果stderr包含"Signature ok"，则认为成功
        if (stdErr.contains("Signature ok") && stdErr.contains("subject=")) {
            emitLog("证书签署成功（忽略序列号文件权限警告）", LogLevel::Info);
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

bool WindowsCertManager::createConfigFiles()
{
    // OpenSSL基础配置
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

    // CA扩展配置
    const QString v3CaCnf = R"([ v3_ca ]
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid:always,issuer
basicConstraints = critical, CA:TRUE, pathlen:3
keyUsage = critical, cRLSign, keyCertSign
nsCertType = sslCA, emailCA
)";

    // 服务器证书扩展配置
    const QString v3ReqCnf = R"([ v3_req ]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
subjectAltName = @alt_names
)";

    // 多域名配置 - 使用接口中定义的默认域名列表
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

    // 域名主题信息（使用第一个域名作为CN）
    QString firstDomain = domains.empty() ? "localhost" : QString::fromStdString(domains.front());
    QString domainSubj = QString("/C=CN/ST=State/L=City/O=Organization/OU=Unit/CN=%1").arg(firstDomain);

    // 配置文件映射
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

OperationResult WindowsCertManager::generateCAKey()
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

OperationResult WindowsCertManager::generateCACert(const std::string& commonName)
{
    emitLog("开始生成CA证书...", LogLevel::Info);
    
    // 如果CA证书已存在，跳过生成
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
    
    // 生成CA私钥
    auto keyResult = generateCAKey();
    if (!keyResult.ok) {
        return keyResult;
    }
    
    // 生成CA证书
    emitLog("正在生成CA证书 (ca.crt)...", LogLevel::Info);
    
    // 合并配置文件内容
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
    
    // 写入临时合并配置文件
    QString tempConfigPath = m_dataDir + "/temp_ca.cnf";
    QFile tempFile(tempConfigPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::failure("无法创建临时配置文件", ErrorCode::CertGenerationFailed);
    }
    tempFile.write(combinedConfig.toUtf8());
    tempFile.close();
    
    // 构建主题字符串
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
    
    // 清理临时文件
    QFile::remove(tempConfigPath);
    
    if (result.ok) {
        emitLog("CA证书已生成: ca.crt", LogLevel::Info);
    }
    
    return result;
}

// ============================================================================
// 服务器证书生成
// ============================================================================

OperationResult WindowsCertManager::generateServerKey()
{
    emitLog("正在生成服务器私钥 (server.key)...", LogLevel::Info);
    
    QString keyPath = QString::fromStdString(serverKeyPath());
    
    QStringList args;
    args << "genrsa" << "-out" << keyPath << "2048";
    
    auto result = runOpenSSL(args, "生成私钥 server.key 失败");
    if (!result.ok) {
        return result;
    }
    
    // 将私钥转换为PKCS#8格式
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
    
    // 删除原始私钥并重命名PKCS#8格式的私钥
    QFile::remove(keyPath);
    QFile::rename(pk8Path, keyPath);
    
    emitLog("服务器私钥 server.key 处理完成", LogLevel::Info);
    return OperationResult::success();
}

OperationResult WindowsCertManager::generateCSR()
{
    emitLog("正在生成证书签名请求 (CSR) server.csr...", LogLevel::Info);
    
    // 读取并合并配置文件
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
    
    // 写入临时合并配置文件
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

OperationResult WindowsCertManager::signServerCert()
{
    emitLog("正在使用CA签署证书 server.crt...", LogLevel::Info);
    
    QString csrPath = m_dataDir + "/server.csr";
    QString tempConfigPath = m_dataDir + "/temp_server.cnf";
    QString serialPath = m_dataDir + "/ca.srl";
    
    // 确保序列号文件存在
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
    
    // 清理临时文件
    QFile::remove(tempConfigPath);
    QFile::remove(csrPath);
    
    if (!result.ok) {
        return result;
    }
    
    // 验证证书文件是否实际生成且不为空
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

OperationResult WindowsCertManager::generateServerCert()
{
    emitLog("开始为多域名生成服务器证书...", LogLevel::Info);
    
    // 如果服务器证书已存在，跳过生成
    if (serverCertExists()) {
        emitLog("服务器证书已存在，跳过生成", LogLevel::Info);
        return OperationResult::success("服务器证书已存在");
    }
    
    // 检查CA证书是否存在
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
    
    // 生成服务器私钥
    auto keyResult = generateServerKey();
    if (!keyResult.ok) {
        return keyResult;
    }
    
    // 生成CSR
    auto csrResult = generateCSR();
    if (!csrResult.ok) {
        return csrResult;
    }
    
    // 使用CA签署证书
    auto signResult = signServerCert();
    if (!signResult.ok) {
        return signResult;
    }
    
    emitLog("=== 服务器证书生成完成 ===", LogLevel::Info);
    return OperationResult::success();
}

OperationResult WindowsCertManager::generateAllCerts(const std::string& commonName)
{
    emitLog("============================================================", LogLevel::Info);
    emitLog("证书管理器 - 一键生成CA证书和服务器证书", LogLevel::Info);
    emitLog("============================================================", LogLevel::Info);
    
    // 检查OpenSSL是否可用
    QProcess process;
    process.setProgram(m_opensslPath);
    process.setArguments({"version"});
    process.start();
    
    if (!process.waitForFinished(5000)) {
        std::string msg = "OpenSSL命令执行失败。请确保OpenSSL已安装并添加到PATH中。";
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    if (process.exitCode() == 0) {
        QString version = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        emitLog("检测到OpenSSL: " + version.toStdString(), LogLevel::Info);
    } else {
        std::string msg = "未找到OpenSSL。请安装OpenSSL并确保它在系统PATH中。";
        emitLog(msg, LogLevel::Error);
        return OperationResult::failure(msg, ErrorCode::CertGenerationFailed);
    }
    
    // 生成CA证书
    auto caResult = generateCACert(commonName);
    if (!caResult.ok) {
        return caResult;
    }
    
    // 生成服务器证书
    auto serverResult = generateServerCert();
    if (!serverResult.ok) {
        return serverResult;
    }
    
    // 输出结果摘要
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
// Windows CryptoAPI - 证书文件处理
// ============================================================================

QByteArray WindowsCertManager::readCertFile(const QString& certPath)
{
    QFile file(certPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emitLog("无法打开证书文件: " + certPath.toStdString(), LogLevel::Error);
        return QByteArray();
    }
    return file.readAll();
}

QByteArray WindowsCertManager::pemToDer(const QByteArray& pemData)
{
    QString pemStr = QString::fromUtf8(pemData);
    
    // 查找 PEM 头尾标记
    int beginPos = pemStr.indexOf("-----BEGIN CERTIFICATE-----");
    int endPos = pemStr.indexOf("-----END CERTIFICATE-----");
    
    if (beginPos == -1 || endPos == -1) {
        // 可能已经是 DER 格式，直接返回
        return pemData;
    }
    
    // 提取 Base64 编码的内容
    int contentStart = beginPos + QString("-----BEGIN CERTIFICATE-----").length();
    QString base64Content = pemStr.mid(contentStart, endPos - contentStart);
    
    // 移除所有空白字符
    base64Content.remove(QRegularExpression("\\s"));
    
    // Base64 解码得到 DER 格式
    return QByteArray::fromBase64(base64Content.toUtf8());
}

// ============================================================================
// Windows CryptoAPI - 证书信息获取（返回 std::string）
// ============================================================================

std::string WindowsCertManager::getCertThumbprint(const std::string& certPath)
{
#ifdef Q_OS_WIN
    QString qCertPath = QString::fromStdString(certPath);
    QByteArray certData = readCertFile(qCertPath);
    if (certData.isEmpty()) {
        return std::string();
    }
    
    QByteArray derData = pemToDer(certData);
    
    PCCERT_CONTEXT pCertContext = CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        reinterpret_cast<const BYTE*>(derData.constData()),
        static_cast<DWORD>(derData.size())
    );
    
    if (!pCertContext) {
        emitLog("无法解析证书: " + certPath, LogLevel::Error);
        return std::string();
    }
    
    BYTE thumbprint[20];
    DWORD thumbprintSize = sizeof(thumbprint);
    
    if (!CertGetCertificateContextProperty(
            pCertContext,
            CERT_SHA1_HASH_PROP_ID,
            thumbprint,
            &thumbprintSize)) {
        CertFreeCertificateContext(pCertContext);
        emitLog("无法获取证书指纹", LogLevel::Error);
        return std::string();
    }
    
    CertFreeCertificateContext(pCertContext);
    
    QString result;
    for (DWORD i = 0; i < thumbprintSize; i++) {
        result += QString("%1").arg(thumbprint[i], 2, 16, QChar('0')).toUpper();
    }
    
    return result.toStdString();
#else
    Q_UNUSED(certPath)
    return std::string();
#endif
}

std::string WindowsCertManager::getCertCommonName(const std::string& certPath)
{
#ifdef Q_OS_WIN
    QString qCertPath = QString::fromStdString(certPath);
    QByteArray certData = readCertFile(qCertPath);
    if (certData.isEmpty()) {
        return std::string();
    }
    
    QByteArray derData = pemToDer(certData);
    
    PCCERT_CONTEXT pCertContext = CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        reinterpret_cast<const BYTE*>(derData.constData()),
        static_cast<DWORD>(derData.size())
    );
    
    if (!pCertContext) {
        return std::string();
    }
    
    DWORD nameSize = CertGetNameStringW(
        pCertContext,
        CERT_NAME_SIMPLE_DISPLAY_TYPE,
        0,
        nullptr,
        nullptr,
        0
    );
    
    if (nameSize <= 1) {
        CertFreeCertificateContext(pCertContext);
        return std::string();
    }
    
    std::vector<wchar_t> nameBuffer(nameSize);
    CertGetNameStringW(
        pCertContext,
        CERT_NAME_SIMPLE_DISPLAY_TYPE,
        0,
        nullptr,
        nameBuffer.data(),
        nameSize
    );
    
    CertFreeCertificateContext(pCertContext);
    
    return QString::fromWCharArray(nameBuffer.data()).toStdString();
#else
    Q_UNUSED(certPath)
    return std::string();
#endif
}

// ============================================================================
// Windows CryptoAPI - 证书存储操作
// ============================================================================

bool WindowsCertManager::isCertInWindowsStore(const QString& thumbprint)
{
#ifdef Q_OS_WIN
    if (thumbprint.isEmpty()) {
        return false;
    }
    
    HCERTSTORE hStore = CertOpenSystemStoreW(0, L"ROOT");
    if (!hStore) {
        emitLog("无法打开系统证书存储", LogLevel::Error);
        return false;
    }
    
    QByteArray thumbprintBytes = QByteArray::fromHex(thumbprint.toUtf8());
    
    CRYPT_HASH_BLOB hashBlob;
    hashBlob.cbData = static_cast<DWORD>(thumbprintBytes.size());
    hashBlob.pbData = reinterpret_cast<BYTE*>(thumbprintBytes.data());
    
    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
        hStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_SHA1_HASH,
        &hashBlob,
        nullptr
    );
    
    bool found = (pCertContext != nullptr);
    
    if (pCertContext) {
        CertFreeCertificateContext(pCertContext);
    }
    CertCloseStore(hStore, 0);
    
    return found;
#else
    Q_UNUSED(thumbprint)
    return false;
#endif
}

bool WindowsCertManager::isCACertInstalled()
{
    if (!caCertExists()) {
        return false;
    }
    
    std::string thumbprint = getCertThumbprint(caCertPath());
    if (thumbprint.empty()) {
        return false;
    }
    
    return isCertInWindowsStore(QString::fromStdString(thumbprint));
}

OperationResult WindowsCertManager::installToWindowsStore(const QString& certPath)
{
#ifdef Q_OS_WIN
    emitLog("正在安装证书到 Windows 受信任的根证书颁发机构: " + certPath.toStdString(), LogLevel::Info);
    
    QByteArray certData = readCertFile(certPath);
    if (certData.isEmpty()) {
        return OperationResult::failure(
            "无法读取证书文件: " + certPath.toStdString(),
            ErrorCode::CertInstallFailed
        );
    }
    
    QByteArray derData = pemToDer(certData);
    
    PCCERT_CONTEXT pCertContext = CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        reinterpret_cast<const BYTE*>(derData.constData()),
        static_cast<DWORD>(derData.size())
    );
    
    if (!pCertContext) {
        DWORD error = GetLastError();
        return OperationResult::failure(
            "无法解析证书文件，错误码: " + std::to_string(error),
            ErrorCode::CertInstallFailed
        );
    }
    
    HCERTSTORE hStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM,
        0,
        0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE,
        L"ROOT"
    );
    
    if (!hStore) {
        DWORD error = GetLastError();
        CertFreeCertificateContext(pCertContext);
        
        if (error == ERROR_ACCESS_DENIED) {
            return OperationResult::failure(
                "需要管理员权限才能安装证书。请以管理员身份运行程序。",
                ErrorCode::CertInstallFailed,
                {{"needsElevation", "true"}}
            );
        }
        
        return OperationResult::failure(
            "无法打开系统证书存储，错误码: " + std::to_string(error),
            ErrorCode::CertInstallFailed
        );
    }
    
    BOOL result = CertAddCertificateContextToStore(
        hStore,
        pCertContext,
        CERT_STORE_ADD_REPLACE_EXISTING,
        nullptr
    );
    
    DWORD error = GetLastError();
    
    CertFreeCertificateContext(pCertContext);
    CertCloseStore(hStore, 0);
    
    if (!result) {
        if (error == ERROR_ACCESS_DENIED) {
            return OperationResult::failure(
                "需要管理员权限才能安装证书。请以管理员身份运行程序。",
                ErrorCode::CertInstallFailed,
                {{"needsElevation", "true"}}
            );
        }
        return OperationResult::failure(
            "安装证书失败，错误码: " + std::to_string(error),
            ErrorCode::CertInstallFailed
        );
    }
    
    emitLog("证书已成功安装到受信任的根证书颁发机构", LogLevel::Info);
    return OperationResult::success("证书安装成功");
#else
    Q_UNUSED(certPath)
    return OperationResult::failure(
        "证书安装仅支持 Windows 平台",
        ErrorCode::CertInstallFailed
    );
#endif
}

OperationResult WindowsCertManager::installCACert()
{
    if (!caCertExists()) {
        return OperationResult::failure(
            "CA证书不存在，请先生成证书",
            ErrorCode::CertNotFound
        );
    }
    
    std::string commonName = getCertCommonName(caCertPath());
    std::string thumbprint = getCertThumbprint(caCertPath());
    
    if (!commonName.empty()) {
        emitLog("证书通用名称: " + commonName, LogLevel::Info);
    }
    if (!thumbprint.empty()) {
        emitLog("证书指纹: " + thumbprint, LogLevel::Info);
    }
    
    // 检查是否已安装
    if (!thumbprint.empty() && isCertInWindowsStore(QString::fromStdString(thumbprint))) {
        emitLog("证书已安装，跳过安装步骤", LogLevel::Info);
        return OperationResult::success("证书已安装");
    }
    
    return installToWindowsStore(QString::fromStdString(caCertPath()));
}

OperationResult WindowsCertManager::uninstallFromWindowsStore(const QString& thumbprint)
{
#ifdef Q_OS_WIN
    if (thumbprint.isEmpty()) {
        return OperationResult::failure(
            "证书指纹为空",
            ErrorCode::CertInstallFailed
        );
    }
    
    emitLog("正在从系统存储卸载证书，指纹: " + thumbprint.toStdString(), LogLevel::Info);
    
    HCERTSTORE hStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM,
        0,
        0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE,
        L"ROOT"
    );
    
    if (!hStore) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            return OperationResult::failure(
                "需要管理员权限才能卸载证书。请以管理员身份运行程序。",
                ErrorCode::CertInstallFailed,
                {{"needsElevation", "true"}}
            );
        }
        return OperationResult::failure(
            "无法打开系统证书存储，错误码: " + std::to_string(error),
            ErrorCode::CertInstallFailed
        );
    }
    
    QByteArray thumbprintBytes = QByteArray::fromHex(thumbprint.toUtf8());
    
    CRYPT_HASH_BLOB hashBlob;
    hashBlob.cbData = static_cast<DWORD>(thumbprintBytes.size());
    hashBlob.pbData = reinterpret_cast<BYTE*>(thumbprintBytes.data());
    
    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
        hStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_SHA1_HASH,
        &hashBlob,
        nullptr
    );
    
    if (!pCertContext) {
        CertCloseStore(hStore, 0);
        emitLog("证书未在系统存储中找到", LogLevel::Warning);
        return OperationResult::success("证书未安装，无需卸载");
    }
    
    BOOL result = CertDeleteCertificateFromStore(pCertContext);
    
    CertCloseStore(hStore, 0);
    
    if (!result) {
        DWORD error = GetLastError();
        return OperationResult::failure(
            "卸载证书失败，错误码: " + std::to_string(error),
            ErrorCode::CertInstallFailed
        );
    }
    
    emitLog("证书已成功从系统存储卸载", LogLevel::Info);
    return OperationResult::success("证书卸载成功");
#else
    Q_UNUSED(thumbprint)
    return OperationResult::failure(
        "证书卸载仅支持 Windows 平台",
        ErrorCode::CertInstallFailed
    );
#endif
}

OperationResult WindowsCertManager::uninstallCACert()
{
    if (!caCertExists()) {
        return OperationResult::failure(
            "CA证书不存在",
            ErrorCode::CertNotFound
        );
    }
    
    std::string thumbprint = getCertThumbprint(caCertPath());
    if (thumbprint.empty()) {
        return OperationResult::failure(
            "无法获取证书指纹",
            ErrorCode::CertInstallFailed
        );
    }
    
    if (!isCertInWindowsStore(QString::fromStdString(thumbprint))) {
        emitLog("证书未安装，无需卸载", LogLevel::Info);
        return OperationResult::success("证书未安装");
    }
    
    return uninstallFromWindowsStore(QString::fromStdString(thumbprint));
}
