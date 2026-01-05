/**
 * @file log_panel.h
 * @brief 日志面板 - 实时日志显示组件
 * 
 * 本文件定义了日志面板组件，提供实时日志显示功能。
 * 
 * @details
 * 功能概述：
 * - 实时显示应用程序日志
 * - 不同日志级别使用不同颜色区分
 * - 自动滚动到最新日志
 * - 日志清空功能
 * 
 * 颜色方案（深色主题）：
 * | 级别 | 颜色 | 十六进制 |
 * |------|------|----------|
 * | Debug | 灰色 | #808080 |
 * | Info | 浅灰 | #d4d4d4 |
 * | Warning | 黄色 | #dcdcaa |
 * | Error | 红色 | #f14c4c |
 * 
 * 样式设置：
 * - 背景色：#1e1e1e（深灰）
 * - 字体：Consolas 9pt（等宽字体）
 * - 边框：1px solid #3c3c3c
 * - 不自动换行
 * 
 * 使用示例：
 * @code
 * LogPanel* logPanel = new LogPanel(this);
 * 
 * // 连接日志信号
 * connect(&LogManager::instance(), &LogManager::logMessageSignal,
 *         logPanel, &LogPanel::appendLog);
 * 
 * // 或手动添加日志
 * logPanel->appendLog("服务器启动", LogLevel::Info);
 * logPanel->appendLog("连接失败", LogLevel::Error);
 * 
 * // 清空日志
 * logPanel->clear();
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see LogManager 日志管理器
 */

#ifndef LOG_PANEL_H
#define LOG_PANEL_H

#include <QWidget>
#include <QTextEdit>
#include "../core/qt/log_manager.h"

/**
 * @brief 日志面板组件
 * 
 * 显示带时间戳的日志消息，不同级别用不同颜色区分。
 * 
 * @details
 * 功能特性：
 * - 自动连接 LogManager 信号
 * - 支持手动追加日志
 * - 自动滚动到最新日志
 * - 支持清空日志
 * 
 * 样式设置：
 * - 深色主题背景
 * - 等宽字体（Consolas）
 * - 不自动换行
 */
class LogPanel : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * 
     * 创建日志面板，初始化UI组件。
     * 
     * @param parent 父窗口部件
     */
    explicit LogPanel(QWidget* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~LogPanel() override = default;

public slots:
    /**
     * @brief 追加日志消息
     * 
     * 将日志消息添加到面板末尾，并自动滚动到底部。
     * 消息会根据日志级别显示不同颜色。
     * 
     * @param message 日志消息（通常已包含时间戳和级别）
     * @param level 日志级别，用于确定显示颜色
     */
    void appendLog(const QString& message, LogLevel level = LogLevel::Info);
    
    /**
     * @brief 清空所有日志
     * 
     * 清除面板中的所有日志内容。
     */
    void clear();

private:
    /**
     * @brief 获取日志级别对应的颜色
     * 
     * @param level 日志级别
     * @return 颜色的十六进制字符串（如 "#FF0000"）
     */
    QString levelColor(LogLevel level) const;
    
    /**
     * @brief 滚动到底部
     * 
     * 将日志视图滚动到最新的日志条目。
     */
    void scrollToBottom();

    QTextEdit* m_logText;    ///< 日志文本显示区域
};

#endif // LOG_PANEL_H
