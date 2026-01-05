/**
 * @file i_cert_manager.h
 * @brief 证书管理器接口定义
 * 
 * 本文件定义了跨平台的证书管理抽象接口。
 * 
 * @details
 * 功能概述：
 * 
 * 证书管理器负责 SSL/TLS 证书的生成、安装和管理。
 * 各平台需要实现此接口以提供完整的证书管理能力。
 * 
 * 功能模块：
 * 
 * 1. 证书生成
 *    - CA 证书：自签名的根证书颁发机构证书
 *    - 服务器证书：由 CA 签署的服务器证书
 *    - 支持多域名 SAN 扩展
 * 
 * 2. 证书安装
 *    - 将 CA 证书安装到系统信任存储
 *    - 安装后由该 CA 签发的证书将被系统信任
 * 
 * 3. 证书查询
 *    - 检查证书是否存在
 *    - 检查证书是否已安装
 *    - 获取证书信息（指纹、通用名称等）
 * 
 * 平台实现差异：
 * 
 * | 平台    | 证书存储                    |
 * |---------|----------------------------|
 * | Windows | CryptoAPI 证书存储          |
 * | macOS   | Keychain                   |
 * | Linux   | ca-certificates / update-ca-trust |
 * 
 * @note 证书安装操作通常需要管理员/root 权限
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef I_CERT_MANAGER_H
#define I_CERT_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "../../core/operation_result.h"
#include "../../core/interfaces/i_log_manager.h"

/**
 * @brief 证书管理器接口
 * 
 * 纯抽象接口，定义证书生成和系统安装的标准操作。
 * 不依赖任何 GUI 框架。
 * 
 * 使用流程：
 * 1. 调用 generateAllCerts() 生成CA和服务器证书
 * 2. 调用 installCACert() 将CA证书安装到系统
 * 3. 使用 serverCertPath() 和 serverKeyPath() 获取服务器证书路径
 * 
 * @note 证书安装操作通常需要管理员/root 权限
 */
class ICertManager {
public:
    virtual ~ICertManager() = default;
    
    // 禁止拷贝
    ICertManager(const ICertManager&) = delete;
    ICertManager& operator=(const ICertManager&) = delete;
    
    // ========== 证书生成 ==========
    
    /**
     * @brief 生成CA证书
     * 
     * 生成自签名的CA（证书颁发机构）证书。
     * 如果CA证书已存在，跳过生成。
     * 
     * @param commonName CA证书的通用名称，默认为"ShangHaoQi_CA"
     * @return 操作结果
     */
    virtual OperationResult generateCACert(const std::string& commonName = "ShangHaoQi_CA") = 0;
    
    /**
     * @brief 生成服务器证书
     * 
     * 生成由CA签署的服务器证书，包含多域名SAN扩展。
     * 需要先生成CA证书。
     * 
     * @return 操作结果
     */
    virtual OperationResult generateServerCert() = 0;
    
    /**
     * @brief 一键生成所有证书
     * 
     * 依次生成CA证书和服务器证书。
     * 
     * @param commonName CA证书的通用名称
     * @return 操作结果
     */
    virtual OperationResult generateAllCerts(const std::string& commonName = "ShangHaoQi_CA") = 0;
    
    // ========== 证书安装/卸载 ==========
    
    /**
     * @brief 安装CA证书到系统信任存储
     * 
     * 将CA证书安装到系统的受信任根证书颁发机构存储。
     * 安装后，由该CA签发的证书将被系统信任。
     * 
     * @return 操作结果
     * 
     * @note 需要管理员权限
     * @note 如果证书已安装，跳过安装并返回成功
     */
    virtual OperationResult installCACert() = 0;
    
    /**
     * @brief 从系统信任存储卸载CA证书
     * 
     * @return 操作结果
     * 
     * @note 需要管理员权限
     */
    virtual OperationResult uninstallCACert() = 0;
    
    // ========== 状态查询 ==========
    
    /**
     * @brief 检查CA证书是否已生成
     * 
     * @return true CA证书文件存在
     */
    [[nodiscard]] virtual bool caCertExists() const = 0;
    
    /**
     * @brief 检查服务器证书是否已生成
     * 
     * @return true 服务器证书文件存在
     */
    [[nodiscard]] virtual bool serverCertExists() const = 0;
    
    /**
     * @brief 检查所有证书是否已生成
     * 
     * @return true CA证书和服务器证书都存在
     */
    [[nodiscard]] virtual bool allCertsExist() const = 0;
    
    /**
     * @brief 检查CA证书是否已安装到系统
     * 
     * @return true CA证书已在系统信任存储中
     */
    virtual bool isCACertInstalled() = 0;
    
    // ========== 路径获取 ==========
    
    /**
     * @brief 获取CA证书路径
     */
    [[nodiscard]] virtual std::string caCertPath() const = 0;
    
    /**
     * @brief 获取CA私钥路径
     */
    [[nodiscard]] virtual std::string caKeyPath() const = 0;
    
    /**
     * @brief 获取服务器证书路径
     */
    [[nodiscard]] virtual std::string serverCertPath() const = 0;
    
    /**
     * @brief 获取服务器私钥路径
     */
    [[nodiscard]] virtual std::string serverKeyPath() const = 0;
    
    /**
     * @brief 获取数据目录路径
     */
    [[nodiscard]] virtual std::string dataDir() const = 0;
    
    // ========== 证书信息 ==========
    
    /**
     * @brief 获取证书指纹 (SHA1)
     * 
     * @param certPath 证书文件路径
     * @return 证书指纹的十六进制字符串，失败返回空字符串
     */
    virtual std::string getCertThumbprint(const std::string& certPath) = 0;
    
    /**
     * @brief 获取证书通用名称 (CN)
     * 
     * @param certPath 证书文件路径
     * @return 证书的CN，失败返回空字符串
     */
    virtual std::string getCertCommonName(const std::string& certPath) = 0;
    
    // ========== 回调设置 ==========
    
    /**
     * @brief 设置日志回调
     * 
     * @param callback 日志回调函数
     */
    virtual void setLogCallback(LogCallback callback) = 0;
    
    // ========== 常量 ==========
    
    /**
     * @brief 获取默认劫持域名列表
     * 
     * 返回默认的域名列表，实现类可以通过配置覆盖。
     * 
     * @return 默认域名列表
     */
    static std::vector<std::string> defaultHijackDomains() {
        return {"api.openai.com", "api.anthropic.com"};
    }
    
    /**
     * @brief CA证书有效期（天）- 100年
     */
    static constexpr int CA_VALIDITY_DAYS = 36500;
    
    /**
     * @brief 服务器证书有效期（天）- 1年
     */
    static constexpr int SERVER_VALIDITY_DAYS = 365;
    
    // ========== 域名配置（可选覆盖）==========
    
    /**
     * @brief 获取当前劫持域名列表
     * 
     * @return 域名列表
     */
    [[nodiscard]] virtual std::vector<std::string> hijackDomains() const {
        return defaultHijackDomains();
    }
    
    /**
     * @brief 设置劫持域名列表
     * 
     * @param domains 域名列表
     * 
     * @note 默认实现为空操作，子类可覆盖
     */
    virtual void setHijackDomains(const std::vector<std::string>& domains) {
        (void)domains; // 默认忽略
    }

protected:
    ICertManager() = default;
};

/// 证书管理器智能指针类型
using ICertManagerPtr = std::unique_ptr<ICertManager>;

#endif // I_CERT_MANAGER_H
