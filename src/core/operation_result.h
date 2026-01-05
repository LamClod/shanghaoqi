/**
 * @file operation_result.h
 * @brief 操作结果类型定义
 * 
 * 本文件定义了应用程序中所有操作的统一结果类型，提供标准化的错误处理机制。
 * 
 * @details
 * 设计理念：
 * 
 * 本模块采用纯 C++ 实现，不依赖任何外部框架，确保跨平台兼容性和代码可移植性。
 * 通过统一的 OperationResult 结构体，所有操作都能返回一致的结果格式，
 * 便于调用方进行错误处理和状态判断。
 * 
 * 错误码分组设计：
 * 
 * 错误码按功能模块分组，每个模块占用 100 个错误码空间：
 * - 0: 成功
 * - 100-199: 端口相关错误
 * - 200-299: 证书相关错误
 * - 300-399: Hosts 文件相关错误
 * - 400-499: 配置相关错误
 * - 500-599: 网络相关错误
 * - 600-699: 认证相关错误
 * - 700-799: API 相关错误
 * - 800-899: 加密相关错误
 * - 900-999: 通用错误
 * 
 * 使用示例：
 * @code
 * // 返回成功结果
 * return OperationResult::success("操作完成");
 * 
 * // 返回失败结果
 * return OperationResult::failure("端口被占用", ErrorCode::PortInUse);
 * 
 * // 检查结果
 * auto result = someOperation();
 * if (result.isSuccess()) {
 *     // 处理成功情况
 * } else {
 *     // 处理失败情况，可以访问 result.message 和 result.code
 * }
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef OPERATION_RESULT_H
#define OPERATION_RESULT_H

#include <string>
#include <map>

/**
 * @brief 错误码枚举
 * 
 * 定义应用程序中所有可能的错误类型，按功能模块分组便于管理和扩展。
 * 
 * @details
 * 错误码分组规则：
 * 
 * | 范围      | 模块         | 说明                           |
 * |-----------|--------------|--------------------------------|
 * | 0         | 成功         | 操作成功完成                   |
 * | 100-199   | 端口         | 端口占用、端口无效等           |
 * | 200-299   | 证书         | 证书生成、安装、验证失败等     |
 * | 300-399   | Hosts        | Hosts 文件读写、权限等         |
 * | 400-499   | 配置         | 配置加载、保存、验证等         |
 * | 500-599   | 网络         | 网络连接、超时等               |
 * | 600-699   | 认证         | API 密钥、认证失败等           |
 * | 700-799   | API          | 目标 API 返回错误等            |
 * | 800-899   | 加密         | 加密解密操作失败等             |
 * | 900-999   | 通用         | 未分类的其他错误               |
 * 
 * @note 新增错误码时请遵循分组规则，在对应范围内选择未使用的值
 */
enum class ErrorCode {
    Success = 0,
    
    // 端口错误 (100-199)
    PortInUse = 100,            ///< 端口已被占用
    PortInvalid = 101,          ///< 端口号无效
    
    // 证书错误 (200-299)
    CertGenerationFailed = 200, ///< 证书生成失败
    CertInstallFailed = 201,    ///< 证书安装失败
    CertNotFound = 202,         ///< 证书未找到
    CertInvalid = 203,          ///< 证书无效
    
    // Hosts文件错误 (300-399)
    HostsPermissionDenied = 300, ///< Hosts 文件权限不足
    HostsWriteFailed = 301,      ///< Hosts 文件写入失败
    HostsReadFailed = 302,       ///< Hosts 文件读取失败
    HostsBackupFailed = 303,     ///< Hosts 文件备份失败
    
    // 配置错误 (400-499)
    ConfigInvalid = 400,        ///< 配置无效
    ConfigSaveFailed = 401,     ///< 配置保存失败
    ConfigLoadFailed = 402,     ///< 配置加载失败
    ConfigNotFound = 403,       ///< 配置未找到
    
    // 网络错误 (500-599)
    NetworkError = 500,         ///< 网络错误
    NetworkTimeout = 501,       ///< 网络超时
    ConnectionFailed = 502,     ///< 连接失败
    
    // 认证错误 (600-699)
    AuthenticationFailed = 600, ///< 认证失败
    AuthKeyInvalid = 601,       ///< 认证密钥无效
    ApiKeyInvalid = 602,        ///< API 密钥无效
    
    // API错误 (700-799)
    TargetApiError = 700,       ///< 目标 API 错误
    ApiResponseInvalid = 701,   ///< API 响应无效
    
    // 加密错误 (800-899)
    EncryptionFailed = 800,     ///< 加密失败
    DecryptionFailed = 801,     ///< 解密失败
    
    // 通用错误 (900-999)
    Unknown = 999               ///< 未知错误
};

/**
 * @brief 操作结果结构体
 * 
 * 统一的操作结果类型，封装操作的成功/失败状态、消息、错误码和详细信息。
 * 
 * @details
 * 结构体成员说明：
 * 
 * - ok: 布尔值，表示操作是否成功
 * - message: 结果消息，成功时为描述信息，失败时为错误描述
 * - code: 错误码枚举值，用于程序化错误处理
 * - details: 键值对映射，存储额外的详细信息
 * 
 * 工厂方法：
 * 
 * 推荐使用静态工厂方法创建实例，而不是直接构造：
 * - success(): 创建成功结果
 * - failure(): 创建失败结果
 * 
 * 状态检查：
 * 
 * 提供多种方式检查操作状态：
 * - isSuccess() / isFailure(): 显式方法调用
 * - operator bool(): 支持在条件语句中直接使用
 * 
 * 使用示例：
 * @code
 * // 创建成功结果
 * auto result = OperationResult::success("文件保存成功");
 * 
 * // 创建带详细信息的成功结果
 * auto result = OperationResult::success("证书生成完成", {
 *     {"certPath", "/path/to/cert.pem"},
 *     {"keyPath", "/path/to/key.pem"}
 * });
 * 
 * // 创建失败结果
 * auto result = OperationResult::failure(
 *     "端口 443 已被占用",
 *     ErrorCode::PortInUse,
 *     {{"port", "443"}, {"process", "nginx"}}
 * );
 * 
 * // 检查结果
 * if (result) {  // 使用 operator bool()
 *     std::cout << "成功: " << result.message << std::endl;
 * } else {
 *     std::cerr << "失败: " << result.message << std::endl;
 *     std::cerr << "错误码: " << OperationResult::errorCodeToString(result.code) << std::endl;
 * }
 * @endcode
 */
struct OperationResult {
    bool ok;                                    ///< 操作是否成功
    std::string message;                        ///< 结果消息
    ErrorCode code;                             ///< 错误码
    std::map<std::string, std::string> details; ///< 详细信息
    
    /**
     * @brief 创建成功结果
     * 
     * @param msg 成功消息（可选）
     * @return 成功的操作结果
     */
    static OperationResult success(const std::string& msg = "") {
        return {true, msg, ErrorCode::Success, {}};
    }
    
    /**
     * @brief 创建带详细信息的成功结果
     * 
     * @param msg 成功消息
     * @param details 详细信息
     * @return 成功的操作结果
     */
    static OperationResult success(const std::string& msg, 
                                   const std::map<std::string, std::string>& details) {
        return {true, msg, ErrorCode::Success, details};
    }
    
    /**
     * @brief 创建失败结果
     * 
     * @param msg 错误消息
     * @param code 错误码
     * @param details 详细信息（可选）
     * @return 失败的操作结果
     */
    static OperationResult failure(const std::string& msg, ErrorCode code,
                                   const std::map<std::string, std::string>& details = {}) {
        return {false, msg, code, details};
    }
    
    /**
     * @brief 检查是否成功
     */
    [[nodiscard]] bool isSuccess() const { return ok; }
    
    /**
     * @brief 检查是否失败
     */
    [[nodiscard]] bool isFailure() const { return !ok; }
    
    /**
     * @brief 布尔转换运算符
     */
    explicit operator bool() const { return ok; }
    
    /**
     * @brief 错误码转字符串
     * 
     * @param code 错误码
     * @return 错误码的字符串表示
     */
    static std::string errorCodeToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::Success:              return "Success";
            case ErrorCode::PortInUse:            return "PortInUse";
            case ErrorCode::PortInvalid:          return "PortInvalid";
            case ErrorCode::CertGenerationFailed: return "CertGenerationFailed";
            case ErrorCode::CertInstallFailed:    return "CertInstallFailed";
            case ErrorCode::CertNotFound:         return "CertNotFound";
            case ErrorCode::CertInvalid:          return "CertInvalid";
            case ErrorCode::HostsPermissionDenied: return "HostsPermissionDenied";
            case ErrorCode::HostsWriteFailed:     return "HostsWriteFailed";
            case ErrorCode::HostsReadFailed:      return "HostsReadFailed";
            case ErrorCode::HostsBackupFailed:    return "HostsBackupFailed";
            case ErrorCode::ConfigInvalid:        return "ConfigInvalid";
            case ErrorCode::ConfigSaveFailed:     return "ConfigSaveFailed";
            case ErrorCode::ConfigLoadFailed:     return "ConfigLoadFailed";
            case ErrorCode::ConfigNotFound:       return "ConfigNotFound";
            case ErrorCode::NetworkError:         return "NetworkError";
            case ErrorCode::NetworkTimeout:       return "NetworkTimeout";
            case ErrorCode::ConnectionFailed:     return "ConnectionFailed";
            case ErrorCode::AuthenticationFailed: return "AuthenticationFailed";
            case ErrorCode::AuthKeyInvalid:       return "AuthKeyInvalid";
            case ErrorCode::ApiKeyInvalid:        return "ApiKeyInvalid";
            case ErrorCode::TargetApiError:       return "TargetApiError";
            case ErrorCode::ApiResponseInvalid:   return "ApiResponseInvalid";
            case ErrorCode::EncryptionFailed:     return "EncryptionFailed";
            case ErrorCode::DecryptionFailed:     return "DecryptionFailed";
            case ErrorCode::Unknown:
            default:                              return "Unknown";
        }
    }
};

#endif // OPERATION_RESULT_H
