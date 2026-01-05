/**
 * @file runtime_options_panel.h
 * @brief 运行时选项面板 - 代理服务器运行时配置界面
 * 
 * 本文件定义了运行时选项面板组件，提供代理服务器的运行时配置。
 * 
 * @details
 * 功能概述：
 * - 调试模式开关：开启后输出详细的请求/响应日志
 * - SSL 严格验证开关：允许连接使用自签名证书的服务器
 * - HTTP/2 协议开关：使用 HTTP/2 协议与上游服务器通信
 * - 连接池开关和大小设置：复用网络连接以提高性能
 * - 上游/下游流模式选择：控制 SSE 流式响应行为
 * 
 * 面板布局：
 * - 使用 QHBoxLayout 水平排列三个分组
 * - 基本选项组：调试模式、SSL 验证
 * - 网络选项组：HTTP/2、连接池
 * - 流模式组：上游流模式、下游流模式
 * 
 * 流模式说明：
 * | 模式 | 说明 |
 * |------|------|
 * | 跟随客户端 | 保持客户端请求的 stream 参数 |
 * | 强制开启 | 始终使用流式响应 |
 * | 强制关闭 | 始终使用非流式响应 |
 * 
 * 使用示例：
 * @code
 * RuntimeOptionsPanel* panel = new RuntimeOptionsPanel(this);
 * 
 * // 设置初始选项
 * RuntimeOptions options;
 * options.debugMode = true;
 * options.enableHttp2 = true;
 * panel->setOptions(options);
 * 
 * // 监听选项变化
 * connect(panel, &RuntimeOptionsPanel::optionsChanged, this, [panel]() {
 *     RuntimeOptions newOptions = panel->getOptions();
 *     // 应用新选项...
 * });
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see RuntimeOptions 运行时选项结构体
 */

#ifndef RUNTIME_OPTIONS_PANEL_H
#define RUNTIME_OPTIONS_PANEL_H

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include "../core/interfaces/i_network_manager.h"

/**
 * @brief 运行时选项面板
 * 
 * 提供代理服务器运行时选项的配置界面。
 * 
 * @details
 * 配置项说明：
 * - 调试模式：开启后输出详细的请求/响应日志
 * - 禁用 SSL 严格验证：允许连接使用自签名证书的服务器
 * - 启用 HTTP/2：使用 HTTP/2 协议与上游服务器通信
 * - 启用连接池：复用网络连接以提高性能
 * - 上游流模式：控制发送到上游的 stream 参数
 * - 下游流模式：控制返回给客户端的响应格式
 * - 连接池大小：连接池中保持的最大连接数
 * 
 * 信号机制：
 * - 当任何选项发生变化时，发出 optionsChanged 信号
 */
class RuntimeOptionsPanel : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * 创建运行时选项面板，初始化所有UI组件。
     * 
     * @param parent 父窗口部件
     */
    explicit RuntimeOptionsPanel(QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~RuntimeOptionsPanel() override = default;
    
    /**
     * @brief 获取当前选项
     * 
     * 从UI组件读取当前设置，返回RuntimeOptions结构体。
     * 
     * @return 当前的运行时选项
     */
    RuntimeOptions getOptions() const;
    
    /**
     * @brief 设置选项
     * 
     * 将RuntimeOptions结构体的值应用到UI组件。
     * 
     * @param options 要设置的运行时选项
     */
    void setOptions(const RuntimeOptions& options);

signals:
    /**
     * @brief 选项变化信号
     * 
     * 当任何选项发生变化时发出此信号。
     */
    void optionsChanged();

private slots:
    /**
     * @brief 选项变化处理
     * 
     * 当任何UI组件的值发生变化时调用，发出optionsChanged信号。
     */
    void onOptionChanged();

private:
    /**
     * @brief 初始化UI
     * 
     * 创建和布局所有UI组件。
     */
    void setupUi();
    
    // ========== UI组件 ==========
    
    QCheckBox* m_debugModeCheck;           ///< 调试模式复选框
    QCheckBox* m_disableSslStrictCheck;    ///< 禁用SSL严格验证复选框
    QCheckBox* m_enableHttp2Check;         ///< 启用HTTP/2复选框
    QCheckBox* m_enableConnectionPoolCheck; ///< 启用连接池复选框
    QComboBox* m_upstreamStreamCombo;      ///< 上游流模式下拉框
    QComboBox* m_downstreamStreamCombo;    ///< 下游流模式下拉框
    QSpinBox* m_connectionPoolSizeSpin;    ///< 连接池大小输入框
};

#endif // RUNTIME_OPTIONS_PANEL_H
