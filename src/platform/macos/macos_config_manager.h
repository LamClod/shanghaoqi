/**
 * @file macos_config_manager.h
 * @brief macOS 配置管理器头文件
 * 
 * 本文件定义了 macOS 平台的配置管理器实现类。
 * 
 * @details
 * 功能概述：
 * 
 * MacOSConfigManager 实现了 IConfigManager 接口，提供 macOS 平台的配置管理功能。
 * 使用 YAML 格式存储配置，使用 Keychain 加密敏感数据。
 * 
 * 主要功能：
 * 
 * 1. YAML 格式配置存储
 *    - 人类可读的配置格式
 *    - 支持复杂数据结构
 * 
 * 2. Keychain 敏感数据加密
 *    - API 密钥等敏感信息加密存储
 *    - 使用 macOS Keychain Services
 *    - 如果 Keychain 不可用，使用 Base64 编码
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

#ifndef MACOS_CONFIG_MANAGER_H
#define MACOS_CONFIG_MANAGER_H

#include "../interfaces/i_config_manager.h"

#include <QObject>
#include <QString>

/**
 * @brief macOS 配置管理器实现
 * 
 * 使用 Keychain 加密敏感数据，YAML 格式存储配置。
 * 同时继承 QObject 以支持 Qt 信号槽（用于 UI 集成）。
 */
class MacOSConfigManager : public QObject, public IConfigManager {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * @param configPath 配置文件路径
     * @param parent Qt 父对象
     */
    explicit MacOSConfigManager(const QString& configPath, QObject* parent = nullptr);
    ~MacOSConfigManager() override = default;
    
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
    void configChanged();
    void logMessage(const QString& message, LogLevel level);

private:
    /**
     * @brief 加密字符串（使用 Keychain 或 Base64）
     */
    QString encryptString(const QString& plainText);
    
    /**
     * @brief 解密字符串
     */
    QString decryptString(const QString& encryptedText);
    
    /**
     * @brief 发送日志
     */
    void emitLog(const std::string& message, LogLevel level);
    
    // ========== 成员变量 ==========
    
    QString m_configPath;
    AppConfig m_config;
    LogCallback m_logCallback;
    ConfigChangedCallback m_configChangedCallback;
};

#endif // MACOS_CONFIG_MANAGER_H
