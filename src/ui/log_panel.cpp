/**
 * @file log_panel.cpp
 * @brief 日志面板实现
 * 
 * 实现日志面板的所有功能。
 * 
 * @details
 * 实现细节：
 * 
 * UI 初始化：
 * - 使用 QVBoxLayout 布局
 * - QTextEdit 设置为只读模式
 * - 使用等宽字体（Consolas）便于阅读
 * - 应用深色主题样式
 * 
 * 日志追加：
 * - 检查消息是否已包含时间戳
 * - 根据日志级别设置颜色
 * - 使用 HTML 格式化输出
 * - 自动滚动到底部
 * 
 * 信号连接：
 * - 自动连接 LogManager::logMessageSignal 信号
 * - 支持外部手动调用 appendLog
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "log_panel.h"

// Qt 布局和控件
#include <QVBoxLayout>   // 垂直布局
#include <QScrollBar>    // 滚动条控制
#include <QFont>         // 字体设置

// ============================================================================
// 构造函数
// ============================================================================

/**
 * @brief 构造函数
 * 
 * 初始化日志面板，设置 UI 组件和样式。
 * 
 * @param parent 父窗口部件
 * 
 * @details
 * 初始化步骤：
 * 1. 创建 QTextEdit 用于显示日志
 * 2. 设置布局（无边距）
 * 3. 配置文本编辑器属性（只读、不换行）
 * 4. 设置等宽字体（Consolas）
 * 5. 应用深色主题样式
 * 6. 连接 LogManager 信号
 */
LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
    , m_logText(new QTextEdit(this))  // 创建日志文本显示区域
{
    // 创建垂直布局
    auto* layout = new QVBoxLayout(this);
    
    // 设置布局边距为 0，让日志区域填满整个面板
    layout->setContentsMargins(0, 0, 0, 0);
    
    // 将文本编辑器添加到布局
    layout->addWidget(m_logText);
    
    // 设置为只读模式，用户不能编辑日志内容
    m_logText->setReadOnly(true);
    
    // 禁用自动换行，保持日志格式整齐
    m_logText->setLineWrapMode(QTextEdit::NoWrap);
    
    // ========== 设置等宽字体 ==========
    // 使用 Consolas 字体，这是 Windows 上常用的编程字体
    QFont font("Consolas", 9);
    
    // 设置字体提示为等宽字体，如果 Consolas 不可用会使用其他等宽字体
    font.setStyleHint(QFont::Monospace);
    m_logText->setFont(font);
    
    // ========== 应用深色主题样式 ==========
    // 使用 VS Code 风格的深色配色
    m_logText->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"  // 深灰背景
        "  color: #d4d4d4;"              // 浅灰文字
        "  border: 1px solid #3c3c3c;"   // 深灰边框
        "}"
    );
    
    // 连接 LogManager 的日志信号到本面板的 appendLog 槽
    // 这样所有通过 LogManager 输出的日志都会显示在面板中
    connect(&LogManager::instance(), &LogManager::logMessageSignal, 
            this, &LogPanel::appendLog);
}

// ============================================================================
// 公共槽函数
// ============================================================================

/**
 * @brief 追加日志消息
 * 
 * 将日志消息添加到面板末尾，并自动滚动到底部。
 * 
 * @param message 日志消息内容
 * @param level 日志级别，用于确定显示颜色
 * 
 * @details
 * 处理流程：
 * 1. 获取日志级别对应的颜色
 * 2. 检查消息是否已包含时间戳
 * 3. 如果没有时间戳，使用 ILogManager::formatMessage 添加
 * 4. 使用 HTML span 标签设置颜色
 * 5. 追加到文本编辑器
 * 6. 滚动到底部
 */
void LogPanel::appendLog(const QString& message, LogLevel level)
{
    // 获取日志级别对应的颜色
    QString color = levelColor(level);
    QString text = message;
    
    // 检查消息是否已包含时间戳（以 "[" 开头）
    // 如果没有，使用 ILogManager::formatMessage 添加时间戳和级别标签
    if (!message.startsWith("[")) {
        text = QString::fromStdString(
            ILogManager::formatMessage(message.toStdString(), level)
        );
    }
    
    // 使用 HTML 格式化日志，设置颜色
    // toHtmlEscaped() 转义特殊字符，防止 HTML 注入
    QString html = QStringLiteral("<span style=\"color: %1;\">%2</span>")
                       .arg(color, text.toHtmlEscaped());
    
    // 追加到文本编辑器
    m_logText->append(html);
    
    // 滚动到底部，显示最新日志
    scrollToBottom();
}

/**
 * @brief 清空所有日志
 * 
 * 清除面板中的所有日志内容。
 */
void LogPanel::clear()
{
    m_logText->clear();
}

// ============================================================================
// 私有方法
// ============================================================================

/**
 * @brief 获取日志级别对应的颜色
 * 
 * 根据日志级别返回对应的颜色代码。
 * 
 * @param level 日志级别
 * @return 颜色的十六进制字符串
 * 
 * @details
 * 颜色方案（深色主题）：
 * - Debug: #808080 (灰色) - 调试信息，不太重要
 * - Info: #d4d4d4 (浅灰) - 普通信息
 * - Warning: #dcdcaa (黄色) - 警告，需要注意
 * - Error: #f14c4c (红色) - 错误，需要处理
 */
QString LogPanel::levelColor(LogLevel level) const
{
    switch (level) {
        case LogLevel::Debug:   
            return "#808080";  // 灰色 - 调试信息
        case LogLevel::Info:    
            return "#d4d4d4";  // 浅灰 - 普通信息
        case LogLevel::Warning: 
            return "#dcdcaa";  // 黄色 - 警告信息
        case LogLevel::Error:   
            return "#f14c4c";  // 红色 - 错误信息
        default:                
            return "#d4d4d4";  // 默认浅灰
    }
}

/**
 * @brief 滚动到底部
 * 
 * 将日志视图滚动到最新的日志条目。
 * 
 * @details
 * 通过获取垂直滚动条并设置其值为最大值来实现滚动到底部。
 */
void LogPanel::scrollToBottom()
{
    // 获取垂直滚动条
    QScrollBar* scrollBar = m_logText->verticalScrollBar();
    
    // 设置滚动条位置为最大值（底部）
    scrollBar->setValue(scrollBar->maximum());
}
