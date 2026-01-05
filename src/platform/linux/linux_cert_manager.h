/**
 * @file linux_cert_manager.h
 * @brief Linux 证书管理器头文件
 * 
 * 本文件定义了 Linux 平台的证书管理器实现类。
 * 
 * @details
 * 功能概述：
 * 
 * LinuxCertManager 实现了 ICertManager 接口，提供 Linux 平台的证书管理功能。
 * 使用 OpenSSL 生成证书，使用 update-ca-certificates 安装证书到系统存储。
 * 
 * 主要功能：
 * 
 * 1. 证书生成（使用 OpenSSL）
 *    - 生成 CA 根证书
 *    - 生成服务器证书
 *    - 支持多域名 SAN 扩展
 * 
 * 2. 证书安装
 *    - Debian/Ubuntu: 复制到 /usr/local/share/ca-certificates/ 并运行 update-ca-certificates
 *    - RHEL/CentOS/Fedora: 复制到 /etc/pki/ca-trust/source/anchors/ 并运行 update-ca-trust
 * 
 * 3. 证书状态查询
 *    - 检查证书文件是否存在
 *    - 检查证书是否已安装到系统
 * 
 * 4. 证书信息获取
 *    - 获取证书指纹（SHA1）
 *    - 获取证书通用名称（CN）
 * 
 * @note 安装/卸载操作需要 root 权限
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef LINUX_CERT_MANAGER_H
#define LINUX_CERT_MANAGER_H

#include "../interfaces/i_cert_manager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

/**
 * @brief Linux 证书管理器实现
 * 
 * 同时继承 QObject 以支持 Qt 信号槽（用于 UI 集成）。
 * 
 * @note 安装/卸载操作需要 root 权限
 */
class LinuxCertManager : public QObject, public ICertManager {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * @param dataDir 证书文件存储目录
     * @param parent Qt 父对象
     */
    explicit LinuxCertManager(const QString& dataDir, QObject* parent = nullptr);
    ~LinuxCertManager() override = default;
    
    // ========== ICertManager 接口实现 ==========
    
    // 证书生成
    OperationResult generateCACert(const std::string& commonName = "ShangHaoQi_CA") override;
    OperationResult generateServerCert() override;
    OperationResult generateAllCerts(const std::string& commonName = "ShangHaoQi_CA") override;
    
    // 证书安装/卸载
    OperationResult installCACert() override;
    OperationResult uninstallCACert() override;
    
    // 状态查询
    [[nodiscard]] bool caCertExists() const override;
    [[nodiscard]] bool serverCertExists() const override;
    [[nodiscard]] bool allCertsExist() const override;
    bool isCACertInstalled() override;
    
    // 路径获取
    [[nodiscard]] std::string caCertPath() const override;
    [[nodiscard]] std::string caKeyPath() const override;
    [[nodiscard]] std::string serverCertPath() const override;
    [[nodiscard]] std::string serverKeyPath() const override;
    [[nodiscard]] std::string dataDir() const override { return m_dataDir.toStdString(); }
    
    // 证书信息
    std::string getCertThumbprint(const std::string& certPath) override;
    std::string getCertCommonName(const std::string& certPath) override;
    
    // 回调设置
    void setLogCallback(LogCallback callback) override;

signals:
    /**
     * @brief Qt 信号：日志消息
     */
    void logMessage(const QString& message, LogLevel level);

private:
    // ========== 日志辅助 ==========
    
    void emitLog(const std::string& message, LogLevel level);
    
    // ========== OpenSSL 相关 ==========
    
    /**
     * @brief 查找 OpenSSL 可执行文件路径
     */
    QString findOpenSSLPath() const;
    
    /**
     * @brief 运行 OpenSSL 命令
     */
    OperationResult runOpenSSL(const QStringList& args, const QString& errorMessage);
    
    /**
     * @brief 确保数据目录存在
     */
    bool ensureDataDirExists();
    
    /**
     * @brief 创建 OpenSSL 配置文件
     */
    bool createConfigFiles();
    
    /**
     * @brief 生成 CA 私钥
     */
    OperationResult generateCAKey();
    
    /**
     * @brief 生成服务器私钥
     */
    OperationResult generateServerKey();
    
    /**
     * @brief 生成 CSR（证书签名请求）
     */
    OperationResult generateCSR();
    
    /**
     * @brief 使用 CA 签署服务器证书
     */
    OperationResult signServerCert();
    
    // ========== Linux 证书存储相关 ==========
    
    /**
     * @brief 检测 Linux 发行版类型
     * @return "debian", "rhel", 或 "unknown"
     */
    QString detectDistroType() const;
    
    /**
     * @brief 获取系统证书安装路径
     */
    QString getSystemCertPath() const;
    
    /**
     * @brief 获取更新证书的命令
     */
    QString getUpdateCertCommand() const;
    
    // ========== 成员变量 ==========
    
    QString m_dataDir;              ///< 证书文件存储目录
    QString m_opensslPath;          ///< OpenSSL 可执行文件路径
    QString m_distroType;           ///< Linux 发行版类型
    LogCallback m_logCallback;      ///< 日志回调
};

#endif // LINUX_CERT_MANAGER_H
