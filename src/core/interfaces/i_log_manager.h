/**
 * @file i_log_manager.h
 * @brief 日志管理器接口定义
 * 
 * 本文件定义了日志系统的纯抽象接口，为不同平台的日志实现提供统一规范。
 * 
 * @details
 * 设计理念：
 * 
 * 本接口采用纯 C++ 设计，不依赖任何外部框架（如 Qt、Boost 等），
 * 确保接口的可移植性和跨平台兼容性。具体实现可以使用任何框架，
 * 但接口层保持纯净。
 * 
 * 功能模块：
 * 
 * 1. 多级别日志记录
 *    - Debug: 调试信息，开发阶段使用
 *    - Info: 一般信息，记录正常流程
 *    - Warning: 警告信息，潜在问题
 *    - Error: 错误信息，操作失败
 * 
 * 2. 日志文件输出
 *    - 支持主日志文件
 *    - 支持独立的错误日志文件
 *    - 自动创建日志目录
 * 
 * 3. 线程安全
 *    - 接口设计考虑多线程场景
 *    - 具体实现需保证线程安全
 * 
 * 4. 回调机制
 *    - 支持设置日志回调函数
 *    - 便于与 UI 层集成
 * 
 * 静态工具方法：
 * 
 * - formatMessage(): 格式化日志消息
 * - timestamp(): 生成时间戳字符串
 * - levelToString(): 日志级别转字符串
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef I_LOG_MANAGER_H
#define I_LOG_MANAGER_H

#include <string>
#include <functional>
#include <memory>

/**
 * @brief 日志级别枚举
 * 
 * 定义日志消息的严重程度级别，用于分类和过滤日志。
 * 
 * @details
 * 级别说明：
 * 
 * | 级别    | 数值 | 用途                                     |
 * |---------|------|------------------------------------------|
 * | Debug   | 0    | 调试信息，仅在开发阶段使用               |
 * | Info    | 1    | 一般信息，记录正常操作流程               |
 * | Warning | 2    | 警告信息，表示潜在问题但不影响运行       |
 * | Error   | 3    | 错误信息，表示操作失败需要关注           |
 * 
 * 使用建议：
 * 
 * - Debug: 用于输出变量值、函数调用跟踪等调试信息
 * - Info: 用于记录重要的业务流程节点
 * - Warning: 用于记录可恢复的异常情况
 * - Error: 用于记录不可恢复的错误
 */
enum class LogLevel {
    Debug,      ///< 调试级别：详细的调试信息
    Info,       ///< 信息级别：一般操作信息
    Warning,    ///< 警告级别：潜在问题警告
    Error       ///< 错误级别：操作失败错误
};

/**
 * @brief 日志回调函数类型
 * 
 * 定义日志消息的回调函数签名，用于通知外部组件有新的日志消息。
 * 
 * @details
 * 回调函数参数：
 * 
 * - message: 格式化后的完整日志消息，包含时间戳和级别标识
 * - level: 日志级别枚举值，便于回调函数进行级别判断
 * 
 * 使用场景：
 * 
 * - UI 层实时显示日志
 * - 日志聚合和分析
 * - 自定义日志处理逻辑
 * 
 * 示例：
 * @code
 * LogCallback callback = [](const std::string& message, LogLevel level) {
 *     if (level == LogLevel::Error) {
 *         // 错误日志特殊处理
 *         sendAlert(message);
 *     }
 *     // 显示到 UI
 *     updateLogView(message);
 * };
 * @endcode
 * 
 * @note 回调函数应该快速返回，避免阻塞日志系统
 */
using LogCallback = std::function<void(const std::string& message, LogLevel level)>;

/**
 * @brief 日志管理器抽象接口
 * 
 * 定义日志记录的标准操作接口，所有平台的日志实现都必须遵循此接口。
 * 
 * @details
 * 接口设计原则：
 * 
 * 1. 纯抽象接口
 *    - 所有方法都是纯虚函数
 *    - 不包含任何实现代码
 *    - 不依赖任何 GUI 框架
 * 
 * 2. 禁止拷贝
 *    - 删除拷贝构造函数
 *    - 删除拷贝赋值运算符
 *    - 日志管理器通常是单例
 * 
 * 3. 生命周期管理
 *    - initialize(): 初始化日志系统
 *    - shutdown(): 关闭日志系统
 *    - isInitialized(): 查询初始化状态
 * 
 * 4. 日志记录方法
 *    - log(): 通用日志记录方法
 *    - debug/info/warning/error(): 特定级别的便捷方法
 * 
 * 5. 静态工具方法
 *    - formatMessage(): 格式化日志消息
 *    - timestamp(): 生成时间戳
 *    - levelToString(): 级别转字符串
 * 
 * 实现要求：
 * 
 * - 必须保证线程安全
 * - 必须支持日志文件输出
 * - 应该支持回调通知机制
 * 
 * @note 具体实现类应该采用单例模式
 */
class ILogManager {
public:
    virtual ~ILogManager() = default;
    
    // 禁止拷贝
    ILogManager(const ILogManager&) = delete;
    ILogManager& operator=(const ILogManager&) = delete;
    
    // ========== 初始化方法 ==========
    
    /**
     * @brief 初始化日志系统（单日志文件）
     * 
     * 使用指定的日志文件路径初始化日志系统。
     * 
     * @param logFilePath 主日志文件的完整路径
     * 
     * @return true  初始化成功
     * @return false 初始化失败（如无法创建文件）
     * 
     * @note 如果目录不存在，应自动创建
     * @note 如果文件已存在，应追加写入
     */
    virtual bool initialize(const std::string& logFilePath) = 0;
    
    /**
     * @brief 初始化日志系统（双日志文件）
     * 
     * 使用主日志文件和错误日志文件路径初始化日志系统。
     * 错误日志文件仅记录 Error 级别的日志。
     * 
     * @param logFilePath      主日志文件路径，记录所有级别
     * @param errorLogFilePath 错误日志文件路径，仅记录 Error 级别
     * 
     * @return true  初始化成功
     * @return false 初始化失败
     */
    virtual bool initialize(const std::string& logFilePath, 
                           const std::string& errorLogFilePath) = 0;
    
    /**
     * @brief 关闭日志系统
     * 
     * 刷新所有缓冲区，关闭日志文件，释放资源。
     * 
     * @note 关闭后可以重新调用 initialize() 初始化
     */
    virtual void shutdown() = 0;
    
    // ========== 日志记录方法 ==========
    
    /**
     * @brief 记录日志（通用方法）
     * 
     * 记录指定级别的日志消息。
     * 
     * @param message 日志消息内容
     * @param level   日志级别，默认为 Info
     */
    virtual void log(const std::string& message, LogLevel level = LogLevel::Info) = 0;
    
    /**
     * @brief 记录调试日志
     * 
     * 记录 Debug 级别的日志消息。
     * 
     * @param message 日志消息内容
     */
    virtual void debug(const std::string& message) = 0;
    
    /**
     * @brief 记录信息日志
     * 
     * 记录 Info 级别的日志消息。
     * 
     * @param message 日志消息内容
     */
    virtual void info(const std::string& message) = 0;
    
    /**
     * @brief 记录警告日志
     * 
     * 记录 Warning 级别的日志消息。
     * 
     * @param message 日志消息内容
     */
    virtual void warning(const std::string& message) = 0;
    
    /**
     * @brief 记录错误日志
     * 
     * 记录 Error 级别的日志消息。
     * 如果配置了错误日志文件，同时写入错误日志文件。
     * 
     * @param message 日志消息内容
     */
    virtual void error(const std::string& message) = 0;
    
    // ========== 状态查询方法 ==========
    
    /**
     * @brief 检查日志系统是否已初始化
     * 
     * @return true  已初始化，可以记录日志
     * @return false 未初始化，需要先调用 initialize()
     */
    virtual bool isInitialized() const = 0;
    
    /**
     * @brief 检查是否配置了错误日志文件
     * 
     * @return true  已配置错误日志文件
     * @return false 未配置错误日志文件
     */
    virtual bool hasErrorLogFile() const = 0;
    
    // ========== 回调设置方法 ==========
    
    /**
     * @brief 设置日志回调函数
     * 
     * 设置日志消息的回调函数，每条日志都会触发回调。
     * 用于通知 UI 层显示日志或进行其他自定义处理。
     * 
     * @param callback 回调函数，传入 nullptr 可清除回调
     * 
     * @note 回调函数应该快速返回，避免阻塞日志系统
     */
    virtual void setLogCallback(LogCallback callback) = 0;
    
    // ========== 静态工具方法 ==========
    
    /**
     * @brief 格式化日志消息
     * 
     * 将原始消息格式化为标准日志格式：[时间戳] [级别] 消息
     * 
     * @param message 原始日志消息
     * @param level   日志级别
     * 
     * @return std::string 格式化后的完整日志消息
     * 
     * @note 这是静态方法，可以在不创建实例的情况下调用
     */
    static std::string formatMessage(const std::string& message, LogLevel level);
    
    /**
     * @brief 获取当前时间戳字符串
     * 
     * 生成格式为 HH:MM:SS.mmm 的时间戳字符串。
     * 
     * @return std::string 时间戳字符串，如 "14:30:25.123"
     */
    static std::string timestamp();
    
    /**
     * @brief 日志级别转字符串
     * 
     * 将日志级别枚举值转换为对应的字符串表示。
     * 
     * @param level 日志级别枚举值
     * 
     * @return std::string 级别字符串（"DEBUG"、"INFO"、"WARNING"、"ERROR"）
     */
    static std::string levelToString(LogLevel level);

protected:
    ILogManager() = default;
};

// 日志管理器智能指针类型
using ILogManagerPtr = std::unique_ptr<ILogManager>;

#endif // I_LOG_MANAGER_H
