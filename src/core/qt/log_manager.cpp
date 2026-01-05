/**
 * @file log_manager.cpp
 * @brief 日志管理器实现文件
 * 
 * 本文件实现了 LogManager 类的所有方法，以及 ILogManager 接口的静态工具方法。
 * 
 * @details
 * 实现内容：
 * 
 * 1. ILogManager 静态方法
 *    - formatMessage(): 格式化日志消息
 *    - timestamp(): 生成时间戳
 *    - levelToString(): 日志级别转字符串
 * 
 * 2. LogManager 单例实现
 *    - instance(): 获取单例实例
 *    - 构造函数和析构函数
 * 
 * 3. 初始化和关闭
 *    - initialize(): 初始化日志系统
 *    - shutdown(): 关闭日志系统
 * 
 * 4. 日志记录方法
 *    - log(): 通用日志记录
 *    - debug/info/warning/error(): 特定级别日志
 * 
 * 5. 文件写入
 *    - writeToFile(): 写入主日志文件
 *    - writeToErrorFile(): 写入错误日志文件
 * 
 * 日志格式：
 * 
 * [HH:MM:SS.mmm] [LEVEL] message
 * 
 * 示例：[14:30:25.123] [INFO] 服务器启动成功
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "log_manager.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <chrono>
#include <iomanip>
#include <sstream>

// ============================================================================
// ILogManager 静态方法实现
// ============================================================================

std::string ILogManager::formatMessage(const std::string& message, LogLevel level)
{
    return "[" + timestamp() + "] [" + levelToString(level) + "] " + message;
}

std::string ILogManager::timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string ILogManager::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

// ============================================================================
// LogManager 实现
// ============================================================================

LogManager& LogManager::instance()
{
    static LogManager instance;
    return instance;
}

LogManager::LogManager()
    : QObject(nullptr)
    , m_initialized(false)
    , m_errorLogInitialized(false)
    , m_callback(nullptr)
{
}

LogManager::~LogManager()
{
    shutdown();
}

bool LogManager::initialize(const std::string& logFilePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    QString path = QString::fromStdString(logFilePath);
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }
    
    m_logFile.setFileName(path);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    
    m_logStream.setDevice(&m_logFile);
    m_initialized = true;
    
    return true;
}

bool LogManager::initialize(const std::string& logFilePath, 
                           const std::string& errorLogFilePath)
{
    if (!initialize(logFilePath)) {
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString errorPath = QString::fromStdString(errorLogFilePath);
    QFileInfo errorFileInfo(errorPath);
    QDir errorDir = errorFileInfo.dir();
    if (!errorDir.exists() && !errorDir.mkpath(".")) {
        return false;
    }
    
    m_errorLogFile.setFileName(errorPath);
    if (!m_errorLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    
    m_errorLogStream.setDevice(&m_errorLogFile);
    m_errorLogInitialized = true;
    
    return true;
}

void LogManager::shutdown()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        m_logStream.flush();
        m_logFile.close();
        m_initialized = false;
    }
    
    if (m_errorLogInitialized) {
        m_errorLogStream.flush();
        m_errorLogFile.close();
        m_errorLogInitialized = false;
    }
    
    // 清除回调，避免悬空引用
    m_callback = nullptr;
}

void LogManager::log(const std::string& message, LogLevel level)
{
    std::string formatted = formatMessage(message, level);
    
    writeToFile(formatted);
    
    // 调用回调
    if (m_callback) {
        m_callback(formatted, level);
    }
    
    // 发出 Qt 信号
    emit logMessageSignal(QString::fromStdString(formatted), level);
}

void LogManager::debug(const std::string& message)
{
    log(message, LogLevel::Debug);
}

void LogManager::info(const std::string& message)
{
    log(message, LogLevel::Info);
}

void LogManager::warning(const std::string& message)
{
    log(message, LogLevel::Warning);
}

void LogManager::error(const std::string& message)
{
    std::string formatted = formatMessage(message, LogLevel::Error);
    
    writeToFile(formatted);
    writeToErrorFile(formatted);
    
    if (m_callback) {
        m_callback(formatted, LogLevel::Error);
    }
    
    emit logMessageSignal(QString::fromStdString(formatted), LogLevel::Error);
}

bool LogManager::hasErrorLogFile() const
{
    QMutexLocker locker(&m_mutex);
    return m_errorLogInitialized;
}

void LogManager::setLogCallback(LogCallback callback)
{
    QMutexLocker locker(&m_mutex);
    m_callback = std::move(callback);
}

void LogManager::writeToFile(const std::string& formattedMessage)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_initialized) {
        m_logStream << QString::fromStdString(formattedMessage) << "\n";
        m_logStream.flush();
    }
}

void LogManager::writeToErrorFile(const std::string& formattedMessage)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_errorLogInitialized) {
        m_errorLogStream << QString::fromStdString(formattedMessage) << "\n";
        m_errorLogStream.flush();
    }
}
