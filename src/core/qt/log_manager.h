/**
 * @file log_manager.h
 * @brief 日志管理器头文件
 * 
 * 本文件定义了日志管理器的接口实现类，提供完整的日志记录功能。
 * 
 * @details
 * 功能概述：
 * 
 * 日志管理器是应用程序的核心基础设施组件，负责：
 * - 多级别日志记录（Debug、Info、Warning、Error）
 * - 日志文件持久化存储
 * - 线程安全的并发写入
 * - 回调通知机制
 * - 与 UI 层的信号槽集成
 * 
 * 日志级别说明：
 * 
 * | 级别    | 用途                                     |
 * |---------|------------------------------------------|
 * | Debug   | 调试信息，仅在开发阶段使用               |
 * | Info    | 一般信息，记录正常操作流程               |
 * | Warning | 警告信息，表示潜在问题但不影响运行       |
 * | Error   | 错误信息，表示操作失败需要关注           |
 * 
 * 日志文件：
 * 
 * - 主日志文件：记录所有级别的日志
 * - 错误日志文件：仅记录 Error 级别的日志，便于快速定位问题
 * 
 * 线程安全：
 * 
 * 所有日志写入操作都通过互斥锁保护，确保多线程环境下的数据一致性。
 * 
 * 使用示例：
 * @code
 * // 初始化日志系统
 * LogManager::instance().initialize("/path/to/app.log", "/path/to/error.log");
 * 
 * // 记录日志
 * LogManager::instance().info("应用程序启动");
 * LogManager::instance().error("连接失败");
 * 
 * // 使用便捷宏
 * LOG_INFO("服务器启动成功");
 * LOG_ERROR("配置文件解析失败");
 * 
 * // 设置回调
 * LogManager::instance().setLogCallback([](const std::string& msg, LogLevel level) {
 *     // 自定义处理逻辑
 * });
 * 
 * // 关闭日志系统
 * LogManager::instance().shutdown();
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see ILogManager 日志管理器接口
 * @see LogLevel 日志级别枚举
 */

#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include "../interfaces/i_log_manager.h"

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>

/**
 * @brief 日志管理器实现类
 * 
 * 实现 ILogManager 接口的单例类，提供完整的日志记录功能。
 * 
 * @details
 * 类设计说明：
 * 
 * 本类采用单例模式设计，确保整个应用程序只有一个日志管理器实例。
 * 同时继承 QObject 以支持 Qt 信号槽机制，便于与 UI 层集成。
 * 
 * 主要特性：
 * 
 * 1. 单例模式
 *    - 通过 instance() 静态方法获取唯一实例
 *    - 禁止拷贝和赋值操作
 * 
 * 2. 双日志文件
 *    - 主日志文件记录所有级别的日志
 *    - 错误日志文件仅记录 Error 级别
 * 
 * 3. 线程安全
 *    - 使用 QMutex 保护文件写入操作
 *    - 支持多线程并发调用
 * 
 * 4. 回调机制
 *    - 支持设置日志回调函数
 *    - 每条日志都会触发回调
 * 
 * 5. Qt 信号
 *    - 发出 logMessageSignal 信号
 *    - 便于 UI 组件实时显示日志
 * 
 * @note 必须在使用前调用 initialize() 方法
 * @note 应用退出前应调用 shutdown() 方法
 * 
 * @see ILogManager 日志管理器接口
 */
class LogManager : public QObject, public ILogManager {
    Q_OBJECT
    
public:
    /**
     * @brief 获取单例实例
     */
    static LogManager& instance();
    
    // ========== ILogManager 接口实现 ==========
    
    bool initialize(const std::string& logFilePath) override;
    bool initialize(const std::string& logFilePath, 
                   const std::string& errorLogFilePath) override;
    void shutdown() override;
    
    void log(const std::string& message, LogLevel level = LogLevel::Info) override;
    void debug(const std::string& message) override;
    void info(const std::string& message) override;
    void warning(const std::string& message) override;
    void error(const std::string& message) override;
    
    bool isInitialized() const override { return m_initialized; }
    bool hasErrorLogFile() const override;
    
    void setLogCallback(LogCallback callback) override;

signals:
    /**
     * @brief Qt 信号：日志消息
     * 
     * 用于与 Qt UI 组件集成。
     */
    void logMessageSignal(const QString& message, LogLevel level);

private:
    LogManager();
    ~LogManager() override;
    
    void writeToFile(const std::string& formattedMessage);
    void writeToErrorFile(const std::string& formattedMessage);
    
    QFile m_logFile;
    QTextStream m_logStream;
    QFile m_errorLogFile;
    QTextStream m_errorLogStream;
    mutable QMutex m_mutex;
    bool m_initialized;
    bool m_errorLogInitialized;
    LogCallback m_callback;
};

// ============================================================================
// 便捷宏定义
// ============================================================================

// 支持 QString 和 std::string 的日志宏
#define LOG_DEBUG(msg) LogManager::instance().debug(QString(msg).toStdString())
#define LOG_INFO(msg) LogManager::instance().info(QString(msg).toStdString())
#define LOG_WARNING(msg) LogManager::instance().warning(QString(msg).toStdString())
#define LOG_ERROR(msg) LogManager::instance().error(QString(msg).toStdString())

#endif // LOG_MANAGER_H
