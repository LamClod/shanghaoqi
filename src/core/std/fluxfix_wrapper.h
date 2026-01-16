/**
 * @file fluxfix_wrapper.h
 * @brief FluxFix 整流中间件 - 标准 C++ 封装
 * 
 * 本文件提供 FluxFix Rust 库的纯标准 C++ 封装，无外部框架依赖。
 * 
 * @details
 * 功能概述：
 * 
 * FluxFix 是一个高性能的 AI API 流式响应整流中间件，用于：
 * - 修复畸形的 SSE (Server-Sent Events) 数据
 * - 修复截断或不完整的 JSON 响应
 * - 流式响应聚合（多个 SSE 事件 → 单个完整响应）
 * - 非流式响应拆分（单个响应 → 多个 SSE 事件）
 * 
 * 架构设计：
 * 
 * 本封装采用 RAII 模式管理 FFI 资源：
 * - Aggregator: 封装 FFIStreamAggregator，负责流式聚合
 * - Splitter: 封装 FFIStreamSplitter，负责响应拆分
 * - SseHandler: 高级 SSE 处理器，组合 Aggregator 使用
 * 
 * 内存管理：
 * 
 * - 所有 FFI 指针在析构时自动释放
 * - 支持移动语义，禁止拷贝
 * - finalize() 会转移所有权，调用后对象不可再用
 * 
 * 使用示例：
 * 
 * @code
 * // 流式聚合
 * fluxfix::Aggregator agg;
 * agg.addBytes("data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n");
 * agg.addBytes("data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n");
 * agg.addBytes("data: [DONE]\n\n");
 * auto response = agg.finalize();
 * // response->content == "Hello World"
 * 
 * // 响应拆分
 * fluxfix::Splitter splitter;
 * splitter.setChunkSize(10);
 * auto chunks = splitter.split("id-123", "gpt-4", "Hello World");
 * // chunks 包含多个 SSE 格式的数据块
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see fluxfix.h FluxFix C FFI 头文件
 */

#ifndef FLUXFIX_STD_WRAPPER_H
#define FLUXFIX_STD_WRAPPER_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <string_view>
#include <optional>

// FluxFix C FFI 头文件
extern "C" {
#include "fluxfix.h"
}

namespace fluxfix {

// ============================================================================
// 数据结构
// ============================================================================

/**
 * @brief 统一响应结构
 * 
 * 表示聚合后的完整 AI 响应，与 API 格式无关。
 * 
 * @details
 * 字段说明：
 * - id: 响应唯一标识符（如 "chatcmpl-123"）
 * - model: 使用的模型名称（如 "gpt-4"）
 * - content: 完整的响应内容文本
 * - finishReason: 结束原因（如 "stop", "length", "tool_calls"）
 */
struct Response {
    std::string id;           ///< 响应 ID
    std::string model;        ///< 模型名称
    std::string content;      ///< 响应内容
    std::string finishReason; ///< 结束原因
};

// ============================================================================
// 流式聚合器
// ============================================================================

/**
 * @brief 流式数据聚合器
 * 
 * 将多个流式 SSE 事件聚合为单个完整响应。
 * 
 * @details
 * 工作原理：
 * 
 * 1. 接收 SSE 格式的数据块（data: {...}\n\n）
 * 2. 解析每个块中的 JSON，提取增量内容
 * 3. 累积所有增量内容到内部缓冲区
 * 4. 检测 [DONE] 标记或 finish_reason 判断完成
 * 5. finalize() 返回聚合后的完整响应
 * 
 * 支持的格式：
 * 
 * - OpenAI 流式格式（choices[].delta.content）
 * - 自动处理 [DONE] 终止标记
 * - 自动提取 usage 统计信息
 * 
 * 生命周期：
 * 
 * - 构造后可多次调用 addBytes()
 * - finalize() 会消耗对象，之后不可再用
 * - 支持移动语义，不支持拷贝
 * 
 * @note 线程安全：单个实例不是线程安全的
 */
class Aggregator {
public:
    /**
     * @brief 构造函数
     * 
     * 创建新的聚合器实例，分配 FFI 资源。
     */
    Aggregator();
    
    /**
     * @brief 析构函数
     * 
     * 释放 FFI 资源。如果未调用 finalize()，资源会被正确清理。
     */
    ~Aggregator();

    // 禁止拷贝
    Aggregator(const Aggregator&) = delete;
    Aggregator& operator=(const Aggregator&) = delete;
    
    // 支持移动
    Aggregator(Aggregator&& other) noexcept;
    Aggregator& operator=(Aggregator&& other) noexcept;

    /**
     * @brief 添加数据块（原始指针版本）
     * 
     * @param data 数据指针
     * @param len 数据长度
     * @return true 添加成功
     * @return false 添加失败（无效指针或内部错误）
     */
    bool addBytes(const uint8_t* data, size_t len);
    
    /**
     * @brief 添加数据块（string_view 版本）
     * 
     * @param data SSE 格式的数据块
     * @return true 添加成功
     * @return false 添加失败
     */
    bool addBytes(std::string_view data);
    
    /**
     * @brief 检查流是否完成
     * 
     * 完成条件：
     * - 收到 [DONE] 标记
     * - 收到包含 finish_reason 的块
     * 
     * @return true 流已完成
     * @return false 流未完成，可继续添加数据
     */
    [[nodiscard]] bool isComplete() const;
    
    /**
     * @brief 完成聚合并获取结果
     * 
     * 消耗聚合器，返回聚合后的完整响应。
     * 调用后聚合器不可再用。
     * 
     * @return 聚合后的响应，失败返回 nullptr
     * 
     * @warning 此方法会转移 FFI 资源所有权，调用后对象处于无效状态
     */
    [[nodiscard]] std::unique_ptr<Response> finalize();

private:
    FFIStreamAggregator* m_ptr;  ///< FFI 聚合器指针
};

// ============================================================================
// 响应拆分器
// ============================================================================

/**
 * @brief 响应拆分器
 * 
 * 将单个完整响应拆分为多个 SSE 事件流。
 * 
 * @details
 * 工作原理：
 * 
 * 1. 接收完整的响应内容（id, model, content）
 * 2. 按指定的 chunk_size 拆分内容
 * 3. 生成 SSE 格式的数据块序列
 * 4. 自动添加角色块、内容块、结束块和 [DONE] 标记
 * 
 * 生成的块序列：
 * 
 * 1. 角色块：data: {"choices":[{"delta":{"role":"assistant"}}]}\n\n
 * 2. 内容块（多个）：data: {"choices":[{"delta":{"content":"..."}}]}\n\n
 * 3. 结束块：data: {"choices":[{"delta":{},"finish_reason":"stop"}]}\n\n
 * 4. 终止标记：data: [DONE]\n\n
 * 
 * @note 拆分器可重复使用，每次 split() 调用独立
 */
class Splitter {
public:
    /**
     * @brief 构造函数
     * 
     * 创建新的拆分器实例，默认 chunk_size = 20 字符。
     */
    Splitter();
    
    /**
     * @brief 析构函数
     */
    ~Splitter();

    // 禁止拷贝
    Splitter(const Splitter&) = delete;
    Splitter& operator=(const Splitter&) = delete;
    
    // 支持移动
    Splitter(Splitter&& other) noexcept;
    Splitter& operator=(Splitter&& other) noexcept;

    /**
     * @brief 设置分块大小
     * 
     * @param size 每个内容块的最大字符数（UTF-8 字符边界安全）
     * 
     * @note 实际块大小可能略大于指定值，以保证 UTF-8 字符完整性
     */
    void setChunkSize(size_t size);
    
    /**
     * @brief 拆分响应为 SSE 块序列
     * 
     * @param id 响应 ID
     * @param model 模型名称
     * @param content 完整响应内容
     * @return SSE 格式的数据块列表
     */
    [[nodiscard]] std::vector<std::vector<uint8_t>> split(
        std::string_view id,
        std::string_view model,
        std::string_view content
    );

private:
    FFIStreamSplitter* m_ptr;  ///< FFI 拆分器指针
};

// ============================================================================
// SSE 流处理器
// ============================================================================

/**
 * @brief SSE 流处理器
 * 
 * 高级 SSE 事件处理器，封装 Aggregator 提供更友好的 API。
 * 
 * @details
 * 与 Aggregator 的区别：
 * 
 * - SseHandler 自动处理 [DONE] 标记
 * - SseHandler 支持 reset() 重置状态
 * - SseHandler 提供更高级的事件处理接口
 * 
 * 使用场景：
 * 
 * - 实时处理 SSE 事件流
 * - 需要在处理过程中检查完成状态
 * - 需要重复使用同一处理器实例
 */
class SseHandler {
public:
    /**
     * @brief 构造函数
     */
    SseHandler();
    
    /**
     * @brief 添加 SSE 事件
     * 
     * @param eventData SSE 事件数据（包含 "data: " 前缀）
     * 
     * @note 自动检测 [DONE] 标记并设置完成状态
     */
    void addEvent(std::string_view eventData);
    
    /**
     * @brief 检查是否完成
     * 
     * @return true 收到 [DONE] 或 finish_reason
     */
    [[nodiscard]] bool isComplete() const;
    
    /**
     * @brief 完成处理并获取结果
     * 
     * @return 聚合后的响应
     */
    [[nodiscard]] std::unique_ptr<Response> finalize();
    
    /**
     * @brief 重置处理器状态
     * 
     * 清空内部状态，可开始处理新的事件流。
     */
    void reset();

private:
    std::unique_ptr<Aggregator> m_aggregator;  ///< 内部聚合器
    bool m_done = false;                        ///< [DONE] 标记状态
};

// ============================================================================
// 请求整流器
// ============================================================================

/**
 * @brief 整流触发类型
 */
enum class RectifierTrigger {
    InvalidSignature = 0,
    MissingThinkingPrefix = 1,
    InvalidRequest = 2,
};

/**
 * @brief 整流结果
 */
struct RectifyResult {
    bool applied = false;
    uint32_t removedThinkingBlocks = 0;
    uint32_t removedRedactedThinkingBlocks = 0;
    uint32_t removedSignatureFields = 0;
    bool removedThinkingField = false;
};

/**
 * @brief 请求整流器
 *
 * 处理 Anthropic API thinking 块兼容性问题。
 */
class Rectifier {
public:
    /**
     * @brief 检测错误消息是否触发整流
     * @param errorMsg 错误消息
     * @return 触发类型，无触发返回 std::nullopt
     */
    [[nodiscard]] static std::optional<RectifierTrigger> detect(std::string_view errorMsg);

    /**
     * @brief 整流 JSON 请求
     * @param jsonStr JSON 请求字符串
     * @param result 整流结果 (可选)
     * @return 整流后的 JSON 字符串，失败返回空字符串
     */
    [[nodiscard]] static std::string rectify(std::string_view jsonStr, RectifyResult* result = nullptr);
};

// ============================================================================
// 全局函数
// ============================================================================

/**
 * @brief 获取 FluxFix 库版本
 * 
 * @return 版本字符串（如 "0.1.0"）
 */
[[nodiscard]] const char* version();

} // namespace fluxfix

#endif // FLUXFIX_STD_WRAPPER_H
