/**
 * @file fluxfix_wrapper.cpp
 * @brief FluxFix 整流中间件 - Qt 扩展实现
 * 
 * @details
 * 本文件实现 Qt 类型到标准类型的适配层。
 * 
 * 类型转换策略：
 * - QByteArray → std::string_view（零拷贝，使用 constData()）
 * - QString → std::string（需要拷贝，UTF-16 → UTF-8）
 * - std::vector<uint8_t> → QByteArray（需要拷贝）
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "fluxfix_wrapper.h"
#include <QByteArray>
#include <QString>

// ============================================================================
// FluxFixAggregator 实现
// ============================================================================

/**
 * @brief 添加 QByteArray 数据
 * 
 * 将 QByteArray 转换为 string_view 后委托给标准实现。
 * 
 * 转换说明：
 * - constData() 返回 const char*，不会触发深拷贝
 * - size() 返回字节数（不含 null 终止符）
 * - string_view 不拥有数据，仅引用 QByteArray 内部缓冲区
 * 
 * @note QByteArray 必须在 addBytes 调用期间保持有效
 */
bool FluxFixAggregator::addBytes(const QByteArray& data)
{
    // 零拷贝转换：QByteArray → string_view
    // constData() 返回内部缓冲区指针
    return m_impl.addBytes(std::string_view(data.constData(), data.size()));
}

/**
 * @brief 检查流是否完成
 * 
 * 直接委托给标准实现。
 */
bool FluxFixAggregator::isComplete() const
{
    return m_impl.isComplete();
}

/**
 * @brief 完成聚合并获取结果
 * 
 * 直接委托给标准实现。
 * 返回的 Response 使用 std::string，调用者需要转换为 QString。
 */
std::unique_ptr<FluxFixAggregator::Response> FluxFixAggregator::finalize()
{
    return m_impl.finalize();
}

// ============================================================================
// FluxFixSplitter 实现
// ============================================================================

/**
 * @brief 设置分块大小
 * 
 * 直接委托给标准实现。
 */
void FluxFixSplitter::setChunkSize(size_t size)
{
    m_impl.setChunkSize(size);
}

/**
 * @brief 拆分响应为 SSE 块序列
 * 
 * 类型转换流程：
 * 1. QString → std::string（toStdString() 执行 UTF-16 → UTF-8 转换）
 * 2. 调用标准实现的 split()
 * 3. std::vector<uint8_t> → QByteArray（逐块转换）
 * 
 * @param id 响应 ID（QString）
 * @param model 模型名称（QString）
 * @param content 完整响应内容（QString）
 * @return QByteArray 列表
 */
std::vector<QByteArray> FluxFixSplitter::split(
    const QString& id,
    const QString& model,
    const QString& content)
{
    // 调用标准实现
    // toStdString() 将 QString (UTF-16) 转换为 std::string (UTF-8)
    auto chunks = m_impl.split(
        id.toStdString(),
        model.toStdString(),
        content.toStdString()
    );
    
    // 转换结果：std::vector<std::vector<uint8_t>> → std::vector<QByteArray>
    std::vector<QByteArray> result;
    result.reserve(chunks.size());  // 预分配空间
    
    for (const auto& chunk : chunks) {
        // QByteArray 构造函数：(const char* data, int size)
        // reinterpret_cast: uint8_t* → char*（内存布局相同）
        // static_cast: size_t → int（QByteArray 使用 int 作为大小类型）
        result.emplace_back(
            reinterpret_cast<const char*>(chunk.data()),
            static_cast<int>(chunk.size())
        );
    }
    
    return result;
}

// ============================================================================
// FluxFixSseHandler 实现
// ============================================================================

/**
 * @brief 添加 SSE 事件
 * 
 * 与 FluxFixAggregator::addBytes 相同的转换策略。
 */
void FluxFixSseHandler::addEvent(const QByteArray& eventData)
{
    // 零拷贝转换：QByteArray → string_view
    m_impl.addEvent(std::string_view(eventData.constData(), eventData.size()));
}

/**
 * @brief 检查是否完成
 */
bool FluxFixSseHandler::isComplete() const
{
    return m_impl.isComplete();
}

/**
 * @brief 完成处理并获取结果
 */
std::unique_ptr<FluxFixSseHandler::Response> FluxFixSseHandler::finalize()
{
    return m_impl.finalize();
}

/**
 * @brief 重置处理器状态
 */
void FluxFixSseHandler::reset()
{
    m_impl.reset();
}
