/**
 * @file windows_cert_manager.h
 * @brief Windows 证书管理器头文件
 * 
 * 本文件定义了 Windows 平台的证书管理器实现类。
 * 
 * @details
 * 功能概述：
 * 
 * WindowsCertManager 实现了 ICertManager 接口，提供 Windows 平台的证书管理功能。
 * 使用 OpenSSL 生成证书，使用 Windows CryptoAPI 安装证书到系统存储。
 * 
 * 主要功能：
 * 
 * 1. 证书生成（使用 OpenSSL）
 *    - 生成 CA 根证书
 *    - 生成服务器证书
 *    - 支持多域名 SAN 扩展
 * 
 * 2. 证书安装（使用 CryptoAPI）
 *    - 安装 CA 证书到系统受信任的根证书颁发机构
 *    - 卸载已安装的 CA 证书
 * 
 * 3. 证书状态查询
 *    - 检查证书文件是否存在
 *    - 检查证书是否已安装到系统
 * 
 * 4. 证书信息获取
 *    - 获取证书指纹（SHA1）
 *    - 获取证书通用名称（CN）
 * 
 * @note 安装/卸载操作需要管理员权限
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef WINDOWS_CERT_MANAGER_H
#define WINDOWS_CERT_MANAGER_H

#include "../interfaces/i_cert_manager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

/**
 * @brief Windows 证书管理器实现
 * 
 * 同时继承 QObject 以支持 Qt 信号槽（用于 UI 集成）。
 * 
 * @note 安装/卸载操作需要管理员权限
 */
class WindowsCertManager : public QObject, public ICertManager {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * @param dataDir 证书文件存储目录
     * @param parent Qt 父对象
     */
    explicit WindowsCertManager(const QString& dataDir, QObject* parent = nullptr);
    ~WindowsCertManager() override = default;
    
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
     * 
     * 用于与 Qt UI 组件集成。
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
    
    // ========== Windows CryptoAPI 相关 ==========
    
    /**
     * @brief 读取证书文件内容
     */
    QByteArray readCertFile(const QString& certPath);
    
    /**
     * @brief 将 PEM 格式转换为 DER 格式
     */
    QByteArray pemToDer(const QByteArray& pemData);
    
    /**
     * @brief 安装证书到 Windows 证书存储
     */
    OperationResult installToWindowsStore(const QString& certPath);
    
    /**
     * @brief 从 Windows 证书存储卸载证书
     */
    OperationResult uninstallFromWindowsStore(const QString& thumbprint);
    
    /**
     * @brief 检查证书是否在 Windows 存储中
     */
    bool isCertInWindowsStore(const QString& thumbprint);
    
    // ========== 成员变量 ==========
    
    QString m_dataDir;              ///< 证书文件存储目录
    QString m_opensslPath;          ///< OpenSSL 可执行文件路径
    LogCallback m_logCallback;      ///< 日志回调
};

#endif // WINDOWS_CERT_MANAGER_H
