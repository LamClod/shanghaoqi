/**
 * @file test_result_dialog.cpp
 * @brief 测活结果对话框实现
 * 
 * 实现测活结果对话框的所有功能。
 * 
 * @details
 * 实现细节：
 * 
 * 单个结果 UI：
 * - 固定大小 260x120
 * - 左侧大图标（24pt 字体）
 * - 右侧配置名称和状态
 * - 底部确定按钮
 * 
 * 批量结果 UI：
 * - 最小大小 320x200，默认 340x260
 * - 顶部统计标签
 * - 中间可滚动列表
 * - 每项：图标 + 名称 + HTTP 状态
 * - 底部确定按钮
 * 
 * 图标说明：
 * - ✓：测活成功
 * - ✗：测活失败
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "test_result_dialog.h"

// ============================================================================
// 构造函数
// ============================================================================

/**
 * @brief 构造函数（单个结果）
 * 
 * 创建显示单个测活结果的对话框。
 * 
 * @param result 测活结果项
 * @param parent 父窗口
 */
TestResultDialog::TestResultDialog(const TestResultItem& result, QWidget* parent)
    : QDialog(parent)
{
    setupSingleResultUi(result);
}

/**
 * @brief 构造函数（批量结果）
 * 
 * 创建显示批量测活结果的对话框。
 * 
 * @param results 测活结果列表
 * @param parent 父窗口
 */
TestResultDialog::TestResultDialog(const QList<TestResultItem>& results, QWidget* parent)
    : QDialog(parent)
{
    setupBatchResultUi(results);
}

// ============================================================================
// UI 初始化
// ============================================================================

/**
 * @brief 设置单个结果 UI
 * 
 * 创建显示单个测活结果的界面。
 * 
 * @param result 测活结果项
 * 
 * @details
 * 布局结构：
 * - 固定大小 260x120
 * - 左侧：大图标（✓ 或 ✗）
 * - 右侧：配置名称和状态文本
 * - 底部：确定按钮
 */
void TestResultDialog::setupSingleResultUi(const TestResultItem& result)
{
    // 设置窗口标题
    setWindowTitle(tr("测活结果"));
    
    // 设置固定大小
    setFixedSize(260, 120);
    
    // 移除帮助按钮（Windows 标题栏上的问号）
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // 创建主布局
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);  // 设置组件间距
    
    // ========== 结果图标和文字 ==========
    auto* resultLayout = new QHBoxLayout();
    
    // 创建图标标签
    auto* iconLabel = new QLabel(this);
    // 根据成功/失败显示不同图标
    iconLabel->setText(result.success ? "✓" : "✗");
    // 设置大字体（24pt）
    iconLabel->setFont(QFont(iconLabel->font().family(), 24));
    resultLayout->addWidget(iconLabel);
    
    // 创建文字区域
    auto* textLayout = new QVBoxLayout();
    
    // 配置名称标签（粗体）
    auto* nameLabel = new QLabel(result.name, this);
    nameLabel->setFont(QFont(nameLabel->font().family(), 11, QFont::Bold));
    textLayout->addWidget(nameLabel);
    
    // 状态文本
    QString statusText = result.success 
        ? tr("测活成功 (HTTP %1)").arg(result.httpStatus)
        : tr("测活失败 (HTTP %1)").arg(result.httpStatus);
    auto* statusLabel = new QLabel(statusText, this);
    textLayout->addWidget(statusLabel);
    
    resultLayout->addLayout(textLayout);
    resultLayout->addStretch();  // 添加弹性空间
    layout->addLayout(resultLayout);
    
    layout->addStretch();  // 添加弹性空间，将按钮推到底部
    
    // ========== 确定按钮 ==========
    auto* okButton = new QPushButton(tr("确定"), this);
    okButton->setDefault(true);  // 设置为默认按钮
    // 连接点击信号到 accept 槽，关闭对话框
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okButton, 0, Qt::AlignRight);  // 右对齐
}

/**
 * @brief 设置批量结果 UI
 * 
 * 创建显示批量测活结果的界面。
 * 
 * @param results 测活结果列表
 * 
 * @details
 * 布局结构：
 * - 最小大小 320x200，默认 340x260
 * - 顶部：统计标签（成功/失败/总计）
 * - 中间：可滚动的结果列表
 * - 底部：确定按钮
 */
void TestResultDialog::setupBatchResultUi(const QList<TestResultItem>& results)
{
    // 设置窗口标题
    setWindowTitle(tr("一键测活结果"));
    
    // 设置大小
    setMinimumSize(320, 200);
    resize(340, 260);
    
    // 移除帮助按钮
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // 创建主布局
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    
    // ========== 统计信息 ==========
    // 计算成功数量
    int successCount = 0;
    for (const auto& r : results) {
        if (r.success) successCount++;
    }
    int total = results.size();
    int failedCount = total - successCount;
    
    // 创建统计标签
    auto* statsLabel = new QLabel(
        tr("成功: %1  |  失败: %2  |  总计: %3")
            .arg(successCount).arg(failedCount).arg(total), 
        this);
    // 设置粗体字体
    statsLabel->setFont(QFont(statsLabel->font().family(), 10, QFont::Bold));
    layout->addWidget(statsLabel);
    
    // ========== 结果列表（可滚动）==========
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);  // 允许内容调整大小
    
    // 创建列表容器
    auto* listWidget = new QWidget(scrollArea);
    auto* listLayout = new QVBoxLayout(listWidget);
    listLayout->setSpacing(4);  // 紧凑的间距
    listLayout->setContentsMargins(4, 4, 4, 4);
    
    // 遍历所有结果，创建列表项
    for (const auto& result : results) {
        auto* itemLayout = new QHBoxLayout();
        
        // 图标
        auto* icon = new QLabel(result.success ? "✓" : "✗", listWidget);
        itemLayout->addWidget(icon);
        
        // 配置名称（占据剩余空间）
        auto* name = new QLabel(result.name, listWidget);
        itemLayout->addWidget(name, 1);  // stretch factor = 1
        
        // HTTP 状态码
        auto* status = new QLabel(tr("HTTP %1").arg(result.httpStatus), listWidget);
        itemLayout->addWidget(status);
        
        listLayout->addLayout(itemLayout);
    }
    
    listLayout->addStretch();  // 添加弹性空间
    scrollArea->setWidget(listWidget);
    layout->addWidget(scrollArea, 1);  // stretch factor = 1，占据剩余空间
    
    // ========== 确定按钮 ==========
    auto* okButton = new QPushButton(tr("确定"), this);
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(okButton, 0, Qt::AlignRight);
}
