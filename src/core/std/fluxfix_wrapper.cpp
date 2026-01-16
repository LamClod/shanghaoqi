/**
 * @file fluxfix_wrapper.cpp
 * @brief FluxFix 整流中间件 - 标准 C++ 实现
 * 
 * @details
 * 本文件实现 FluxFix Rust 库的 C++ 封装层，通过 FFI 调用底层 Rust 实现。
 * 
 * 实现要点：
 * - RAII 模式管理 FFI 资源生命周期
 * - 移动语义支持，避免资源泄漏
 * - 空指针安全检查
 * - FFI 返回值到 C++ 类型的转换
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "fluxfix_wrapper.h"

namespace fluxfix {

// ============================================================================
// Aggregator 实现
// ============================================================================

/**
 * @brief 构造函数 - 创建 FFI 聚合器实例
 * 
 * 调用 Rust FFI 函数 fluxfix_aggregator_new() 分配底层资源。
 * 初始化列表确保 m_ptr 在构造时立即获得有效指针。
 */
Aggregator::Aggregator()
    : m_ptr(fluxfix_aggregator_new())  // FFI: 分配 Rust StreamAggregator
{
}

/**
 * @brief 析构函数 - 释放 FFI 资源
 * 
 * 检查指针有效性后调用 FFI 释放函数。
 * 如果 finalize() 已被调用，m_ptr 为 nullptr，跳过释放。
 */
Aggregator::~Aggregator()
{
    if (m_ptr) {
        fluxfix_aggregator_free(m_ptr);  // FFI: 释放 Rust 内存
    }
}

/**
 * @brief 移动构造函数 - 转移 FFI 资源所有权
 * 
 * 窃取源对象的指针，并将源对象置空。
 * noexcept 保证移动操作不会抛出异常。
 */
Aggregator::Aggregator(Aggregator&& other) noexcept
    : m_ptr(other.m_ptr)  // 窃取指针
{
    other.m_ptr = nullptr;  // 源对象置空，防止双重释放
}

/**
 * @brief 移动赋值运算符 - 安全转移资源
 * 
 * 实现步骤：
 * 1. 自赋值检查（防止 a = std::move(a) 导致资源丢失）
 * 2. 释放当前持有的资源
 * 3. 窃取源对象的资源
 * 4. 源对象置空
 */
Aggregator& Aggregator::operator=(Aggregator&& other) noexcept
{
    if (this != &other) {                      // 自赋值检查
        if (m_ptr) fluxfix_aggregator_free(m_ptr);  // 释放旧资源
        m_ptr = other.m_ptr;                   // 窃取新资源
        other.m_ptr = nullptr;                 // 源对象置空
    }
    return *this;
}

/**
 * @brief 添加原始字节数据到聚合器
 * 
 * @param data 数据指针（SSE 格式的字节流）
 * @param len 数据长度
 * @return true 成功添加
 * @return false 失败（无效参数或内部错误）
 * 
 * FFI 返回值约定：0 = 成功，非 0 = 错误码
 */
bool Aggregator::addBytes(const uint8_t* data, size_t len)
{
    // 前置条件检查：指针有效、数据非空、长度非零
    if (!m_ptr || !data || len == 0) return false;
    
    // FFI 调用：将字节数据传递给 Rust 聚合器
    // 返回 0 表示成功
    return fluxfix_aggregator_add_bytes(m_ptr, data, len) == 0;
}

/**
 * @brief 添加字符串数据到聚合器（便捷重载）
 * 
 * 将 string_view 转换为原始字节指针，委托给原始指针版本。
 * reinterpret_cast 安全：char 和 uint8_t 在内存中等价。
 */
bool Aggregator::addBytes(std::string_view data)
{
    return addBytes(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

/**
 * @brief 检查流是否已完成
 * 
 * FFI 返回值约定：0 = 未完成，非 0 = 已完成
 * 完成条件由 Rust 端判断（[DONE] 标记或 finish_reason）
 */
bool Aggregator::isComplete() const
{
    // 短路求值：m_ptr 为空时直接返回 false
    return m_ptr && fluxfix_aggregator_is_complete(m_ptr) != 0;
}

/**
 * @brief 完成聚合并获取结果
 * 
 * 此方法会消耗聚合器，转移 FFI 资源所有权给 Rust 端处理。
 * 
 * 实现步骤：
 * 1. 空指针检查
 * 2. 调用 FFI finalize，获取 FFIResponse 指针
 * 3. 立即将 m_ptr 置空（所有权已转移）
 * 4. 将 FFI 结构转换为 C++ Response
 * 5. 释放 FFI 响应内存
 * 
 * @return 聚合后的响应，失败返回 nullptr
 */
std::unique_ptr<Response> Aggregator::finalize()
{
    if (!m_ptr) return nullptr;  // 已被 finalize 或移动

    // FFI 调用：完成聚合，获取结果
    // 此调用会消耗 Rust 端的 StreamAggregator
    FFIResponse* ffi = fluxfix_aggregator_finalize(m_ptr);
    m_ptr = nullptr;  // 所有权已转移，置空防止重复释放

    if (!ffi) return nullptr;  // Rust 端返回空（聚合失败）

    // 将 FFI 结构转换为 C++ 类型
    // 使用条件检查避免空指针解引用
    auto resp = std::make_unique<Response>();
    if (ffi->id) resp->id = ffi->id;                    // 响应 ID
    if (ffi->model) resp->model = ffi->model;           // 模型名称
    if (ffi->content) resp->content = ffi->content;     // 聚合后的内容
    if (ffi->finish_reason) resp->finishReason = ffi->finish_reason;  // 结束原因

    // 释放 FFI 响应内存（Rust 分配的字符串）
    fluxfix_response_free(ffi);
    return resp;
}

// ============================================================================
// Splitter 实现
// ============================================================================

/**
 * @brief 构造函数 - 创建 FFI 拆分器实例
 * 
 * 调用 Rust FFI 函数分配底层 StreamSplitter。
 * 默认 chunk_size 由 Rust 端设置（通常为 20 字符）。
 */
Splitter::Splitter()
    : m_ptr(fluxfix_splitter_new())  // FFI: 分配 Rust StreamSplitter
{
}

/**
 * @brief 析构函数 - 释放 FFI 资源
 */
Splitter::~Splitter()
{
    if (m_ptr) {
        fluxfix_splitter_free(m_ptr);  // FFI: 释放 Rust 内存
    }
}

/**
 * @brief 移动构造函数
 * 
 * 与 Aggregator 相同的移动语义实现。
 */
Splitter::Splitter(Splitter&& other) noexcept
    : m_ptr(other.m_ptr)
{
    other.m_ptr = nullptr;
}

/**
 * @brief 移动赋值运算符
 */
Splitter& Splitter::operator=(Splitter&& other) noexcept
{
    if (this != &other) {
        if (m_ptr) fluxfix_splitter_free(m_ptr);
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }
    return *this;
}

/**
 * @brief 设置分块大小
 * 
 * @param size 每个内容块的最大字符数
 * 
 * 参数验证：size 必须大于 0，否则忽略调用。
 * Rust 端会确保 UTF-8 字符边界安全。
 */
void Splitter::setChunkSize(size_t size)
{
    if (m_ptr && size > 0) {
        fluxfix_splitter_set_chunk_size(m_ptr, size);  // FFI: 设置块大小
    }
}

/**
 * @brief 将完整响应拆分为 SSE 块序列
 * 
 * @param id 响应 ID（如 "chatcmpl-123"）
 * @param model 模型名称（如 "gpt-4"）
 * @param content 完整响应内容
 * @return SSE 格式的字节块列表
 * 
 * 实现步骤：
 * 1. 将 string_view 转换为 null-terminated 字符串（FFI 要求）
 * 2. 调用 FFI split 函数获取块数组
 * 3. 遍历块数组，复制数据到 C++ vector
 * 4. 释放 FFI 块数组内存
 */
std::vector<std::vector<uint8_t>> Splitter::split(
    std::string_view id,
    std::string_view model,
    std::string_view content)
{
    std::vector<std::vector<uint8_t>> result;
    if (!m_ptr) return result;

    // FFI 要求 null-terminated 字符串
    // string_view 不保证 null 结尾，需要转换
    std::string idStr(id);
    std::string modelStr(model);
    std::string contentStr(content);

    // FFI 调用：执行拆分
    // 返回 Rust 分配的块数组
    FFIChunkArray* chunks = fluxfix_splitter_split(
        m_ptr,
        idStr.c_str(),      // null-terminated
        modelStr.c_str(),
        contentStr.c_str()
    );

    if (!chunks) return result;  // 拆分失败

    // 获取块数量并预分配空间
    size_t len = fluxfix_chunk_array_len(chunks);
    result.reserve(len);  // 避免多次重新分配

    // 遍历块数组，复制数据到 C++ vector
    for (size_t i = 0; i < len; ++i) {
        const uint8_t* data = nullptr;
        size_t dataLen = 0;
        
        // FFI: 获取第 i 个块的数据指针和长度
        // 返回 0 表示成功
        if (fluxfix_chunk_array_get(chunks, i, &data, &dataLen) == 0 && data) {
            // 使用迭代器范围构造，复制数据
            result.emplace_back(data, data + dataLen);
        }
    }

    // 释放 FFI 块数组（包括所有块的内存）
    fluxfix_chunk_array_free(chunks);
    return result;
}

// ============================================================================
// SseHandler 实现
// ============================================================================

/**
 * @brief 构造函数 - 创建内部聚合器
 * 
 * 使用 unique_ptr 管理 Aggregator 生命周期。
 * m_done 初始化为 false（类内初始化）。
 */
SseHandler::SseHandler()
    : m_aggregator(std::make_unique<Aggregator>())
{
}

/**
 * @brief 添加 SSE 事件
 * 
 * @param eventData SSE 事件数据（如 "data: {...}\n\n"）
 * 
 * 处理逻辑：
 * 1. 检测 [DONE] 标记 - 如果存在，设置完成标志并返回
 * 2. 否则将数据传递给内部聚合器
 * 
 * [DONE] 是 OpenAI SSE 流的终止标记，表示流结束。
 */
void SseHandler::addEvent(std::string_view eventData)
{
    // 检测 [DONE] 终止标记
    // npos 表示未找到，!= npos 表示找到
    if (eventData.find("[DONE]") != std::string_view::npos) {
        m_done = true;  // 设置完成标志
        return;         // 不将 [DONE] 传递给聚合器
    }
    
    // 将事件数据传递给聚合器处理
    if (m_aggregator) {
        m_aggregator->addBytes(eventData);
    }
}

/**
 * @brief 检查是否完成
 * 
 * 完成条件（任一满足）：
 * - 收到 [DONE] 标记（m_done == true）
 * - 聚合器检测到 finish_reason
 */
bool SseHandler::isComplete() const
{
    return m_done || (m_aggregator && m_aggregator->isComplete());
}

/**
 * @brief 完成处理并获取结果
 * 
 * 委托给内部聚合器的 finalize() 方法。
 * 调用后聚合器被消耗，需要 reset() 才能重新使用。
 */
std::unique_ptr<Response> SseHandler::finalize()
{
    if (!m_aggregator) return nullptr;
    return m_aggregator->finalize();
}

/**
 * @brief 重置处理器状态
 * 
 * 创建新的聚合器实例，重置完成标志。
 * 允许同一个 SseHandler 实例处理多个独立的 SSE 流。
 */
void SseHandler::reset()
{
    m_aggregator = std::make_unique<Aggregator>();  // 创建新聚合器
    m_done = false;  // 重置完成标志
}

// ============================================================================
// Rectifier 实现
// ============================================================================

std::optional<RectifierTrigger> Rectifier::detect(std::string_view errorMsg)
{
    std::string msg(errorMsg);
    int result = fluxfix_rectifier_detect(msg.c_str());
    switch (result) {
        case 0: return RectifierTrigger::InvalidSignature;
        case 1: return RectifierTrigger::MissingThinkingPrefix;
        case 2: return RectifierTrigger::InvalidRequest;
        default: return std::nullopt;
    }
}

std::string Rectifier::rectify(std::string_view jsonStr, RectifyResult* result)
{
    std::string input(jsonStr);
    FFIRectifyResult ffiResult{};
    char* output = fluxfix_rectifier_rectify(input.c_str(), &ffiResult);
    if (!output) return {};

    if (result) {
        result->applied = ffiResult.applied != 0;
        result->removedThinkingBlocks = ffiResult.removed_thinking_blocks;
        result->removedRedactedThinkingBlocks = ffiResult.removed_redacted_thinking_blocks;
        result->removedSignatureFields = ffiResult.removed_signature_fields;
        result->removedThinkingField = ffiResult.removed_thinking_field != 0;
    }

    std::string ret(output);
    fluxfix_string_free(output);
    return ret;
}

// ============================================================================
// 全局函数实现
// ============================================================================

/**
 * @brief 获取 FluxFix 库版本
 * 
 * 直接调用 FFI 函数获取 Rust 库版本字符串。
 * 返回的字符串是静态的，无需释放。
 * 
 * @return 版本字符串（如 "0.1.0"）
 */
const char* version()
{
    return fluxfix_version();  // FFI: 获取版本
}

} // namespace fluxfix
