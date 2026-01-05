/**
 * @file fluxfix_wrapper.h
 * @brief FluxFix 整流中间件 - Qt 扩展
 * 
 * @details
 * 本文件提供 FluxFix 的 Qt 类型封装，基于标准 C++ 实现。
 * 
 * 设计理念：
 * - 组合模式：Qt 类内部持有标准实现实例
 * - 类型适配：Qt 类型（QString, QByteArray）↔ 标准类型
 * - 零开销抽象：仅做类型转换，无额外逻辑
 * 
 * 类对应关系：
 * - FluxFixAggregator → fluxfix::Aggregator
 * - FluxFixSplitter → fluxfix::Splitter
 * - FluxFixSseHandler → fluxfix::SseHandler
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 * 
 * @see fluxfix_wrapper.h (std) 标准 C++ 实现
 */

#ifndef FLUXFIX_QT_WRAPPER_H
#define FLUXFIX_QT_WRAPPER_H

#include "../std/fluxfix_wrapper.h"

// 前向声明 Qt 类型，避免包含完整头文件
// 减少编译依赖，加快编译速度
class QByteArray;
class QString;

/**
 * @brief Qt 扩展 - 流式数据聚合器
 * 
 * 封装 fluxfix::Aggregator，提供 Qt 类型接口。
 * 
 * @details
 * 使用示例：
 * @code
 * FluxFixAggregator agg;
 * agg.addBytes(reply->readAll());  // QByteArray
 * if (agg.isComplete()) {
 *     auto resp = agg.finalize();
 *     qDebug() << QString::fromStdString(resp->content);
 * }
 * @endcode
 * 
 * @note 生命周期与 fluxfix::Aggregator 相同
 */
class FluxFixAggregator {
public:
    /// 响应类型别名，复用标准实现的 Response 结构
    using Response = fluxfix::Response;
    
    /**
     * @brief 添加 QByteArray 数据
     * 
     * @param data Qt 字节数组（通常来自 QNetworkReply::readAll()）
     * @return true 成功
     * @return false 失败
     */
    bool addBytes(const QByteArray& data);
    
    /**
     * @brief 检查流是否完成
     */
    [[nodiscard]] bool isComplete() const;
    
    /**
     * @brief 完成聚合并获取结果
     * 
     * @return 聚合后的响应（Response 使用 std::string，需要转换为 QString）
     */
    [[nodiscard]] std::unique_ptr<Response> finalize();

private:
    fluxfix::Aggregator m_impl;  ///< 标准实现实例（组合模式）
};

/**
 * @brief Qt 扩展 - 响应拆分器
 * 
 * 封装 fluxfix::Splitter，提供 Qt 类型接口。
 * 
 * @details
 * 使用示例：
 * @code
 * FluxFixSplitter splitter;
 * splitter.setChunkSize(50);
 * auto chunks = splitter.split("id-123", "gpt-4", "Hello World");
 * for (const QByteArray& chunk : chunks) {
 *     socket->write(chunk);  // 发送 SSE 块
 * }
 * @endcode
 */
class FluxFixSplitter {
public:
    /**
     * @brief 设置分块大小
     * 
     * @param size 每个内容块的最大字符数
     */
    void setChunkSize(size_t size);
    
    /**
     * @brief 拆分响应为 SSE 块序列
     * 
     * @param id 响应 ID
     * @param model 模型名称
     * @param content 完整响应内容
     * @return QByteArray 列表，每个元素是一个 SSE 块
     */
    [[nodiscard]] std::vector<QByteArray> split(
        const QString& id,
        const QString& model,
        const QString& content
    );

private:
    fluxfix::Splitter m_impl;  ///< 标准实现实例
};

/**
 * @brief Qt 扩展 - SSE 流处理器
 * 
 * 封装 fluxfix::SseHandler，提供 Qt 类型接口。
 * 
 * @details
 * 使用示例：
 * @code
 * FluxFixSseHandler handler;
 * 
 * // 在 QNetworkReply::readyRead 槽中
 * handler.addEvent(reply->readAll());
 * 
 * if (handler.isComplete()) {
 *     auto resp = handler.finalize();
 *     // 处理完整响应
 *     handler.reset();  // 准备处理下一个流
 * }
 * @endcode
 */
class FluxFixSseHandler {
public:
    /// 响应类型别名
    using Response = fluxfix::Response;
    
    /**
     * @brief 添加 SSE 事件
     * 
     * @param eventData SSE 事件数据（QByteArray）
     */
    void addEvent(const QByteArray& eventData);
    
    /**
     * @brief 检查是否完成
     */
    [[nodiscard]] bool isComplete() const;
    
    /**
     * @brief 完成处理并获取结果
     */
    [[nodiscard]] std::unique_ptr<Response> finalize();
    
    /**
     * @brief 重置处理器状态
     */
    void reset();

private:
    fluxfix::SseHandler m_impl;  ///< 标准实现实例
};

/**
 * @brief 获取 FluxFix 库版本（全局函数）
 * 
 * @return 版本字符串
 */
inline const char* fluxfixVersion() { return fluxfix::version(); }

#endif // FLUXFIX_QT_WRAPPER_H
