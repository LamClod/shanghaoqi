/**
 * @file runtime_options_panel.cpp
 * @brief 运行时选项面板实现
 * 
 * 实现运行时选项面板的所有功能。
 * 
 * @details
 * 实现细节：
 * 
 * UI 初始化：
 * - 使用 QHBoxLayout 水平排列三个 QGroupBox
 * - 每个分组使用 QVBoxLayout 或 QFormLayout
 * - 连接池大小使用 QSpinBox，范围 5-15
 * - 流模式使用 QComboBox，存储枚举值作为 userData
 * 
 * 信号连接：
 * - 所有控件的值变化信号连接到 onOptionChanged
 * - onOptionChanged 发出 optionsChanged 信号
 * 
 * 选项读写：
 * - getOptions：从 UI 控件读取值，构造 RuntimeOptions
 * - setOptions：使用 QSignalBlocker 阻止信号，设置 UI 控件值
 * 
 * 默认值：
 * - HTTP/2：启用
 * - 连接池：启用，大小 10
 * - 流模式：跟随客户端
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "runtime_options_panel.h"

// Qt 布局
#include <QVBoxLayout>   // 垂直布局
#include <QHBoxLayout>   // 水平布局
#include <QGroupBox>     // 分组框
#include <QLabel>        // 标签
#include <QFormLayout>   // 表单布局

// ============================================================================
// 构造函数
// ============================================================================

/**
 * @brief 构造函数
 * 
 * 初始化运行时选项面板。
 * 
 * @param parent 父窗口部件
 */
RuntimeOptionsPanel::RuntimeOptionsPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

// ============================================================================
// UI 初始化
// ============================================================================

/**
 * @brief 初始化 UI
 * 
 * 创建和布局所有 UI 组件。
 * 
 * @details
 * 布局结构：
 * - 主布局：QHBoxLayout（水平排列三个分组）
 *   - 基本选项组：调试模式、SSL 验证
 *   - 网络选项组：HTTP/2、连接池
 *   - 流模式组：上游流模式、下游流模式
 */
void RuntimeOptionsPanel::setupUi()
{
    // 创建主布局（水平排列）
    auto* mainLayout = new QHBoxLayout(this);
    
    // ========== 基本选项分组 ==========
    auto* basicGroup = new QGroupBox(tr("基本选项"), this);
    auto* basicLayout = new QVBoxLayout(basicGroup);
    
    // 调试模式复选框
    m_debugModeCheck = new QCheckBox(tr("开启调试模式"), this);
    m_debugModeCheck->setToolTip(tr("输出详细的请求/响应日志"));
    basicLayout->addWidget(m_debugModeCheck);
    
    // SSL 严格模式复选框
    m_disableSslStrictCheck = new QCheckBox(tr("关闭SSL严格模式"), this);
    m_disableSslStrictCheck->setToolTip(tr("不验证上游SSL证书"));
    basicLayout->addWidget(m_disableSslStrictCheck);

    // FluxFix 整流器复选框
    m_enableFluxFixCheck = new QCheckBox(tr("启用FluxFix整流"), this);
    m_enableFluxFixCheck->setChecked(true);  // 默认启用
    m_enableFluxFixCheck->setToolTip(tr("完整的FluxFix整流器支持"));
    basicLayout->addWidget(m_enableFluxFixCheck);

    mainLayout->addWidget(basicGroup);
    
    // ========== 网络选项分组 ==========
    auto* networkGroup = new QGroupBox(tr("网络选项"), this);
    auto* networkLayout = new QVBoxLayout(networkGroup);
    
    // HTTP/2 复选框（默认启用）
    m_enableHttp2Check = new QCheckBox(tr("启用HTTP/2"), this);
    m_enableHttp2Check->setChecked(true);  // 默认启用
    networkLayout->addWidget(m_enableHttp2Check);
    
    // 连接池复选框（默认启用）
    m_enableConnectionPoolCheck = new QCheckBox(tr("启用连接池"), this);
    m_enableConnectionPoolCheck->setChecked(true);  // 默认启用
    networkLayout->addWidget(m_enableConnectionPoolCheck);
    
    // 连接池大小设置
    auto* poolSizeLayout = new QHBoxLayout();
    poolSizeLayout->addWidget(new QLabel(tr("连接池大小:"), this));
    
    // 连接池大小数字输入框
    m_connectionPoolSizeSpin = new QSpinBox(this);
    m_connectionPoolSizeSpin->setRange(5, 15);   // 范围 5-15
    m_connectionPoolSizeSpin->setValue(10);       // 默认值 10
    poolSizeLayout->addWidget(m_connectionPoolSizeSpin);
    poolSizeLayout->addStretch();  // 添加弹性空间
    networkLayout->addLayout(poolSizeLayout);
    
    mainLayout->addWidget(networkGroup);
    
    // ========== 流模式选项分组 ==========
    auto* streamGroup = new QGroupBox(tr("流模式"), this);
    auto* streamLayout = new QFormLayout(streamGroup);
    
    // 上游流模式下拉框
    m_upstreamStreamCombo = new QComboBox(this);
    // 添加选项，使用枚举值作为 userData
    m_upstreamStreamCombo->addItem(tr("跟随客户端"), 
                                    static_cast<int>(StreamMode::FollowClient));
    m_upstreamStreamCombo->addItem(tr("强制开启"), 
                                    static_cast<int>(StreamMode::ForceOn));
    m_upstreamStreamCombo->addItem(tr("强制关闭"), 
                                    static_cast<int>(StreamMode::ForceOff));
    streamLayout->addRow(tr("上游:"), m_upstreamStreamCombo);
    
    // 下游流模式下拉框
    m_downstreamStreamCombo = new QComboBox(this);
    m_downstreamStreamCombo->addItem(tr("跟随客户端"), 
                                      static_cast<int>(StreamMode::FollowClient));
    m_downstreamStreamCombo->addItem(tr("强制开启"), 
                                      static_cast<int>(StreamMode::ForceOn));
    m_downstreamStreamCombo->addItem(tr("强制关闭"), 
                                      static_cast<int>(StreamMode::ForceOff));
    streamLayout->addRow(tr("下游:"), m_downstreamStreamCombo);
    
    mainLayout->addWidget(streamGroup);
    
    // ========== 连接信号 ==========
    // 将所有控件的值变化信号连接到 onOptionChanged 槽
    connect(m_debugModeCheck, &QCheckBox::stateChanged, 
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_disableSslStrictCheck, &QCheckBox::stateChanged,
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_enableFluxFixCheck, &QCheckBox::stateChanged,
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_enableHttp2Check, &QCheckBox::stateChanged, 
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_enableConnectionPoolCheck, &QCheckBox::stateChanged, 
            this, &RuntimeOptionsPanel::onOptionChanged);
    
    // QSpinBox 和 QComboBox 使用 QOverload 指定重载版本
    connect(m_connectionPoolSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_upstreamStreamCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_downstreamStreamCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
}

// ============================================================================
// 选项读写
// ============================================================================

/**
 * @brief 获取当前选项
 * 
 * 从 UI 控件读取当前设置，返回 RuntimeOptions 结构体。
 * 
 * @return 当前的运行时选项
 */
RuntimeOptions RuntimeOptionsPanel::getOptions() const
{
    RuntimeOptions options;
    
    // 从复选框读取布尔值
    options.debugMode = m_debugModeCheck->isChecked();
    options.disableSslStrict = m_disableSslStrictCheck->isChecked();
    options.enableHttp2 = m_enableHttp2Check->isChecked();
    options.enableConnectionPool = m_enableConnectionPoolCheck->isChecked();
    options.enableFluxFix = m_enableFluxFixCheck->isChecked();
    
    // 从数字输入框读取连接池大小
    options.connectionPoolSize = m_connectionPoolSizeSpin->value();
    
    // 从下拉框读取流模式（使用 userData 中存储的枚举值）
    options.upstreamStreamMode = static_cast<StreamMode>(
        m_upstreamStreamCombo->currentData().toInt());
    options.downstreamStreamMode = static_cast<StreamMode>(
        m_downstreamStreamCombo->currentData().toInt());
    
    return options;
}

/**
 * @brief 设置选项
 * 
 * 将 RuntimeOptions 结构体的值应用到 UI 控件。
 * 
 * @param options 要设置的运行时选项
 * 
 * @details
 * 使用 QSignalBlocker 阻止信号发送，避免在设置过程中触发 optionsChanged 信号。
 * 这样可以防止不必要的信号级联。
 */
void RuntimeOptionsPanel::setOptions(const RuntimeOptions& options)
{
    // 使用 QSignalBlocker 阻止信号
    // 这些对象在作用域结束时自动恢复信号
    const QSignalBlocker blocker1(m_debugModeCheck);
    const QSignalBlocker blocker2(m_disableSslStrictCheck);
    const QSignalBlocker blocker3(m_enableHttp2Check);
    const QSignalBlocker blocker4(m_enableConnectionPoolCheck);
    const QSignalBlocker blocker5(m_connectionPoolSizeSpin);
    const QSignalBlocker blocker6(m_upstreamStreamCombo);
    const QSignalBlocker blocker7(m_downstreamStreamCombo);
    const QSignalBlocker blocker8(m_enableFluxFixCheck);

    // 设置复选框状态
    m_debugModeCheck->setChecked(options.debugMode);
    m_disableSslStrictCheck->setChecked(options.disableSslStrict);
    m_enableHttp2Check->setChecked(options.enableHttp2);
    m_enableConnectionPoolCheck->setChecked(options.enableConnectionPool);
    m_enableFluxFixCheck->setChecked(options.enableFluxFix);
    
    // 设置连接池大小
    m_connectionPoolSizeSpin->setValue(options.connectionPoolSize);
    
    // 设置上游流模式（通过 userData 查找索引）
    int upIdx = m_upstreamStreamCombo->findData(
        static_cast<int>(options.upstreamStreamMode));
    if (upIdx >= 0) {
        m_upstreamStreamCombo->setCurrentIndex(upIdx);
    }
    
    // 设置下游流模式
    int downIdx = m_downstreamStreamCombo->findData(
        static_cast<int>(options.downstreamStreamMode));
    if (downIdx >= 0) {
        m_downstreamStreamCombo->setCurrentIndex(downIdx);
    }
}

// ============================================================================
// 槽函数
// ============================================================================

/**
 * @brief 选项变化处理
 * 
 * 当任何 UI 组件的值发生变化时调用，发出 optionsChanged 信号。
 */
void RuntimeOptionsPanel::onOptionChanged()
{
    // 发出选项变化信号
    emit optionsChanged();
}
