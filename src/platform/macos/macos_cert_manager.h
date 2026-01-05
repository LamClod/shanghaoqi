/**
 * @file macos_cert_manager.h
 * @brief macOS 证书管理器头文件
 * 
 * 本文件定义了 macOS 平台的证书管理器实现类。
 * 
 * @details
 * 功能概述：
 * 
 * MacOSCertManager 实现了 ICertManager 接口，提供 macOS 平台的证书管理功能。
 * 使用 OpenSSL 生成证书，使用 security 命令安装证书到系统 Keychain。
 * 
 * 主要功能：
 * 
 * 1. 证书生成（使用 OpenSSL）
 *    - 生成 CA 根证书
 *    - 生成服务器证书
 *    - 支持多域名 SAN 扩展
 * 
 * 2. 证书安装（使用 security 命令）
 *    - 安装 CA 证书到系统 Keychain
 *    - 设置证书信任策略
 * 
 * 3. 证书状态查询
 *    - 检查证书文件是否存在
 *    - 检查证书是否已安装到 Keychain
 * 
 * @note 安装/卸载操作需要管理员权限
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef MACOS_CERT_MANAGER_H
#define MACOS_CERT_MANAGER_H

#include "../interfaces/i_cert_manager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

/**
 * @brief macOS 证书管理器实现
 * 
 * @note 安装/卸载操作需要管理员权限
 */
class MacOSCertManager : public QObject, public ICertManager {
    Q_OBJECT
    
public:
    explicit MacOSCertManager(const QString& dataDir, QObject* parent = nullptr);
    ~MacOSCertManager() override = default;
    
    // ========== ICertManager 接口实现 ==========
    
    OperationResult generateCACert(const std::string& commonName = "ShangHaoQi_CA") override;
    OperationResult generateServerCert() override;
    OperationResult generateAllCerts(const std::string& commonName = "ShangHaoQi_CA") override;
    
    OperationResult installCACert() override;
    OperationResult uninstallCACert() override;
    
    [[nodiscard]] bool caCertExists() const override;
    [[nodiscard]] bool serverCertExists() const override;
    [[nodiscard]] bool allCertsExist() const override;
    bool isCACertInstalled() override;
    
    [[nodiscard]] std::string caCertPath() const override;
    [[nodiscard]] std::string caKeyPath() const override;
    [[nodiscard]] std::string serverCertPath() const override;
    [[nodiscard]] std::string serverKeyPath() const override;
    [[nodiscard]] std::string dataDir() const override { return m_dataDir.toStdString(); }
    
    std::string getCertThumbprint(const std::string& certPath) override;
    std::string getCertCommonName(const std::string& certPath) override;
    
    void setLogCallback(LogCallback callback) override;

signals:
    void logMessage(const QString& message, LogLevel level);

private:
    void emitLog(const std::string& message, LogLevel level);
    
    QString findOpenSSLPath() const;
    OperationResult runOpenSSL(const QStringList& args, const QString& errorMessage);
    bool ensureDataDirExists();
    bool createConfigFiles();
    
    OperationResult generateCAKey();
    OperationResult generateServerKey();
    OperationResult generateCSR();
    OperationResult signServerCert();
    
    QString m_dataDir;
    QString m_opensslPath;
    LogCallback m_logCallback;
};

#endif // MACOS_CERT_MANAGER_H
