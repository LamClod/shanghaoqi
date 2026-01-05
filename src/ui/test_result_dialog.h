/**
 * @file test_result_dialog.h
 * @brief 测活结果对话框
 * 
 * 显示 API 测活结果的模态对话框。
 * 
 * @details
 * 功能概述：
 * - 显示单个配置的测活结果
 * - 显示批量测活的汇总结果
 * - 使用图标和颜色区分成功/失败
 * 
 * 单个结果显示：
 * - 成功/失败图标（✓/✗）
 * - 配置名称
 * - HTTP 状态码
 * - 错误信息（如有）
 * 
 * 批量结果显示：
 * - 统计信息（成功/失败/总计）
 * - 可滚动的结果列表
 * - 每项显示图标、名称、状态码
 * 
 * 使用示例：
 * @code
 * // 单个结果
 * TestResultItem result;
 * result.name = "GPT-4";
 * result.success = true;
 * result.httpStatus = 200;
 * TestResultDialog dialog(result, this);
 * dialog.exec();
 * 
 * // 批量结果
 * QList<TestResultItem> results;
 * // ... 填充结果 ...
 * TestResultDialog batchDialog(results, this);
 * batchDialog.exec();
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef TEST_RESULT_DIALOG_H
#define TEST_RESULT_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QFont>

/**
 * @brief 单个测活结果项
 * 
 * 存储单个 API 配置的测活结果。
 * 
 * @details
 * 字段说明：
 * - name：配置名称
 * - success：测活是否成功
 * - httpStatus：HTTP 响应状态码
 * - errorMessage：错误信息（失败时）
 */
struct TestResultItem {
    QString name;
    bool success;
    int httpStatus;
    QString errorMessage;
};

/**
 * @brief 测活结果对话框
 * 
 * 显示 API 测活结果的模态对话框。
 * 
 * @details
 * 支持两种模式：
 * - 单个结果：显示单个配置的详细测活结果
 * - 批量结果：显示多个配置的测活结果列表和统计
 */
class TestResultDialog : public QDialog {
    Q_OBJECT

public:
    explicit TestResultDialog(const TestResultItem& result, QWidget* parent = nullptr);
    explicit TestResultDialog(const QList<TestResultItem>& results, QWidget* parent = nullptr);

private:
    void setupSingleResultUi(const TestResultItem& result);
    void setupBatchResultUi(const QList<TestResultItem>& results);
};

#endif // TEST_RESULT_DIALOG_H
