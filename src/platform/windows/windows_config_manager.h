/**
 * @file windows_config_manager.h
 * @brief Windows 配置管理器头文件
 * 
 * 本文件定义了 Windows 平台的配置管理器实现类。
 * 
 * @details
 * 功能概述：
 * 
 * WindowsConfigManager 实现了 IConfigManager 接口，提供 Windows 平台的配置管理功能。
 * 使用 YAML 格式存储配置，使用 DPAPI 加密敏感数据。
 * 
 * 主要功能：
 * 
 * 1. YAML 格式配置存储
 *    - 人类可读的配置格式
 *    - 支持复杂数据结构
 * 
 * 2. DPAPI 敏感数据加密
 *    - API 密钥等敏感信息加密存储
 *    - 使用 Windows 数据保护 API
 *    - 绑定到当前机器
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

#ifndef WINDOWS_CONFIG_MANAGER_H
#define WINDOWS_CONFIG_MANAGER_H

#include "../interfaces/i_config_manager.h"

#include <QObject>
#include <QString>

/**
 * @brief Windows 配置管理器实现
 * 
 * 使用 DPAPI 加密敏感数据，YAML 格式存储配置。
 * 同时继承 QObject 以支持 Qt 信号槽（用于 UI 集成）。
 */
class WindowsConfigManager : public QObject, public IConfigManager {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * @param configPath 配置文件路径
     * @param parent Qt 父对象
     */
    explicit WindowsConfigManager(const QString& configPath, QObject* parent = nullptr);
    ~WindowsConfigManager() override = default;
    
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
     * 
     * 用于与 Qt UI 组件集成。
     */
    void configChanged();
    
    /**
     * @brief Qt 信号：日志消息
     * 
     * 用于与 Qt UI 组件集成。
     */
    void logMessage(const QString& message, LogLevel level);

private:
    // ========== DPAPI 加密 ==========
    
    /**
     * @brief 加密字符串
     * 
     * 使用 Windows DPAPI 加密敏感数据。
     * 
     * @param plainText 明文
     * @return 加密后的 Base64 编码字符串
     */
    QString encryptString(const QString& plainText);
    
    /**
     * @brief 解密字符串
     * 
     * 使用 Windows DPAPI 解密数据。
     * 
     * @param encryptedText 加密的 Base64 编码字符串
     * @return 解密后的明文
     */
    QString decryptString(const QString& encryptedText);
    
    /**
     * @brief 发送日志
     */
    void emitLog(const std::string& message, LogLevel level);
    
    // ========== 成员变量 ==========
    
    QString m_configPath;                       ///< 配置文件路径
    AppConfig m_config;                         ///< 当前配置
    LogCallback m_logCallback;                  ///< 日志回调
    ConfigChangedCallback m_configChangedCallback; ///< 配置变更回调
};

#endif // WINDOWS_CONFIG_MANAGER_H
