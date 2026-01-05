/**
 * @file linux_config_manager.cpp
 * @brief Linux 配置管理器实现文件
 * 
 * 本文件实现了 LinuxConfigManager 类的所有方法。
 * 
 * @details
 * 实现模块：
 * 
 * 1. 加密（使用 libsecret 或 Base64 回退）
 *    - encryptString(): 加密字符串
 *    - decryptString(): 解密字符串
 * 
 * 2. 配置加载
 *    - load(): 从 YAML 文件加载配置
 *    - 自动解密敏感字段
 * 
 * 3. 配置保存
 *    - save(): 保存配置到 YAML 文件
 *    - saveAsync(): 异步保存配置
 *    - 自动加密敏感字段
 * 
 * 4. 配置操作
 *    - addProxyConfig(): 添加代理配置
 *    - updateProxyConfig(): 更新代理配置
 *    - removeProxyConfig(): 删除代理配置
 *    - getProxyConfig(): 获取代理配置
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "linux_config_manager.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <yaml-cpp/yaml.h>
#include <sstream>

// ============================================================================
// 构造函数
// ============================================================================

LinuxConfigManager::LinuxConfigManager(const QString& configPath, QObject* parent)
    : QObject(parent)
    , m_configPath(configPath)
    , m_logCallback(nullptr)
    , m_configChangedCallback(nullptr)
    , m_libsecretAvailable(isLibsecretAvailable())
{
}

// ============================================================================
// 回调设置
// ============================================================================

void LinuxConfigManager::setLogCallback(LogCallback callback)
{
    m_logCallback = std::move(callback);
}

void LinuxConfigManager::setConfigChangedCallback(ConfigChangedCallback callback)
{
    m_configChangedCallback = std::move(callback);
}

void LinuxConfigManager::emitLog(const std::string& message, LogLevel level)
{
    if (m_logCallback) {
        m_logCallback(message, level);
    }
    emit logMessage(QString::fromStdString(message), level);
}

// ============================================================================
// libsecret 检测和加密
// ============================================================================

bool LinuxConfigManager::isLibsecretAvailable() const
{
    // 简单检测：尝试运行 secret-tool 命令
    // 在实际实现中，可以使用 libsecret 库的 API
    QProcess process;
    process.start("which", {"secret-tool"});
    process.waitForFinished(1000);
    return process.exitCode() == 0;
}

QString LinuxConfigManager::encryptString(const QString& plainText)
{
    if (plainText.isEmpty()) {
        return QString();
    }
    
    // 使用 Base64 编码作为简单的混淆
    // 注意：这不是真正的加密，只是防止明文存储
    // 在生产环境中应该使用 libsecret
    QByteArray utf8Data = plainText.toUtf8();
    QString encoded = QString::fromLatin1(utf8Data.toBase64());
    
    // 添加前缀标识这是编码后的数据
    return "BASE64:" + encoded;
}

QString LinuxConfigManager::decryptString(const QString& encryptedText)
{
    if (encryptedText.isEmpty()) {
        return QString();
    }
    
    // 检查是否是 Base64 编码的数据
    if (encryptedText.startsWith("BASE64:")) {
        QString encoded = encryptedText.mid(7); // 跳过 "BASE64:" 前缀
        QByteArray decoded = QByteArray::fromBase64(encoded.toLatin1());
        return QString::fromUtf8(decoded);
    }
    
    // 可能是未加密的明文，直接返回
    return encryptedText;
}

// ============================================================================
// 配置加载
// ============================================================================

OperationResult LinuxConfigManager::load()
{
    QFileInfo fileInfo(m_configPath);
    
    if (!fileInfo.exists()) {
        emitLog("配置文件不存在，使用默认配置: " + m_configPath.toStdString(), LogLevel::Info);
        m_config = AppConfig{};
        return OperationResult::success("使用默认配置");
    }
    
    try {
        QFile file(m_configPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::string errorMsg = "无法打开配置文件: " + m_configPath.toStdString();
            emitLog(errorMsg, LogLevel::Error);
            return OperationResult::failure(errorMsg, ErrorCode::ConfigLoadFailed);
        }
        
        QByteArray content = file.readAll();
        file.close();
        
        YAML::Node root = YAML::Load(content.toStdString());
        
        AppConfig config;
        
        if (root["proxy_configs"] && root["proxy_configs"].IsSequence()) {
            config.proxyConfigs.reserve(root["proxy_configs"].size());
            for (const auto& node : root["proxy_configs"]) {
                ProxyConfigItem item;
                
                if (node["name"]) {
                    item.name = node["name"].as<std::string>();
                }
                if (node["local_url"]) {
                    item.localUrl = node["local_url"].as<std::string>();
                }
                if (node["mapped_url"]) {
                    item.mappedUrl = node["mapped_url"].as<std::string>();
                }
                if (node["local_model_name"]) {
                    item.localModelName = node["local_model_name"].as<std::string>();
                }
                if (node["mapped_model_name"]) {
                    item.mappedModelName = node["mapped_model_name"].as<std::string>();
                }
                if (node["auth_key"]) {
                    item.authKey = decryptString(
                        QString::fromStdString(node["auth_key"].as<std::string>())
                    ).toStdString();
                }
                if (node["api_key"]) {
                    item.apiKey = decryptString(
                        QString::fromStdString(node["api_key"].as<std::string>())
                    ).toStdString();
                }
                
                config.proxyConfigs.push_back(item);
            }
        }
        
        m_config = std::move(config);
        
        if (m_configChangedCallback) m_configChangedCallback();
        emit configChanged();
        
        emitLog("配置加载成功，共 " + std::to_string(m_config.proxyConfigs.size()) + " 个代理配置", 
                LogLevel::Info);
        return OperationResult::success("配置加载成功");
        
    } catch (const YAML::Exception& e) {
        std::string errorMsg = "YAML解析错误: " + std::string(e.what());
        emitLog(errorMsg, LogLevel::Error);
        return OperationResult::failure(errorMsg, ErrorCode::ConfigLoadFailed);
    } catch (const std::exception& e) {
        std::string errorMsg = "配置加载失败: " + std::string(e.what());
        emitLog(errorMsg, LogLevel::Error);
        return OperationResult::failure(errorMsg, ErrorCode::ConfigLoadFailed);
    }
}

// ============================================================================
// 配置保存
// ============================================================================

OperationResult LinuxConfigManager::save()
{
    QFileInfo fileInfo(m_configPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        std::string errorMsg = "无法创建配置目录: " + dir.absolutePath().toStdString();
        emitLog(errorMsg, LogLevel::Error);
        return OperationResult::failure(errorMsg, ErrorCode::ConfigSaveFailed);
    }
    
    try {
        YAML::Node root;
        YAML::Node configsNode(YAML::NodeType::Sequence);
        
        for (const auto& config : m_config.proxyConfigs) {
            YAML::Node node;
            node["name"] = config.name;
            node["local_url"] = config.localUrl;
            node["mapped_url"] = config.mappedUrl;
            node["local_model_name"] = config.localModelName;
            node["mapped_model_name"] = config.mappedModelName;
            node["auth_key"] = encryptString(QString::fromStdString(config.authKey)).toStdString();
            node["api_key"] = encryptString(QString::fromStdString(config.apiKey)).toStdString();
            configsNode.push_back(node);
        }
        root["proxy_configs"] = configsNode;
        
        QFile file(m_configPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::string errorMsg = "无法打开配置文件进行写入: " + m_configPath.toStdString();
            emitLog(errorMsg, LogLevel::Error);
            return OperationResult::failure(errorMsg, ErrorCode::ConfigSaveFailed);
        }
        
        std::ostringstream oss;
        oss << root;
        file.write(QByteArray::fromStdString(oss.str()));
        file.close();
        
        if (m_configChangedCallback) m_configChangedCallback();
        emit configChanged();
        
        emitLog("配置保存成功", LogLevel::Info);
        return OperationResult::success("配置保存成功");
        
    } catch (const YAML::Exception& e) {
        std::string errorMsg = "YAML序列化错误: " + std::string(e.what());
        emitLog(errorMsg, LogLevel::Error);
        return OperationResult::failure(errorMsg, ErrorCode::ConfigSaveFailed);
    } catch (const std::exception& e) {
        std::string errorMsg = "配置保存失败: " + std::string(e.what());
        emitLog(errorMsg, LogLevel::Error);
        return OperationResult::failure(errorMsg, ErrorCode::ConfigSaveFailed);
    }
}

void LinuxConfigManager::saveAsync()
{
    auto future = QtConcurrent::run([this]() {
        return this->save();
    });
    
    auto* watcher = new QFutureWatcher<OperationResult>(this);
    connect(watcher, &QFutureWatcher<OperationResult>::finished, this, [this, watcher]() {
        auto result = watcher->result();
        if (!result.ok) {
            emitLog("异步配置保存失败: " + result.message, LogLevel::Error);
        }
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

// ============================================================================
// 配置操作
// ============================================================================

void LinuxConfigManager::setConfig(const AppConfig& config)
{
    m_config = config;
    if (m_configChangedCallback) m_configChangedCallback();
    emit configChanged();
}

OperationResult LinuxConfigManager::addProxyConfig(const ProxyConfigItem& config)
{
    auto validation = validateProxyConfig(config);
    if (!validation.ok) {
        return validation;
    }
    
    m_config.proxyConfigs.push_back(config);
    if (m_configChangedCallback) m_configChangedCallback();
    emit configChanged();
    return OperationResult::success("代理配置添加成功");
}

OperationResult LinuxConfigManager::updateProxyConfig(size_t index, const ProxyConfigItem& config)
{
    if (index >= m_config.proxyConfigs.size()) {
        return OperationResult::failure("索引超出范围", ErrorCode::ConfigInvalid);
    }
    
    auto validation = validateProxyConfig(config);
    if (!validation.ok) {
        return validation;
    }
    
    m_config.proxyConfigs[index] = config;
    if (m_configChangedCallback) m_configChangedCallback();
    emit configChanged();
    return OperationResult::success("代理配置更新成功");
}

OperationResult LinuxConfigManager::removeProxyConfig(size_t index)
{
    if (index >= m_config.proxyConfigs.size()) {
        return OperationResult::failure("索引超出范围", ErrorCode::ConfigInvalid);
    }
    
    m_config.proxyConfigs.erase(m_config.proxyConfigs.begin() + static_cast<ptrdiff_t>(index));
    if (m_configChangedCallback) m_configChangedCallback();
    emit configChanged();
    return OperationResult::success("代理配置删除成功");
}

const ProxyConfigItem* LinuxConfigManager::getProxyConfig(size_t index) const
{
    if (index >= m_config.proxyConfigs.size()) {
        return nullptr;
    }
    return &m_config.proxyConfigs[index];
}
