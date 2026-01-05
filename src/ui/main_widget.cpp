/**
 * @file main_widget.cpp
 * @brief 主窗口部件实现
 * 
 * 实现应用程序主界面的所有功能。
 * 
 * @details
 * 实现细节：
 * 
 * 初始化流程：
 * 1. 创建平台工厂（或使用注入的工厂）
 * 2. 初始化平台服务（配置、证书、权限、Hosts）
 * 3. 初始化核心服务（网络管理器）
 * 4. 加载配置文件
 * 5. 设置 UI 组件
 * 6. 连接信号槽
 * 
 * UI 布局：
 * - QStackedWidget 切换配置页面和日志页面
 * - 配置页面：配置组面板 + 运行时选项面板
 * - 日志页面：日志面板
 * - 底部：操作按钮区域 + 状态栏
 * 
 * 资源管理：
 * - 窗口关闭时自动停止代理服务
 * - 自动恢复 hosts 文件修改
 * - 使用 Qt 父子对象机制管理内存
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "main_widget.h"
#include "config_group_panel.h"
#include "runtime_options_panel.h"
#include "log_panel.h"
#include "../core/qt/network_manager.h"

// Qt 布局
#include <QVBoxLayout>       // 垂直布局
#include <QHBoxLayout>       // 水平布局
#include <QGroupBox>         // 分组框

// Qt 对话框
#include <QMessageBox>       // 消息对话框

// Qt 路径和文件
#include <QStandardPaths>    // 标准路径
#include <QDir>              // 目录操作

// Qt 事件
#include <QCloseEvent>       // 关闭事件
#include <QApplication>      // 应用程序

// ============================================================================
// 构造与析构
// ============================================================================

/**
 * @brief 默认构造函数
 * 
 * 使用默认平台工厂创建主窗口。
 * 
 * @param parent 父窗口部件
 */
MainWidget::MainWidget(QWidget* parent)
    : MainWidget(PlatformFactory::create(), parent)  // 委托构造
{
}

/**
 * @brief 带依赖注入的构造函数
 * 
 * 允许外部注入平台工厂，便于测试和自定义。
 * 
 * @param factory 平台工厂智能指针
 * @param parent 父窗口部件
 * 
 * @details
 * 初始化流程：
 * 1. 初始化所有成员变量
 * 2. 设置窗口标题和大小
 * 3. 初始化平台服务
 * 4. 初始化核心服务
 * 5. 加载配置
 * 6. 设置 UI
 * 7. 连接信号槽
 */
MainWidget::MainWidget(IPlatformFactoryPtr factory, QWidget* parent)
    : QWidget(parent)
    , m_platformFactory(std::move(factory))     // 移动平台工厂所有权
    , m_proxyServer(nullptr)                    // 代理服务器初始化为空
    , m_stackedWidget(nullptr)                  // UI 组件初始化为空
    , m_configGroupPanel(nullptr)
    , m_runtimeOptionsPanel(nullptr)
    , m_logPanel(nullptr)
    , m_statusLabel(nullptr)
    , m_startAllButton(nullptr)
    , m_stopButton(nullptr)
    , m_toggleViewButton(nullptr)
    , m_servicesRunning(false)                  // 服务未运行
    , m_hostsModified(false)                    // hosts 未修改
{
    // 设置窗口标题
    setWindowTitle(QStringLiteral("上号器"));
    
    // 设置窗口默认大小
    resize(900, 600);
    
    // 初始化平台服务（配置、证书、权限、Hosts）
    initializePlatformServices();
    
    // 初始化核心服务（网络管理器）
    initializeCoreServices();
    
    // 加载配置文件
    if (m_configManager) {
        m_configManager->load();
    }
    
    // 设置 UI 组件
    setupUi();
    
    // 连接信号槽
    connectSignals();
    
    // 记录初始化完成日志
    LOG_INFO(QStringLiteral("主窗口初始化完成，平台: %1")
             .arg(QString::fromStdString(m_platformFactory->platformName())));
}

/**
 * @brief 析构函数
 * 
 * 清理资源，停止服务，恢复 hosts 文件。
 */
MainWidget::~MainWidget()
{
    cleanup();
}

// ============================================================================
// 初始化方法
// ============================================================================

/**
 * @brief 获取用户数据目录
 * 
 * 获取应用程序数据存储目录，如果不存在则创建。
 * 
 * @return 用户数据目录路径
 * 
 * @details
 * Windows 上通常是：C:\Users\<用户名>\AppData\Local\<应用名>
 */
std::string MainWidget::getUserDataDir() const
{
    // 获取应用程序数据目录
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    
    // 确保目录存在
    QDir dir(appDataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    return appDataPath.toStdString();
}

/**
 * @brief 初始化平台服务
 * 
 * 使用平台工厂创建所有平台相关服务。
 * 
 * @details
 * 创建的服务：
 * - ConfigManager: 配置管理器
 * - CertManager: 证书管理器
 * - PrivilegeManager: 权限管理器
 * - HostsManager: Hosts 文件管理器
 * 
 * 同时为各服务设置日志回调，将日志输出到 UI。
 */
void MainWidget::initializePlatformServices()
{
    // 获取用户数据目录
    std::string dataDir = getUserDataDir();
    
    // 使用平台工厂创建各服务
    m_configManager = m_platformFactory->createConfigManager(dataDir + "/config.yaml");
    m_certManager = m_platformFactory->createCertManager(dataDir);
    m_privilegeManager = m_platformFactory->createPrivilegeManager();
    m_hostsManager = m_platformFactory->createHostsManager(dataDir);
    
    // 设置日志回调，将服务日志输出到 UI
    // 使用 lambda 捕获 this 指针
    m_configManager->setLogCallback([this](const std::string& msg, LogLevel level) {
        onLogMessage(QString::fromStdString(msg), level);
    });
    
    m_certManager->setLogCallback([this](const std::string& msg, LogLevel level) {
        onLogMessage(QString::fromStdString(msg), level);
    });
    
    m_hostsManager->setLogCallback([this](const std::string& msg, LogLevel level) {
        onLogMessage(QString::fromStdString(msg), level);
    });
    
    // 记录初始化日志
    LOG_INFO(QStringLiteral("平台服务初始化完成"));
    LOG_INFO(QStringLiteral("  - 权限提升方式: %1")
             .arg(QString::fromStdString(m_privilegeManager->elevationMethod())));
}

/**
 * @brief 初始化核心服务
 * 
 * 创建网络管理器（代理服务器）。
 */
void MainWidget::initializeCoreServices()
{
    // 创建网络管理器，使用 this 作为父对象
    m_proxyServer = new NetworkManager(this);
}

/**
 * @brief 设置 UI
 * 
 * 创建和布局所有 UI 组件。
 * 
 * @details
 * 布局结构：
 * - 主布局：QVBoxLayout
 *   - QStackedWidget（页面切换）
 *     - 配置页面
 *       - 配置组面板（QGroupBox）
 *       - 运行时选项面板（QGroupBox）
 *     - 日志页面
 *       - 日志面板（QGroupBox）
 *   - 操作按钮区域（QGroupBox）
 *     - 切换视图按钮
 *     - 一键启动按钮
 *     - 停止服务按钮
 *   - 状态栏
 */
void MainWidget::setupUi()
{
    // 创建主布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);  // 设置边距
    mainLayout->setSpacing(10);                       // 设置组件间距
    
    // ========== 创建堆叠窗口（用于页面切换）==========
    m_stackedWidget = new QStackedWidget(this);
    
    // ========== 配置页面 ==========
    auto* configPage = new QWidget(this);
    auto* configLayout = new QVBoxLayout(configPage);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(10);
    
    // 配置组面板（包含配置表格和操作按钮）
    auto* configGroupBox = new QGroupBox(QStringLiteral("配置组管理"), this);
    auto* configGroupLayout = new QVBoxLayout(configGroupBox);
    m_configGroupPanel = new ConfigGroupPanel(m_configManager.get(), this);
    configGroupLayout->addWidget(m_configGroupPanel);
    configLayout->addWidget(configGroupBox, 1);  // stretch factor = 1，占据剩余空间
    
    // 运行时选项面板
    auto* runtimeOptionsBox = new QGroupBox(QStringLiteral("运行时选项"), this);
    auto* runtimeOptionsLayout = new QVBoxLayout(runtimeOptionsBox);
    m_runtimeOptionsPanel = new RuntimeOptionsPanel(this);
    runtimeOptionsLayout->addWidget(m_runtimeOptionsPanel);
    configLayout->addWidget(runtimeOptionsBox);
    
    // 将配置页面添加到堆叠窗口
    m_stackedWidget->addWidget(configPage);
    
    // ========== 日志页面 ==========
    auto* logPage = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logPage);
    logLayout->setContentsMargins(0, 0, 0, 0);
    
    // 日志面板
    auto* logBox = new QGroupBox(QStringLiteral("日志"), this);
    auto* logBoxLayout = new QVBoxLayout(logBox);
    m_logPanel = new LogPanel(this);
    logBoxLayout->addWidget(m_logPanel);
    logLayout->addWidget(logBox);
    
    // 将日志页面添加到堆叠窗口
    m_stackedWidget->addWidget(logPage);
    
    // 将堆叠窗口添加到主布局
    mainLayout->addWidget(m_stackedWidget, 1);  // stretch factor = 1
    
    // ========== 操作按钮区域 ==========
    auto* actionBox = new QGroupBox(QStringLiteral("操作"), this);
    auto* actionLayout = new QHBoxLayout(actionBox);
    
    // 切换视图按钮
    m_toggleViewButton = new QPushButton(QStringLiteral("查看日志"), this);
    m_toggleViewButton->setMinimumHeight(40);
    actionLayout->addWidget(m_toggleViewButton);
    
    // 一键启动按钮（主要操作按钮，使用粗体大字体）
    m_startAllButton = new QPushButton(QStringLiteral("一键启动全部服务"), this);
    m_startAllButton->setMinimumHeight(40);
    m_startAllButton->setStyleSheet("QPushButton { font-weight: bold; font-size: 14px; }");
    actionLayout->addWidget(m_startAllButton);
    
    // 停止服务按钮（初始禁用）
    m_stopButton = new QPushButton(QStringLiteral("停止服务"), this);
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setEnabled(false);
    actionLayout->addWidget(m_stopButton);
    
    mainLayout->addWidget(actionBox);
    
    // ========== 状态栏 ==========
    auto* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(QStringLiteral("代理状态: 已停止"), this);
    statusLayout->addStretch();  // 将状态标签推到右侧
    statusLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(statusLayout);
    
    // 更新状态标签样式
    updateStatusLabel(false);
}

/**
 * @brief 连接信号槽
 * 
 * 连接所有 UI 组件的信号到对应的槽函数。
 */
void MainWidget::connectSignals()
{
    // 按钮点击信号
    connect(m_toggleViewButton, &QPushButton::clicked, 
            this, &MainWidget::onToggleView);
    connect(m_startAllButton, &QPushButton::clicked, 
            this, &MainWidget::onStartAllServices);
    connect(m_stopButton, &QPushButton::clicked, 
            this, &MainWidget::onStopServices);
    
    // 代理服务器信号（使用 QueuedConnection 确保跨线程安全）
    connect(m_proxyServer, &NetworkManager::statusChanged, 
            this, &MainWidget::onProxyStatusChanged, Qt::QueuedConnection);
    connect(m_proxyServer, &NetworkManager::logMessage, 
            this, &MainWidget::onLogMessage, Qt::QueuedConnection);
    
    // 配置面板日志信号
    connect(m_configGroupPanel, &ConfigGroupPanel::logMessage, 
            this, &MainWidget::onLogMessage);
}

// ============================================================================
// 槽函数
// ============================================================================

/**
 * @brief 切换视图
 * 
 * 在配置页面和日志页面之间切换。
 */
void MainWidget::onToggleView()
{
    if (m_stackedWidget->currentIndex() == 0) {
        // 当前是配置页面，切换到日志页面
        m_stackedWidget->setCurrentIndex(1);
        m_toggleViewButton->setText(QStringLiteral("查看配置"));
    } else {
        // 当前是日志页面，切换到配置页面
        m_stackedWidget->setCurrentIndex(0);
        m_toggleViewButton->setText(QStringLiteral("查看日志"));
    }
}

/**
 * @brief 处理日志消息
 * 
 * 将日志消息转发到日志面板显示。
 * 
 * @param message 日志消息
 * @param level 日志级别
 */
void MainWidget::onLogMessage(const QString& message, LogLevel level)
{
    if (m_logPanel) {
        m_logPanel->appendLog(message, level);
    }
}

/**
 * @brief 处理代理状态变化
 * 
 * 更新 UI 状态以反映代理服务器的运行状态。
 * 
 * @param running 代理是否正在运行
 */
void MainWidget::onProxyStatusChanged(bool running)
{
    m_servicesRunning = running;
    updateStatusLabel(running);
    updateButtonStates(running);
}

/**
 * @brief 一键启动所有服务
 * 
 * 执行完整的启动流程：验证配置、生成证书、安装证书、修改 hosts、启动代理。
 * 
 * @details
 * 启动流程（5 步）：
 * 1. 验证配置：检查是否有有效的代理配置
 * 2. 生成证书：检查/生成 CA 证书和服务器证书
 * 3. 安装证书：将 CA 证书安装到系统信任存储
 * 4. 修改 hosts：添加域名劫持条目
 * 5. 启动代理：启动 HTTPS 代理服务器
 * 
 * 任何步骤失败都会中止流程并恢复已做的修改。
 */
void MainWidget::onStartAllServices()
{
    // 禁用启动按钮，防止重复点击
    m_startAllButton->setEnabled(false);
    
    onLogMessage(QStringLiteral("========== 开始启动服务 =========="), LogLevel::Info);
    
    // ========== Step 1: 验证配置 ==========
    onLogMessage(QStringLiteral("[1/5] 验证配置..."), LogLevel::Info);
    
    // 获取当前配置
    const auto& config = m_configManager->getConfig();
    
    // 检查是否有配置
    if (config.proxyConfigs.empty()) {
        onLogMessage(QStringLiteral("错误: 没有代理配置，请至少添加一个配置"), LogLevel::Error);
        m_startAllButton->setEnabled(true);
        return;
    }
    
    // 获取有效配置（过滤掉无效的）
    std::vector<ProxyConfigItem> validConfigs = config.getValidConfigs();
    if (validConfigs.empty()) {
        onLogMessage(QStringLiteral("错误: 没有有效的代理配置"), LogLevel::Error);
        m_startAllButton->setEnabled(true);
        return;
    }
    
    onLogMessage(QStringLiteral("配置验证通过，共 %1 个有效配置")
                 .arg(static_cast<int>(validConfigs.size())), LogLevel::Info);
    
    // ========== Step 2: 生成证书 ==========
    onLogMessage(QStringLiteral("[2/5] 检查/生成证书..."), LogLevel::Info);
    
    // 检查证书是否已存在
    if (m_certManager->allCertsExist()) {
        onLogMessage(QStringLiteral("证书已存在，跳过生成"), LogLevel::Info);
    } else {
        // 生成所有证书
        auto certResult = m_certManager->generateAllCerts();
        if (!certResult.ok) {
            onLogMessage(QStringLiteral("错误: 证书生成失败 - %1")
                         .arg(QString::fromStdString(certResult.message)), LogLevel::Error);
            m_startAllButton->setEnabled(true);
            return;
        }
        onLogMessage(QStringLiteral("证书生成成功"), LogLevel::Info);
    }
    
    // ========== Step 3: 安装 CA 证书 ==========
    onLogMessage(QStringLiteral("[3/5] 安装 CA 证书..."), LogLevel::Info);
    
    // 检查 CA 证书是否已安装
    if (m_certManager->isCACertInstalled()) {
        onLogMessage(QStringLiteral("CA 证书已安装，跳过安装"), LogLevel::Info);
    } else {
        // 安装 CA 证书到系统信任存储
        auto installResult = m_certManager->installCACert();
        if (!installResult.ok) {
            onLogMessage(QStringLiteral("错误: CA 证书安装失败 - %1")
                         .arg(QString::fromStdString(installResult.message)), LogLevel::Error);
            m_startAllButton->setEnabled(true);
            return;
        }
        onLogMessage(QStringLiteral("CA 证书安装成功"), LogLevel::Info);
    }
    
    // ========== Step 4: 修改 hosts 文件 ==========
    onLogMessage(QStringLiteral("[4/5] 修改 hosts 文件..."), LogLevel::Info);
    
    // 添加 hosts 条目
    if (!m_hostsManager->addEntry()) {
        onLogMessage(QStringLiteral("错误: hosts 文件修改失败"), LogLevel::Error);
        m_startAllButton->setEnabled(true);
        return;
    }
    
    // 标记 hosts 已修改，用于后续清理
    m_hostsModified = true;
    onLogMessage(QStringLiteral("hosts 文件修改成功"), LogLevel::Info);
    
    // ========== Step 5: 启动代理服务器 ==========
    onLogMessage(QStringLiteral("[5/5] 启动代理服务器..."), LogLevel::Info);
    
    // 获取运行时选项
    auto uiOptions = m_runtimeOptionsPanel->getOptions();
    
    // 构建代理服务器配置
    ProxyServerConfig proxyConfig;
    proxyConfig.proxyConfigs = validConfigs;
    proxyConfig.options = uiOptions;
    proxyConfig.certPath = m_certManager->serverCertPath();
    proxyConfig.keyPath = m_certManager->serverKeyPath();
    
    // 检查端口是否被占用
    if (NetworkManager::isPortInUse(proxyConfig.options.proxyPort)) {
        onLogMessage(QStringLiteral("错误: 端口 %1 已被占用")
                     .arg(proxyConfig.options.proxyPort), LogLevel::Error);
        // 恢复 hosts 文件
        m_hostsManager->removeEntry();
        m_hostsModified = false;
        m_startAllButton->setEnabled(true);
        return;
    }
    
    // 启动代理服务器
    if (!m_proxyServer->start(proxyConfig)) {
        onLogMessage(QStringLiteral("错误: 代理服务器启动失败"), LogLevel::Error);
        // 恢复 hosts 文件
        m_hostsManager->removeEntry();
        m_hostsModified = false;
        m_startAllButton->setEnabled(true);
        return;
    }
    
    // ========== 启动成功 ==========
    onLogMessage(QStringLiteral("========== 所有服务启动成功 =========="), LogLevel::Info);
    onLogMessage(QStringLiteral("代理服务器监听: 0.0.0.0:%1")
                 .arg(proxyConfig.options.proxyPort), LogLevel::Info);
    
    // 输出所有配置的映射关系
    for (const auto& cfg : validConfigs) {
        onLogMessage(QStringLiteral("  - %1: %2 -> %3")
                     .arg(QString::fromStdString(cfg.name), 
                          QString::fromStdString(cfg.localUrl), 
                          QString::fromStdString(cfg.mappedUrl)), LogLevel::Info);
    }
}

/**
 * @brief 停止所有服务
 * 
 * 停止代理服务器并恢复 hosts 文件。
 */
void MainWidget::onStopServices()
{
    onLogMessage(QStringLiteral("========== 停止服务 =========="), LogLevel::Info);
    
    // 停止代理服务器
    if (m_proxyServer && m_proxyServer->isRunning()) {
        m_proxyServer->stop();
        onLogMessage(QStringLiteral("代理服务器已停止"), LogLevel::Info);
    }
    
    // 恢复 hosts 文件
    if (m_hostsModified && m_hostsManager) {
        if (m_hostsManager->removeEntry()) {
            onLogMessage(QStringLiteral("hosts 文件已恢复"), LogLevel::Info);
        } else {
            onLogMessage(QStringLiteral("警告: hosts 文件恢复失败"), LogLevel::Warning);
        }
        m_hostsModified = false;
    }
    
    onLogMessage(QStringLiteral("========== 服务已停止 =========="), LogLevel::Info);
}

// ============================================================================
// 状态更新
// ============================================================================

/**
 * @brief 更新状态标签
 * 
 * 根据代理运行状态更新状态标签的文本和样式。
 * 
 * @param running 代理是否正在运行
 */
void MainWidget::updateStatusLabel(bool running)
{
    // 检查状态标签是否存在
    if (!m_statusLabel) return;
    
    if (running) {
        // 运行中：绿色粗体
        m_statusLabel->setText(QStringLiteral("代理状态: 运行中"));
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }"
    );
    } else {
        // 已停止：灰色
        m_statusLabel->setText(QStringLiteral("代理状态: 已停止"));
        m_statusLabel->setStyleSheet("QLabel { color: gray; }");
    }
}

/**
 * @brief 更新按钮状态
 * 
 * 根据服务运行状态启用/禁用相关按钮。
 * 
 * @param servicesRunning 服务是否正在运行
 */
void MainWidget::updateButtonStates(bool servicesRunning)
{
    // 服务运行时禁用启动按钮，启用停止按钮
    // 服务停止时启用启动按钮，禁用停止按钮
    m_startAllButton->setEnabled(!servicesRunning);
    m_stopButton->setEnabled(servicesRunning);
}

// ============================================================================
// 资源管理
// ============================================================================

/**
 * @brief 窗口关闭事件处理
 * 
 * 在窗口关闭前检查服务状态，如果服务正在运行则提示用户确认。
 * 
 * @param event 关闭事件
 */
void MainWidget::closeEvent(QCloseEvent* event)
{
    // 如果服务正在运行，提示用户确认
    if (m_servicesRunning) {
        auto reply = QMessageBox::question(
            this,
            QStringLiteral("确认退出"),
            QStringLiteral("代理服务正在运行，确定要退出吗？\n"
                           "退出将自动停止服务并恢复 hosts 文件。"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // 默认选择"否"
        );
        
        // 用户选择不退出
        if (reply == QMessageBox::No) {
            event->ignore();  // 忽略关闭事件
            return;
        }
    }
    
    // 清理资源
    cleanup();
    
    // 接受关闭事件
    event->accept();
}

/**
 * @brief 清理资源
 * 
 * 停止代理服务器并恢复 hosts 文件。
 * 在窗口关闭和析构时调用。
 */
void MainWidget::cleanup()
{
    // 停止代理服务器
    if (m_proxyServer && m_proxyServer->isRunning()) {
        m_proxyServer->stop();
    }
    
    // 恢复 hosts 文件
    if (m_hostsModified && m_hostsManager) {
        m_hostsManager->removeEntry();
        m_hostsModified = false;
    }
}
