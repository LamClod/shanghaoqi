/**
 * @file linux_config_manager.h
 * @brief Linux 配置管理器头文件
 * 
 * 本文件定义了 Linux 平台的配置管理器实现类。
 * 
 * @details
 * 功能概述：
 * 
 * LinuxConfigManager 实现了 IConfigManager 接口，提供 Linux 平台的配置管理功能。
 * 使用 YAML 格式存储配置，使用 libsecret 加密敏感数据。
 * 
 * 主要功能：
 * 
 * 1. YAML 格式配置存储
 *    - 人类可读的配置格式
 *    - 支持复杂数据结构
 * 
 * 2. libsecret 敏感数据加密
 *    - API 密钥等敏感信息加密存储
 *    - 使用 GNOME Keyring 或 KDE Wallet
 *    - 如果 libsecret 不可用，使用 Base64 编码（不安全）
 * 
 * 3. 配置项的增删改查
 *    - 添加、更新、删除代理配置
 *    - 配置验证
 * 
 * 4. Qt 信号支持
 *    - configChanged 信号
 *    - logMessage 信号
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef LINUX_CONFIG_MANAGER_H
#define LINUX_CONFIG_MANAGER_H

#include "../interfaces/i_config_manager.h"

#include <QObject>
#include <QString>

/**
 * @brief Linux 配置管理器实现
 * 
 * 使用 libsecret 加密敏感数据，YAML 格式存储配置。
 * 同时继承 QObject 以支持 Qt 信号槽（用于 UI 集成）。
 */
class LinuxConfigManager : public QObject, public IConfigManager {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * @param configPath 配置文件路径
     * @param parent Qt 父对象
     */
    explicit LinuxConfigManager(const QString& configPath, QObject* parent = nullptr);
    ~LinuxConfigManager() override = default;
    
    // ========== IConfigManager 接口实现 ==========
    
    OperationResult load() override;
    OperationResult save() override;
    void saveAsync() override;
    
    [[nodiscard]] const AppConfig& getConfig() const override { return m_config; }
    void setConfig(const AppConfig& config) override;
    
    OperationResult addProxyConfig(const ProxyConfigItem& config) override;
    OperationResult updateProxyConfig(size_t index, const ProxyConfigItem& config) override;
    OperationResult removeProxyConfig(size_t index) override;
    [[nodiscard]] const ProxyConfigItem* getProxyConfig(size_t index) const override;
    
    [[nodiscard]] std::string configPath() const override { return m_configPath.toStdString(); }
    [[nodiscard]] size_t configCount() const override { return m_config.proxyConfigs.size(); }
    
    void setLogCallback(LogCallback callback) override;
    void setConfigChangedCallback(ConfigChangedCallback callback) override;

signals:
    /**
     * @brief Qt 信号：配置变更
     */
    void configChanged();
    
    /**
     * @brief Qt 信号：日志消息
     */
    void logMessage(const QString& message, LogLevel level);

private:
    // ========== 加密方法 ==========
    
    /**
     * @brief 加密字符串
     * 
     * 尝试使用 libsecret 加密，如果不可用则使用 Base64 编码。
     * 
     * @param plainText 明文
     * @return 加密后的字符串
     */
    QString encryptString(const QString& plainText);
    
    /**
     * @brief 解密字符串
     * 
     * 尝试使用 libsecret 解密，如果不可用则使用 Base64 解码。
     * 
     * @param encryptedText 加密的字符串
     * @return 解密后的明文
     */
    QString decryptString(const QString& encryptedText);
    
    /**
     * @brief 检查 libsecret 是否可用
     */
    bool isLibsecretAvailable() const;
    
    /**
     * @brief 发送日志
     */
    void emitLog(const std::string& message, LogLevel level);
    
    // ========== 成员变量 ==========
    
    QString m_configPath;                       ///< 配置文件路径
    AppConfig m_config;                         ///< 当前配置
    LogCallback m_logCallback;                  ///< 日志回调
    ConfigChangedCallback m_configChangedCallback; ///< 配置变更回调
    bool m_libsecretAvailable;                  ///< libsecret 是否可用
};

#endif // LINUX_CONFIG_MANAGER_H
