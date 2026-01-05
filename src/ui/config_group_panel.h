/**
 * @file config_group_panel.h
 * @brief 配置组面板
 * 
 * 本文件定义了配置组面板及相关组件，提供代理配置的完整管理功能。
 * 
 * @details
 * 功能概述：
 * - 代理配置的表格显示（名称、URL、模型、密钥等）
 * - 配置的增删改查操作
 * - 配置的导入导出（JSON 格式）
 * - 单个/批量配置测活
 * - 模型列表自动获取
 * 
 * 组件结构：
 * - ProxyConfigDialog：配置编辑对话框
 * - ConfigGroupPanel：配置列表面板
 * 
 * 表格列说明：
 * | 列名 | 说明 |
 * |------|------|
 * | 名称 | 配置的显示名称 |
 * | 本地URL | 客户端请求的目标域名 |
 * | 映射URL | 实际转发的 API 地址 |
 * | 本地模型 | 客户端请求的模型名称 |
 * | 映射模型 | 实际使用的模型名称 |
 * | 鉴权Key | 客户端认证密钥（脱敏显示） |
 * | 状态 | 配置状态 |
 * 
 * 模型获取逻辑：
 * - 当映射 URL 和 API 密钥都填写后，自动延迟获取模型列表
 * - 使用定时器防抖，避免频繁请求
 * - 调用 /v1/models 接口获取可用模型
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see IConfigManager 配置管理器接口
 */

#ifndef CONFIG_GROUP_PANEL_H
#define CONFIG_GROUP_PANEL_H

#include <QWidget>
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QNetworkAccessManager>
#include "../platform/interfaces/i_config_manager.h"
#include "../core/qt/log_manager.h"

/**
 * @brief 代理配置编辑对话框
 * 
 * 用于创建和编辑代理配置项的模态对话框。
 * 
 * @details
 * 功能特性：
 * - 支持新建和编辑两种模式
 * - 本地 URL 下拉选择（api.openai.com、api.anthropic.com）
 * - 模型列表自动获取（输入 API 地址和密钥后自动获取）
 * - 输入验证（必填字段检查）
 * 
 * 模型获取逻辑：
 * - 当映射 URL 和 API 密钥都填写后，自动延迟获取模型列表
 * - 使用定时器防抖，避免频繁请求
 * - 获取成功后填充模型下拉框
 */
class ProxyConfigDialog : public QDialog {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数（新建模式）
     * 
     * 创建空白的配置编辑对话框。
     * 
     * @param parent 父窗口
     */
    explicit ProxyConfigDialog(QWidget* parent = nullptr);
    
    /**
     * @brief 构造函数（编辑模式）
     * 
     * 创建预填充的配置编辑对话框。
     * 
     * @param config 要编辑的配置项
     * @param parent 父窗口
     */
    explicit ProxyConfigDialog(const ProxyConfigItem& config, QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ProxyConfigDialog() override;

    /**
     * @brief 获取编辑后的配置
     * 
     * @return 用户输入的配置项
     */
    ProxyConfigItem getProxyConfig() const;

private slots:
    /**
     * @brief 确认按钮点击处理
     * 
     * 验证输入并关闭对话框。
     */
    void onAccept();
    
    /**
     * @brief 手动获取模型列表
     */
    void onFetchModels();
    
    /**
     * @brief 映射URL变化处理
     * 
     * 当映射URL变化时，重置模型获取定时器。
     */
    void onMappedUrlChanged();
    
    /**
     * @brief API密钥变化处理
     * 
     * 当API密钥变化时，重置模型获取定时器。
     */
    void onApiKeyChanged();

private:
    /**
     * @brief 初始化UI
     */
    void setupUi();
    
    /**
     * @brief 填充字段
     * 
     * @param config 要填充的配置项
     */
    void populateFields(const ProxyConfigItem& config);
    
    /**
     * @brief 启动模型获取定时器
     */
    void startModelFetchTimer();
    
    /**
     * @brief 停止模型获取定时器
     */
    void stopModelFetchTimer();
    
    /**
     * @brief 从API获取模型列表
     */
    void fetchModelsFromApi();
    
    /**
     * @brief 检查是否可以获取模型
     * 
     * @return true 可以获取（URL和密钥都已填写）
     */
    bool canFetchModels() const;
    
    // ========== UI组件 ==========
    
    QLineEdit* m_nameEdit;              ///< 配置名称输入框
    QComboBox* m_localUrlCombo;         ///< 本地URL下拉框
    QLineEdit* m_mappedUrlEdit;         ///< 映射URL输入框
    QLineEdit* m_localModelNameEdit;    ///< 本地模型名称输入框
    QComboBox* m_mappedModelNameCombo;  ///< 映射模型名称下拉框
    QLineEdit* m_authKeyEdit;           ///< 认证密钥输入框
    QLineEdit* m_apiKeyEdit;            ///< API密钥输入框
    QLabel* m_statusLabel;              ///< 状态标签
    
    // ========== 模型获取相关 ==========
    
    QTimer* m_modelFetchTimer;          ///< 模型获取防抖定时器
    QNetworkAccessManager* m_networkManager; ///< 网络管理器
    QString m_lastFetchedUrl;           ///< 上次获取的URL
    QString m_lastFetchedApiKey;        ///< 上次获取的API密钥
};

/**
 * @brief 配置组面板
 * 
 * 显示代理配置表格，提供完整的配置管理功能。
 * 
 * @details
 * 功能列表：
 * - 表格显示所有配置项
 * - 添加新配置
 * - 编辑现有配置（双击或按钮）
 * - 删除配置
 * - 刷新配置列表
 * - 测试单个配置
 * - 批量测试所有配置
 * - 导出配置到 JSON 文件
 * - 从 JSON 文件导入配置
 * 
 * 信号机制：
 * - configChanged：配置变化时发出
 * - logMessage：日志消息信号
 */
class ConfigGroupPanel : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * @param configManager 配置管理器实例
     * @param parent 父窗口部件
     */
    explicit ConfigGroupPanel(IConfigManager* configManager, QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~ConfigGroupPanel() override = default;
    
    /**
     * @brief 刷新配置表格
     * 
     * 从ConfigStore重新加载数据并更新表格显示。
     */
    void refreshTable();

signals:
    /**
     * @brief 配置变化信号
     * 
     * 当配置被添加、修改或删除时发出。
     */
    void configChanged();
    
    /**
     * @brief 日志消息信号
     * 
     * @param message 日志消息
     * @param level 日志级别
     */
    void logMessage(const QString& message, LogLevel level);

private slots:
    /**
     * @brief 添加配置
     */
    void onAddConfig();
    
    /**
     * @brief 编辑配置
     */
    void onEditConfig();
    
    /**
     * @brief 删除配置
     */
    void onDeleteConfig();
    
    /**
     * @brief 刷新列表
     */
    void onRefresh();
    
    /**
     * @brief 测试选中的配置
     */
    void onTestConfig();
    
    /**
     * @brief 导出配置
     */
    void onExportConfig();
    
    /**
     * @brief 导入配置
     */
    void onImportConfig();
    
    /**
     * @brief 测试所有配置
     */
    void onTestAllConfigs();
    
    /**
     * @brief 选择变化处理
     * 
     * 更新按钮状态（编辑、删除、测试按钮的启用状态）。
     */
    void onSelectionChanged();

private:
    /**
     * @brief 初始化UI
     */
    void setupUi();
    
    /**
     * @brief 更新按钮状态
     * 
     * 根据当前选择状态启用/禁用相关按钮。
     */
    void updateButtonStates();
    
    /**
     * @brief 获取当前选中行
     * 
     * @return 选中行的索引，未选中返回-1
     */
    int currentRow() const;
    
    /**
     * @brief 脱敏API密钥
     * 
     * 将API密钥转换为脱敏显示格式（如 sk-***xxx）。
     * 
     * @param apiKey 原始API密钥
     * @return 脱敏后的字符串
     */
    QString maskApiKey(const QString& apiKey) const;
    
    // ========== UI组件 ==========
    
    QTableWidget* m_configTable;    ///< 配置表格
    QPushButton* m_addButton;       ///< 添加按钮
    QPushButton* m_editButton;      ///< 编辑按钮
    QPushButton* m_deleteButton;    ///< 删除按钮
    QPushButton* m_refreshButton;   ///< 刷新按钮
    QPushButton* m_testButton;      ///< 测试按钮
    QPushButton* m_exportButton;    ///< 导出按钮
    QPushButton* m_importButton;    ///< 导入按钮
    QPushButton* m_testAllButton;   ///< 批量测试按钮
    
    IConfigManager* m_configManager;  ///< 配置管理器引用
};

#endif // CONFIG_GROUP_PANEL_H
