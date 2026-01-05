/**
 * @file main_widget.h
 * @brief 主窗口部件
 * 
 * 应用程序主界面，使用 QWidget 作为基类。
 * 
 * @details
 * 功能概述：
 * - 管理 UI 面板：配置组面板、运行时选项面板、日志面板
 * - 协调各服务组件：证书管理、Hosts 管理、代理服务
 * - 处理一键启动和停止流程
 * - 窗口关闭时自动清理资源
 * 
 * 架构设计：
 * - 依赖倒置：通过接口依赖平台服务，不直接依赖具体实现
 * - 依赖注入：平台服务通过工厂创建并注入，便于测试和扩展
 * - 信号槽机制：使用 Qt 信号槽实现组件间通信
 * 
 * 启动流程（一键启动）：
 * 1. 验证配置：检查是否有有效的代理配置
 * 2. 生成证书：检查/生成 CA 证书和服务器证书
 * 3. 安装证书：将 CA 证书安装到系统信任存储
 * 4. 修改 hosts：添加域名劫持条目
 * 5. 启动代理：启动 HTTPS 代理服务器
 * 
 * 停止流程：
 * 1. 停止代理服务器
 * 2. 恢复 hosts 文件
 * 
 * 使用示例：
 * @code
 * // 使用默认平台工厂
 * MainWidget* widget = new MainWidget();
 * widget->show();
 * 
 * // 使用自定义平台工厂（用于测试）
 * auto mockFactory = std::make_shared<MockPlatformFactory>();
 * MainWidget* testWidget = new MainWidget(mockFactory);
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef MAIN_WIDGET_H
#define MAIN_WIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <memory>

#include "../core/qt/log_manager.h"
#include "../platform/platform_factory.h"

// 前向声明
class ConfigGroupPanel;
class RuntimeOptionsPanel;
class LogPanel;
class NetworkManager;

/**
 * @brief 主窗口部件类
 * 
 * 应用程序主界面，负责协调各服务组件和 UI 面板。
 * 
 * @details
 * 职责：
 * - 管理 UI 面板：配置、运行时选项、日志
 * - 协调各服务组件：证书、Hosts、代理
 * - 处理一键启动和停止流程
 * 
 * 设计特点：
 * - 使用 QWidget 作为基类而非 QMainWindow
 * - 依赖倒置：通过接口依赖平台服务
 * - 依赖注入：平台服务通过工厂创建并注入
 * 
 * @note 窗口关闭时会自动清理资源，停止代理并恢复 hosts
 */
class MainWidget : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * 使用平台工厂创建所有平台相关服务。
     */
    explicit MainWidget(QWidget* parent = nullptr);
    
    /**
     * @brief 带依赖注入的构造函数
     * 
     * 允许外部注入平台工厂，便于测试和自定义。
     */
    explicit MainWidget(IPlatformFactoryPtr factory, QWidget* parent = nullptr);
    
    ~MainWidget() override;
    
    // 禁止拷贝
    MainWidget(const MainWidget&) = delete;
    MainWidget& operator=(const MainWidget&) = delete;

signals:
    /// 日志消息信号
    void logMessage(const QString& message, LogLevel level);

protected:
    /// 窗口关闭事件处理
    void closeEvent(QCloseEvent* event) override;

private slots:
    // UI 事件处理
    void onStartAllServices();
    void onStopServices();
    void onProxyStatusChanged(bool running);
    void onLogMessage(const QString& message, LogLevel level);
    void onToggleView();

private:
    // 初始化方法
    void initializePlatformServices();
    void initializeCoreServices();
    void setupUi();
    void connectSignals();
    
    // 状态更新
    void updateStatusLabel(bool running);
    void updateButtonStates(bool servicesRunning);
    
    // 资源管理
    void cleanup();
    std::string getUserDataDir() const;

    // ========== 平台服务（纯 C++ 接口）==========
    
    IPlatformFactoryPtr m_platformFactory;      ///< 平台工厂
    IConfigManagerPtr m_configManager;          ///< 配置管理器
    ICertManagerPtr m_certManager;              ///< 证书管理器
    IPrivilegeManagerPtr m_privilegeManager;    ///< 权限管理器
    IHostsManagerPtr m_hostsManager;            ///< Hosts 管理器
    
    // ========== 核心服务 ==========
    
    NetworkManager* m_proxyServer;              ///< 网络管理器（Qt 实现）
    
    // ========== UI 组件 ==========
    
    QStackedWidget* m_stackedWidget;
    ConfigGroupPanel* m_configGroupPanel;
    RuntimeOptionsPanel* m_runtimeOptionsPanel;
    LogPanel* m_logPanel;
    QLabel* m_statusLabel;
    QPushButton* m_startAllButton;
    QPushButton* m_stopButton;
    QPushButton* m_toggleViewButton;
    
    // ========== 状态标志 ==========
    
    bool m_servicesRunning;
    bool m_hostsModified;
};

#endif // MAIN_WIDGET_H
