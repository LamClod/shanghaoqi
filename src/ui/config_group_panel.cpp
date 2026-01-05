/**
 * @file config_group_panel.cpp
 * @brief 配置组面板实现
 * 
 * 实现配置组面板和配置编辑对话框的所有功能。
 * 
 * @details
 * 实现细节：
 * 
 * ProxyConfigDialog 实现：
 * - 使用 QFormLayout 组织表单字段
 * - 本地 URL 使用下拉框，预设常用域名
 * - 模型名称使用可编辑下拉框，支持自动获取
 * - 密钥字段使用密码模式显示
 * - 输入验证：必填字段检查
 * 
 * ConfigGroupPanel 实现：
 * - 使用 QTableWidget 显示配置列表
 * - 支持单选和双击编辑
 * - 按钮状态根据选择自动更新
 * - 测活使用 /v1/chat/completions 接口
 * 
 * 测活请求格式：
 * @code
 * POST /v1/chat/completions
 * {
 *   "model": "模型名称",
 *   "messages": [{"role": "user", "content": "hi"}],
 *   "max_tokens": 5
 * }
 * @endcode
 * 
 * 导入导出格式（JSON）：
 * @code
 * [
 *   {
 *     "name": "配置名称",
 *     "localUrl": "api.openai.com",
 *     "mappedUrl": "https://api.example.com",
 *     "localModelName": "gpt-4",
 *     "mappedModelName": "claude-3",
 *     "authKey": "可选",
 *     "apiKey": "必填"
 *   }
 * ]
 * @endcode
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

// ============================================================================
// 头文件包含
// ============================================================================

#include "config_group_panel.h"      // 配置组面板头文件
#include "test_result_dialog.h"      // 测试结果对话框

// Qt 布局相关
#include <QVBoxLayout>               // 垂直布局管理器
#include <QHBoxLayout>               // 水平布局管理器
#include <QHeaderView>               // 表格头部视图
#include <QMessageBox>               // 消息对话框
#include <QFormLayout>               // 表单布局管理器
#include <QDialogButtonBox>          // 对话框按钮盒

// Qt 网络相关
#include <QNetworkRequest>           // 网络请求
#include <QNetworkReply>             // 网络响应

// Qt JSON 相关
#include <QJsonDocument>             // JSON 文档
#include <QJsonObject>               // JSON 对象
#include <QJsonArray>                // JSON 数组

// Qt 文件相关
#include <QFileDialog>               // 文件选择对话框
#include <QStandardPaths>            // 标准路径
#include <QFile>                     // 文件操作

// ============================================================================
// 匿名命名空间 - 模块内部常量
// ============================================================================

namespace {

/**
 * @brief 预设的本地 URL 列表
 * 
 * 包含常用的 API 域名，用于本地 URL 下拉框的预设选项。
 * 用户可以从这些预设中选择，也可以手动输入自定义域名。
 */
const QStringList PRESET_LOCAL_URLS = {"api.openai.com", "api.anthropic.com"};

}  // namespace

// ============================================================================
// ProxyConfigDialog 类实现 - 代理配置编辑对话框
// ============================================================================

/**
 * @brief 构造函数 - 新增配置模式
 * 
 * 创建一个空白的配置编辑对话框，用于添加新的代理配置。
 * 
 * @param parent 父窗口指针，用于窗口层级管理
 * 
 * @details
 * 初始化流程：
 * 1. 调用基类构造函数，设置父窗口
 * 2. 初始化成员指针为 nullptr（防止野指针）
 * 3. 调用 setupUi() 创建界面元素
 * 4. 设置窗口标题为"新增代理配置"
 * 5. 启动模型获取定时器（用于自动刷新模型列表）
 */
ProxyConfigDialog::ProxyConfigDialog(QWidget* parent)
    : QDialog(parent)                    // 调用基类构造函数
    , m_modelFetchTimer(nullptr)         // 初始化定时器指针
    , m_networkManager(nullptr)          // 初始化网络管理器指针
{
    setupUi();                           // 创建用户界面
    setWindowTitle(tr("新增代理配置"));   // 设置窗口标题
    startModelFetchTimer();              // 启动模型自动获取定时器
}

/**
 * @brief 构造函数 - 编辑配置模式
 * 
 * 创建一个预填充的配置编辑对话框，用于修改现有的代理配置。
 * 
 * @param config 要编辑的配置项，包含所有现有配置数据
 * @param parent 父窗口指针，用于窗口层级管理
 * 
 * @details
 * 初始化流程：
 * 1. 调用基类构造函数，设置父窗口
 * 2. 初始化成员指针为 nullptr
 * 3. 调用 setupUi() 创建界面元素
 * 4. 设置窗口标题为"修改代理配置"
 * 5. 调用 populateFields() 填充现有配置数据
 * 6. 启动模型获取定时器
 */
ProxyConfigDialog::ProxyConfigDialog(const ProxyConfigItem& config, QWidget* parent)
    : QDialog(parent)                    // 调用基类构造函数
    , m_modelFetchTimer(nullptr)         // 初始化定时器指针
    , m_networkManager(nullptr)          // 初始化网络管理器指针
{
    setupUi();                           // 创建用户界面
    setWindowTitle(tr("修改代理配置"));   // 设置窗口标题
    populateFields(config);              // 填充现有配置数据到表单
    startModelFetchTimer();              // 启动模型自动获取定时器
}

/**
 * @brief 析构函数
 * 
 * 清理对话框资源，停止定时器以防止内存泄漏。
 * 
 * @note 网络管理器由 Qt 父子对象机制自动管理，无需手动删除
 */
ProxyConfigDialog::~ProxyConfigDialog()
{
    stopModelFetchTimer();               // 停止定时器，释放资源
}

/**
 * @brief 初始化用户界面
 * 
 * 创建并配置对话框的所有 UI 元素，包括表单字段、按钮和布局。
 * 
 * @details
 * 界面结构：
 * - 主布局：QVBoxLayout（垂直布局）
 *   - 表单布局：QFormLayout
 *     - 配置名称输入框（可选）
 *     - 本地 URL 下拉框（可编辑，预设常用域名）
 *     - 映射 URL 输入框（必填）
 *     - 本地模型名输入框（可选，默认使用映射模型名）
 *     - 映射模型名下拉框（可编辑，支持自动获取）
 *     - 鉴权 Key 输入框（可选，密码模式）
 *     - API Key 输入框（必填，密码模式）
 *   - 状态标签：显示模型获取状态
 *   - 按钮盒：确定/取消按钮
 * 
 * 信号连接：
 * - 映射 URL 变化时触发模型列表获取
 * - API Key 变化时触发模型列表获取
 * - 确定按钮触发验证逻辑
 * - 取消按钮关闭对话框
 */
void ProxyConfigDialog::setupUi()
{
    // ========== 创建主布局 ==========
    auto* layout = new QVBoxLayout(this);      // 创建垂直主布局
    auto* formLayout = new QFormLayout();      // 创建表单布局
    
    // ========== 配置名称输入框 ==========
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("可选，留空则使用模型名称"));  // 设置占位提示文本
    formLayout->addRow(tr("配置名称:"), m_nameEdit);                // 添加到表单
    
    // ========== 本地 URL 下拉框 ==========
    m_localUrlCombo = new QComboBox(this);
    m_localUrlCombo->setEditable(true);                    // 允许用户手动输入
    m_localUrlCombo->setMinimumWidth(350);                 // 设置最小宽度
    m_localUrlCombo->addItems(PRESET_LOCAL_URLS);          // 添加预设域名选项
    m_localUrlCombo->setCurrentText("api.openai.com");     // 设置默认值
    formLayout->addRow(tr("本地URL:"), m_localUrlCombo);
    
    // ========== 映射 URL 输入框 ==========
    m_mappedUrlEdit = new QLineEdit(this);
    m_mappedUrlEdit->setMinimumWidth(350);                              // 设置最小宽度
    m_mappedUrlEdit->setPlaceholderText(tr("如 https://api.openai.com")); // 设置占位提示
    m_mappedUrlEdit->setText("https://api.openai.com");                 // 设置默认值
    // 连接文本变化信号，用于触发模型列表获取
    connect(m_mappedUrlEdit, &QLineEdit::textChanged, 
            this, &ProxyConfigDialog::onMappedUrlChanged);
    formLayout->addRow(tr("映射URL:"), m_mappedUrlEdit);
    
    // ========== 本地模型名输入框 ==========
    m_localModelNameEdit = new QLineEdit(this);
    m_localModelNameEdit->setPlaceholderText(tr("客户端请求的模型名称"));  // 设置占位提示
    formLayout->addRow(tr("本地模型名:"), m_localModelNameEdit);
    
    // ========== 映射模型名下拉框 ==========
    m_mappedModelNameCombo = new QComboBox(this);
    m_mappedModelNameCombo->setEditable(true);             // 允许用户手动输入
    m_mappedModelNameCombo->setMinimumWidth(350);          // 设置最小宽度
    formLayout->addRow(tr("映射模型名:"), m_mappedModelNameCombo);
    
    // ========== 鉴权 Key 输入框 ==========
    m_authKeyEdit = new QLineEdit(this);
    m_authKeyEdit->setPlaceholderText(tr("可选"));         // 设置占位提示
    m_authKeyEdit->setEchoMode(QLineEdit::Password);       // 设置密码显示模式
    formLayout->addRow(tr("鉴权Key:"), m_authKeyEdit);
    
    // ========== API Key 输入框 ==========
    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setPlaceholderText(tr("必填"));          // 设置占位提示
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);        // 设置密码显示模式
    // 连接文本变化信号，用于触发模型列表获取
    connect(m_apiKeyEdit, &QLineEdit::textChanged, 
            this, &ProxyConfigDialog::onApiKeyChanged);
    formLayout->addRow(tr("API Key:"), m_apiKeyEdit);
    
    // 将表单布局添加到主布局
    layout->addLayout(formLayout);
    
    // ========== 状态标签 ==========
    m_statusLabel = new QLabel(tr("输入API Key后可自动获取模型列表"), this);
    m_statusLabel->setStyleSheet("color: #999; font-size: 12px;");  // 设置灰色小字体样式
    layout->addWidget(m_statusLabel);
    
    // ========== 按钮盒 ==========
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);  // 创建确定/取消按钮
    // 连接按钮信号
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ProxyConfigDialog::onAccept);  // 确定按钮
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);              // 取消按钮
    layout->addWidget(buttonBox);
    
    // ========== 对话框设置 ==========
    setMinimumWidth(500);                                  // 设置对话框最小宽度
    m_networkManager = new QNetworkAccessManager(this);    // 创建网络管理器（用于获取模型列表）
}

/**
 * @brief 填充表单字段
 * 
 * 将现有配置数据填充到对话框的各个输入控件中。
 * 用于编辑模式下显示当前配置的值。
 * 
 * @param config 要填充的配置项数据
 * 
 * @details
 * 填充顺序：
 * 1. 配置名称 -> 名称输入框
 * 2. 本地 URL -> 本地 URL 下拉框
 * 3. 映射 URL -> 映射 URL 输入框
 * 4. 本地模型名 -> 本地模型名输入框
 * 5. 映射模型名 -> 映射模型名下拉框（先添加选项再设置当前值）
 * 6. 鉴权 Key -> 鉴权 Key 输入框
 * 7. API Key -> API Key 输入框
 * 
 * @note 映射模型名需要先添加到下拉框选项中，否则无法正确显示
 */
void ProxyConfigDialog::populateFields(const ProxyConfigItem& config)
{
    // 填充基本文本字段
    m_nameEdit->setText(QString::fromStdString(config.name));
    m_localUrlCombo->setCurrentText(QString::fromStdString(config.localUrl));
    m_mappedUrlEdit->setText(QString::fromStdString(config.mappedUrl));
    m_localModelNameEdit->setText(QString::fromStdString(config.localModelName));
    
    // 填充映射模型名（需要先添加到下拉框选项）
    if (!config.mappedModelName.empty()) {
        m_mappedModelNameCombo->addItem(QString::fromStdString(config.mappedModelName));  // 添加选项
        m_mappedModelNameCombo->setCurrentText(QString::fromStdString(config.mappedModelName));  // 设置当前值
    }
    
    // 填充密钥字段
    m_authKeyEdit->setText(QString::fromStdString(config.authKey));
    m_apiKeyEdit->setText(QString::fromStdString(config.apiKey));
}

/**
 * @brief 获取配置数据
 * 
 * 从对话框的各个输入控件中收集数据，构造并返回配置项对象。
 * 
 * @return ProxyConfigItem 包含用户输入的配置数据
 * 
 * @details
 * 数据处理逻辑：
 * 1. 所有文本字段都进行 trimmed() 处理，去除首尾空白
 * 2. 如果配置名称为空，则使用映射模型名作为配置名称
 * 3. 如果本地模型名为空，则使用映射模型名作为本地模型名
 * 
 * @note 此方法不进行验证，验证逻辑在 onAccept() 中处理
 */
ProxyConfigItem ProxyConfigDialog::getProxyConfig() const
{
    ProxyConfigItem config;
    
    // 获取并处理文本值
    QString name = m_nameEdit->text().trimmed();
    QString mappedModel = m_mappedModelNameCombo->currentText().trimmed();
    
    // 配置名称：如果为空则使用映射模型名
    config.name = (name.isEmpty() ? mappedModel : name).toStdString();
    
    // 基本配置字段
    config.localUrl = m_localUrlCombo->currentText().trimmed().toStdString();
    config.mappedUrl = m_mappedUrlEdit->text().trimmed().toStdString();
    config.localModelName = m_localModelNameEdit->text().trimmed().toStdString();
    config.mappedModelName = mappedModel.toStdString();
    
    // 密钥字段
    config.authKey = m_authKeyEdit->text().trimmed().toStdString();
    config.apiKey = m_apiKeyEdit->text().trimmed().toStdString();
    
    // 本地模型名：如果为空则使用映射模型名
    if (config.localModelName.empty()) {
        config.localModelName = config.mappedModelName;
    }
    
    return config;
}

/**
 * @brief 确定按钮点击处理
 * 
 * 验证用户输入的配置数据，如果验证通过则接受对话框。
 * 
 * @details
 * 验证规则：
 * 1. 本地 URL 不能为空
 * 2. 映射 URL 不能为空
 * 3. 映射模型名不能为空
 * 4. API Key 不能为空
 * 
 * 如果任何验证失败，显示警告对话框并阻止关闭。
 * 所有验证通过后调用 accept() 关闭对话框并返回 Accepted 结果。
 */
void ProxyConfigDialog::onAccept()
{
    // 验证本地 URL
    if (m_localUrlCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证错误"), tr("本地URL不能为空"));
        return;  // 验证失败，不关闭对话框
    }
    
    // 验证映射 URL
    if (m_mappedUrlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证错误"), tr("映射URL不能为空"));
        return;
    }
    
    // 验证映射模型名
    if (m_mappedModelNameCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证错误"), tr("映射模型名不能为空"));
        return;
    }
    
    // 验证 API Key
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证错误"), tr("API Key不能为空"));
        return;
    }
    
    // 所有验证通过，接受对话框
    accept();
}

/**
 * @brief 启动模型获取定时器
 * 
 * 创建并启动定时器，用于定期检查是否需要获取模型列表。
 * 
 * @details
 * 定时器配置：
 * - 间隔：3000 毫秒（3 秒）
 * - 触发：调用 onFetchModels() 检查并获取模型
 * 
 * 工作原理：
 * 定时器每 3 秒触发一次，检查映射 URL 和 API Key 是否有变化。
 * 如果有变化且两者都不为空，则自动获取模型列表。
 * 这种机制可以在用户输入完成后自动刷新模型列表。
 */
void ProxyConfigDialog::startModelFetchTimer()
{
    // 如果定时器不存在，创建新的定时器
    if (!m_modelFetchTimer) {
        m_modelFetchTimer = new QTimer(this);        // 创建定时器，设置父对象
        m_modelFetchTimer->setInterval(3000);        // 设置间隔为 3 秒
        // 连接定时器超时信号到模型获取槽函数
        connect(m_modelFetchTimer, &QTimer::timeout, 
                this, &ProxyConfigDialog::onFetchModels);
    }
    m_modelFetchTimer->start();  // 启动定时器
}

/**
 * @brief 停止模型获取定时器
 * 
 * 停止定时器，防止在对话框关闭后继续触发。
 */
void ProxyConfigDialog::stopModelFetchTimer()
{
    if (m_modelFetchTimer) {
        m_modelFetchTimer->stop();  // 停止定时器
    }
}

/**
 * @brief 检查是否可以获取模型列表
 * 
 * 判断当前输入状态是否满足获取模型列表的条件。
 * 
 * @return true 如果映射 URL 和 API Key 都不为空
 * @return false 如果任一字段为空
 */
bool ProxyConfigDialog::canFetchModels() const
{
    // 两个必要字段都不为空时才能获取模型
    return !m_mappedUrlEdit->text().trimmed().isEmpty() &&
           !m_apiKeyEdit->text().trimmed().isEmpty();
}

/**
 * @brief 映射 URL 变化处理
 * 
 * 当映射 URL 输入框内容变化时触发，检查是否需要获取模型列表。
 * 
 * @details
 * 如果满足获取条件（URL 和 API Key 都不为空），
 * 立即更新状态标签并触发模型列表获取。
 */
void ProxyConfigDialog::onMappedUrlChanged()
{
    if (canFetchModels()) {
        m_statusLabel->setText(tr("正在获取模型列表..."));  // 更新状态提示
        fetchModelsFromApi();                              // 立即获取模型列表
    }
}

/**
 * @brief API Key 变化处理
 * 
 * 当 API Key 输入框内容变化时触发，检查是否需要获取模型列表。
 * 
 * @details
 * 如果满足获取条件，立即获取模型列表。
 * 如果不满足条件，显示提示信息引导用户输入。
 */
void ProxyConfigDialog::onApiKeyChanged()
{
    if (canFetchModels()) {
        m_statusLabel->setText(tr("正在获取模型列表..."));  // 更新状态提示
        fetchModelsFromApi();                              // 立即获取模型列表
    } else {
        // 显示引导提示
        m_statusLabel->setText(tr("输入API Key后可自动获取模型列表"));
    }
}

/**
 * @brief 定时器触发的模型获取处理
 * 
 * 由定时器定期触发，检查是否需要重新获取模型列表。
 * 
 * @details
 * 检查逻辑：
 * 1. 首先检查是否满足获取条件
 * 2. 比较当前 URL 和 API Key 与上次获取时的值
 * 3. 如果有变化，则重新获取模型列表
 * 
 * 这种机制可以避免重复获取相同的模型列表，减少网络请求。
 */
void ProxyConfigDialog::onFetchModels()
{
    // 检查是否满足获取条件
    if (!canFetchModels()) return;
    
    // 获取当前值
    QString url = m_mappedUrlEdit->text().trimmed();
    QString key = m_apiKeyEdit->text().trimmed();
    
    // 检查是否与上次获取时的值不同
    if (url != m_lastFetchedUrl || key != m_lastFetchedApiKey) {
        fetchModelsFromApi();  // 值有变化，重新获取
    }
}

/**
 * @brief 从 API 获取模型列表
 * 
 * 向映射 URL 的 /v1/models 端点发送请求，获取可用的模型列表。
 * 
 * @details
 * 请求流程：
 * 1. 验证映射 URL 和 API Key 不为空
 * 2. 记录当前 URL 和 API Key（用于去重检查）
 * 3. 构造 /v1/models 请求 URL
 * 4. 设置请求头（Content-Type 和 Authorization）
 * 5. 发送 GET 请求
 * 6. 异步处理响应
 * 
 * 响应处理：
 * - 成功：解析 JSON 响应，提取模型 ID 列表，更新下拉框
 * - 失败：显示错误信息
 * 
 * API 响应格式（OpenAI 兼容）：
 * @code
 * {
 *   "data": [
 *     {"id": "gpt-4", ...},
 *     {"id": "gpt-3.5-turbo", ...}
 *   ]
 * }
 * @endcode
 */
void ProxyConfigDialog::fetchModelsFromApi()
{
    // 获取并验证输入值
    QString mappedUrl = m_mappedUrlEdit->text().trimmed();
    QString apiKey = m_apiKeyEdit->text().trimmed();
    if (mappedUrl.isEmpty() || apiKey.isEmpty()) return;  // 必要字段为空，直接返回
    
    // 记录当前获取的 URL 和 API Key（用于去重）
    m_lastFetchedUrl = mappedUrl;
    m_lastFetchedApiKey = apiKey;
    
    // 构造模型列表 API URL
    QString modelsUrl = mappedUrl;
    if (!modelsUrl.endsWith('/')) modelsUrl += '/';  // 确保 URL 以斜杠结尾
    modelsUrl += "v1/models";                        // 添加模型列表端点
    
    // 创建网络请求
    QNetworkRequest request;
    request.setUrl(QUrl(modelsUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");  // 设置内容类型
    request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());       // 设置认证头
    
    // 发送 GET 请求
    QNetworkReply* reply = m_networkManager->get(request);
    
    // 连接响应完成信号，使用 Lambda 处理响应
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 检查请求是否成功
        if (reply->error() == QNetworkReply::NoError) {
            // 解析 JSON 响应
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                // 提取 data 数组
                QJsonArray dataArray = doc.object()["data"].toArray();
                
                // 保存当前选中的模型名（用于恢复选择）
                QString currentModel = m_mappedModelNameCombo->currentText();
                m_mappedModelNameCombo->clear();  // 清空现有选项
                
                // 提取所有模型 ID
                QStringList models;
                for (const auto& item : dataArray) {
                    QString id = item.toObject()["id"].toString();
                    if (!id.isEmpty()) models.append(id);  // 添加非空的模型 ID
                }
                
                // 排序并添加到下拉框
                models.sort();
                m_mappedModelNameCombo->addItems(models);
                
                // 恢复之前选中的模型（如果存在）
                if (!currentModel.isEmpty()) {
                    int idx = m_mappedModelNameCombo->findText(currentModel);
                    if (idx >= 0) {
                        m_mappedModelNameCombo->setCurrentIndex(idx);  // 找到则选中
                    } else {
                        m_mappedModelNameCombo->setCurrentText(currentModel);  // 未找到则设置文本
                    }
                }
                
                // 更新状态标签（成功，绿色）
                m_statusLabel->setText(tr("已获取 %1 个模型").arg(models.size()));
                m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 12px;");
            }
        } else {
            // 请求失败，显示错误信息（红色）
            m_statusLabel->setText(tr("获取失败: %1").arg(reply->errorString()));
            m_statusLabel->setStyleSheet("color: #f44336; font-size: 12px;");
        }
        
        // 清理响应对象
        reply->deleteLater();
    });
}

// ============================================================================
// ConfigGroupPanel 类实现 - 配置组面板
// ============================================================================

/**
 * @brief 构造函数
 * 
 * 创建配置组面板，初始化 UI 并加载配置数据。
 * 
 * @param configManager 配置管理器接口指针，用于读写配置数据
 * @param parent 父窗口指针，用于窗口层级管理
 * 
 * @details
 * 初始化流程：
 * 1. 保存配置管理器指针
 * 2. 调用 setupUi() 创建界面元素
 * 3. 调用 refreshTable() 加载并显示配置数据
 */
ConfigGroupPanel::ConfigGroupPanel(IConfigManager* configManager, QWidget* parent)
    : QWidget(parent)                    // 调用基类构造函数
    , m_configManager(configManager)     // 保存配置管理器指针
{
    setupUi();       // 创建用户界面
    refreshTable();  // 刷新配置表格
}

/**
 * @brief 初始化用户界面
 * 
 * 创建并配置面板的所有 UI 元素，包括配置表格和操作按钮。
 * 
 * @details
 * 界面结构：
 * - 主布局：QVBoxLayout（垂直布局）
 *   - 配置表格：QTableWidget
 *     - 列：名称、本地URL、映射URL、本地模型、映射模型、鉴权Key、状态
 *     - 单选模式，整行选择
 *     - 禁用编辑触发器（通过双击打开对话框编辑）
 *   - 按钮布局：QHBoxLayout
 *     - 左侧：新增、修改、删除
 *     - 右侧：导入、导出、刷新、测活、一键测活
 * 
 * 信号连接：
 * - 表格选择变化 -> 更新按钮状态
 * - 表格双击 -> 打开编辑对话框
 * - 各按钮点击 -> 对应操作槽函数
 */
void ConfigGroupPanel::setupUi()
{
    // ========== 创建主布局 ==========
    auto* layout = new QVBoxLayout(this);
    
    // ========== 创建配置表格 ==========
    m_configTable = new QTableWidget(this);
    m_configTable->setColumnCount(7);  // 设置 7 列
    
    // 设置表头标签
    m_configTable->setHorizontalHeaderLabels({
        tr("名称"), tr("本地URL"), tr("映射URL"), 
        tr("本地模型"), tr("映射模型"), tr("鉴权Key"), tr("状态")
    });
    
    // 配置表格外观和行为
    m_configTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);  // 列宽自动拉伸
    m_configTable->verticalHeader()->setVisible(false);                             // 隐藏行号
    m_configTable->setSelectionBehavior(QAbstractItemView::SelectRows);             // 整行选择
    m_configTable->setSelectionMode(QAbstractItemView::SingleSelection);            // 单选模式
    m_configTable->setEditTriggers(QAbstractItemView::NoEditTriggers);              // 禁用直接编辑
    
    // 连接表格信号
    connect(m_configTable, &QTableWidget::itemSelectionChanged, 
            this, &ConfigGroupPanel::onSelectionChanged);      // 选择变化
    connect(m_configTable, &QTableWidget::itemDoubleClicked, 
            this, &ConfigGroupPanel::onEditConfig);            // 双击编辑
    
    layout->addWidget(m_configTable);
    
    // ========== 创建按钮布局 ==========
    auto* buttonLayout = new QHBoxLayout();
    
    // 创建所有按钮
    m_addButton = new QPushButton(tr("新增"), this);        // 新增配置
    m_editButton = new QPushButton(tr("修改"), this);       // 修改配置
    m_deleteButton = new QPushButton(tr("删除"), this);     // 删除配置
    m_refreshButton = new QPushButton(tr("刷新"), this);    // 刷新列表
    m_testButton = new QPushButton(tr("测活"), this);       // 测试单个配置
    m_testAllButton = new QPushButton(tr("一键测活"), this); // 测试所有配置
    m_exportButton = new QPushButton(tr("导出"), this);     // 导出配置
    m_importButton = new QPushButton(tr("导入"), this);     // 导入配置
    
    // 连接按钮信号到槽函数
    connect(m_addButton, &QPushButton::clicked, this, &ConfigGroupPanel::onAddConfig);
    connect(m_editButton, &QPushButton::clicked, this, &ConfigGroupPanel::onEditConfig);
    connect(m_deleteButton, &QPushButton::clicked, this, &ConfigGroupPanel::onDeleteConfig);
    connect(m_refreshButton, &QPushButton::clicked, this, &ConfigGroupPanel::onRefresh);
    connect(m_testButton, &QPushButton::clicked, this, &ConfigGroupPanel::onTestConfig);
    connect(m_testAllButton, &QPushButton::clicked, this, &ConfigGroupPanel::onTestAllConfigs);
    connect(m_exportButton, &QPushButton::clicked, this, &ConfigGroupPanel::onExportConfig);
    connect(m_importButton, &QPushButton::clicked, this, &ConfigGroupPanel::onImportConfig);
    
    // 布局按钮（左侧：增删改，右侧：导入导出和测试）
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();              // 添加弹性空间
    buttonLayout->addWidget(m_importButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_testButton);
    buttonLayout->addWidget(m_testAllButton);
    
    layout->addLayout(buttonLayout);
    
    // 初始化按钮状态
    updateButtonStates();
}

/**
 * @brief 刷新配置表格
 * 
 * 从配置管理器重新加载数据并更新表格显示。
 * 
 * @details
 * 刷新流程：
 * 1. 清空表格所有行
 * 2. 从配置管理器获取配置列表
 * 3. 按配置名称排序
 * 4. 逐行添加配置数据到表格
 * 5. 更新按钮状态
 * 
 * 表格列内容：
 * - 列 0：配置名称
 * - 列 1：本地 URL
 * - 列 2：映射 URL
 * - 列 3：本地模型名
 * - 列 4：映射模型名
 * - 列 5：鉴权 Key（脱敏显示）
 * - 列 6：状态（默认"就绪"）
 */
void ConfigGroupPanel::refreshTable()
{
    // 清空表格
    m_configTable->setRowCount(0);
    
    // 获取配置数据
    const auto& config = m_configManager->getConfig();
    
    // 复制配置列表并按名称排序
    auto sortedConfigs = config.proxyConfigs;
    std::sort(sortedConfigs.begin(), sortedConfigs.end(),
              [](const ProxyConfigItem& a, const ProxyConfigItem& b) {
                  return a.name < b.name;  // 按名称升序排序
              });
    
    // 遍历配置列表，添加到表格
    for (size_t i = 0; i < sortedConfigs.size(); ++i) {
        const auto& cfg = sortedConfigs[i];
        m_configTable->insertRow(static_cast<int>(i));  // 插入新行
        
        // 创建居中对齐的表格项的辅助 Lambda
        auto createItem = [](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);  // 居中对齐
            return item;
        };
        
        // 设置各列数据
        m_configTable->setItem(static_cast<int>(i), 0, createItem(QString::fromStdString(cfg.name)));           // 名称
        m_configTable->setItem(static_cast<int>(i), 1, createItem(QString::fromStdString(cfg.localUrl)));       // 本地 URL
        m_configTable->setItem(static_cast<int>(i), 2, createItem(QString::fromStdString(cfg.mappedUrl)));      // 映射 URL
        m_configTable->setItem(static_cast<int>(i), 3, createItem(QString::fromStdString(cfg.localModelName))); // 本地模型
        m_configTable->setItem(static_cast<int>(i), 4, createItem(QString::fromStdString(cfg.mappedModelName)));// 映射模型
        m_configTable->setItem(static_cast<int>(i), 5, createItem(maskApiKey(QString::fromStdString(cfg.authKey))));  // 鉴权 Key（脱敏）
        m_configTable->setItem(static_cast<int>(i), 6, createItem(tr("就绪")));                                 // 状态
    }
    
    // 更新按钮状态
    updateButtonStates();
}

/**
 * @brief 新增配置处理
 * 
 * 打开配置编辑对话框，添加新的代理配置。
 * 
 * @details
 * 操作流程：
 * 1. 创建空白的配置编辑对话框
 * 2. 显示对话框并等待用户操作
 * 3. 如果用户点击确定：
 *    - 获取用户输入的配置数据
 *    - 调用配置管理器添加配置
 *    - 如果添加成功，保存配置并刷新表格
 *    - 发送配置变化信号和日志消息
 *    - 如果添加失败，显示错误对话框
 */
void ConfigGroupPanel::onAddConfig()
{
    // 创建新增配置对话框
    ProxyConfigDialog dialog(this);
    
    // 显示对话框并检查结果
    if (dialog.exec() == QDialog::Accepted) {
        // 尝试添加配置
        auto result = m_configManager->addProxyConfig(dialog.getProxyConfig());
        
        if (result.ok) {
            // 添加成功
            m_configManager->save();                              // 保存配置到文件
            refreshTable();                                       // 刷新表格显示
            emit configChanged();                                 // 发送配置变化信号
            emit logMessage(tr("配置添加成功"), LogLevel::Info);   // 发送日志消息
        } else {
            // 添加失败，显示错误信息
            QMessageBox::warning(this, tr("错误"), QString::fromStdString(result.message));
        }
    }
}

/**
 * @brief 编辑配置处理
 * 
 * 打开配置编辑对话框，修改选中的代理配置。
 * 
 * @details
 * 操作流程：
 * 1. 获取当前选中的行索引
 * 2. 如果没有选中行，直接返回
 * 3. 获取选中行对应的配置数据
 * 4. 创建预填充的配置编辑对话框
 * 5. 如果用户点击确定：
 *    - 获取修改后的配置数据
 *    - 调用配置管理器更新配置
 *    - 如果更新成功，保存配置并刷新表格
 *    - 发送配置变化信号和日志消息
 *    - 如果更新失败，显示错误对话框
 */
void ConfigGroupPanel::onEditConfig()
{
    // 获取当前选中行
    int row = currentRow();
    if (row < 0) return;  // 没有选中行，直接返回
    
    // 获取选中行的配置数据
    const auto* cfg = m_configManager->getProxyConfig(static_cast<size_t>(row));
    if (!cfg) return;  // 配置不存在，直接返回
    
    // 创建编辑对话框（预填充现有数据）
    ProxyConfigDialog dialog(*cfg, this);
    
    // 显示对话框并检查结果
    if (dialog.exec() == QDialog::Accepted) {
        // 尝试更新配置
        auto result = m_configManager->updateProxyConfig(static_cast<size_t>(row), dialog.getProxyConfig());
        
        if (result.ok) {
            // 更新成功
            m_configManager->save();                              // 保存配置到文件
            refreshTable();                                       // 刷新表格显示
            emit configChanged();                                 // 发送配置变化信号
            emit logMessage(tr("配置修改成功"), LogLevel::Info);   // 发送日志消息
        } else {
            // 更新失败，显示错误信息
            QMessageBox::warning(this, tr("错误"), QString::fromStdString(result.message));
        }
    }
}

/**
 * @brief 删除配置处理
 * 
 * 删除选中的代理配置，需要用户确认。
 * 
 * @details
 * 操作流程：
 * 1. 获取当前选中的行索引
 * 2. 如果没有选中行，直接返回
 * 3. 获取选中行对应的配置数据
 * 4. 显示确认对话框
 * 5. 如果用户确认删除：
 *    - 调用配置管理器删除配置
 *    - 如果删除成功，保存配置并刷新表格
 *    - 发送配置变化信号和日志消息
 */
void ConfigGroupPanel::onDeleteConfig()
{
    // 获取当前选中行
    int row = currentRow();
    if (row < 0) return;  // 没有选中行，直接返回
    
    // 获取选中行的配置数据
    const auto* cfg = m_configManager->getProxyConfig(static_cast<size_t>(row));
    if (!cfg) return;  // 配置不存在，直接返回
    
    // 显示确认对话框
    if (QMessageBox::question(this, tr("确认删除"),
            tr("确定要删除配置 \"%1\" 吗？").arg(QString::fromStdString(cfg->name))) == QMessageBox::Yes) {
        // 用户确认删除
        auto result = m_configManager->removeProxyConfig(static_cast<size_t>(row));
        
        if (result.ok) {
            // 删除成功
            m_configManager->save();                              // 保存配置到文件
            refreshTable();                                       // 刷新表格显示
            emit configChanged();                                 // 发送配置变化信号
            emit logMessage(tr("配置删除成功"), LogLevel::Info);   // 发送日志消息
        }
    }
}

/**
 * @brief 刷新配置处理
 * 
 * 从配置文件重新加载配置数据。
 * 
 * @details
 * 操作流程：
 * 1. 调用配置管理器的 load() 方法重新加载配置
 * 2. 如果加载成功，刷新表格显示
 * 3. 发送日志消息
 */
void ConfigGroupPanel::onRefresh()
{
    // 重新加载配置
    if (m_configManager->load().ok) {
        refreshTable();                                           // 刷新表格显示
        emit logMessage(tr("配置刷新成功"), LogLevel::Info);       // 发送日志消息
    }
}

/**
 * @brief 测试单个配置
 * 
 * 对选中的配置进行测活测试，验证 API 连接是否正常。
 * 
 * @details
 * 测试流程：
 * 1. 获取当前选中的配置
 * 2. 构造测试请求（POST /v1/chat/completions）
 * 3. 发送请求并等待响应
 * 4. 根据响应状态判断测试结果
 * 5. 显示测试结果对话框
 * 
 * 测试请求格式：
 * @code
 * POST /v1/chat/completions
 * {
 *   "model": "映射模型名",
 *   "messages": [{"role": "user", "content": "hi"}],
 *   "max_tokens": 5
 * }
 * @endcode
 * 
 * 成功条件：HTTP 状态码 200-299
 */
void ConfigGroupPanel::onTestConfig()
{
    // 获取当前选中行
    int row = currentRow();
    if (row < 0) return;  // 没有选中行，直接返回
    
    // 获取选中行的配置数据
    const auto* cfg = m_configManager->getProxyConfig(static_cast<size_t>(row));
    if (!cfg) return;  // 配置不存在，直接返回
    
    // 发送测试开始日志
    emit logMessage(tr("开始测活: %1").arg(QString::fromStdString(cfg->name)), LogLevel::Info);
    
    // 禁用测试按钮（防止重复点击）
    m_testButton->setEnabled(false);
    
    // 创建网络管理器
    auto* manager = new QNetworkAccessManager(this);
    
    // 构造测试 URL
    QString testUrl = QString::fromStdString(cfg->mappedUrl);
    if (!testUrl.endsWith('/')) testUrl += '/';
    testUrl += "v1/chat/completions";  // 使用 chat completions 端点测试
    
    // ========== 构造请求体 ==========
    // 创建消息对象
    QJsonObject message;
    message["role"] = "user";
    message["content"] = "hi";  // 简单的测试消息
    
    // 创建消息数组
    QJsonArray messages;
    messages.append(message);
    
    // 创建请求体
    QJsonObject body;
    body["model"] = QString::fromStdString(cfg->mappedModelName);  // 使用映射模型名
    body["messages"] = messages;
    body["max_tokens"] = 5;  // 限制响应长度，减少测试时间
    
    // ========== 创建网络请求 ==========
    QNetworkRequest request;
    request.setUrl(QUrl(testUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + QString::fromStdString(cfg->apiKey)).toUtf8());
    manager->setTransferTimeout(30000);  // 设置 30 秒超时
    
    // 保存配置名称（用于 Lambda 捕获）
    QString configName = QString::fromStdString(cfg->name);
    
    // 发送 POST 请求
    QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());
    
    // 连接响应完成信号
    connect(reply, &QNetworkReply::finished, this, 
            [this, reply, manager, configName]() {
        // 重新启用测试按钮
        m_testButton->setEnabled(true);
        
        // 获取响应状态
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        
        // 发送测试结果日志
        emit logMessage(tr("%1: %2 - HTTP %3")
                       .arg(ok ? "测活成功" : "测活失败", configName)
                       .arg(status),
                       ok ? LogLevel::Info : LogLevel::Error);
        
        // 构造测试结果对象
        TestResultItem result;
        result.name = configName;
        result.success = ok;
        result.httpStatus = status;
        result.errorMessage = ok ? QString() : reply->errorString();
        
        // 显示测试结果对话框
        TestResultDialog dialog(result, this);
        dialog.exec();
        
        // 清理资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

/**
 * @brief 测试所有配置
 * 
 * 对所有配置进行批量测活测试，并显示汇总结果。
 * 
 * @details
 * 测试流程：
 * 1. 检查是否有配置可测试
 * 2. 禁用测试按钮
 * 3. 并行发送所有配置的测试请求
 * 4. 收集所有响应结果
 * 5. 当所有测试完成后，显示汇总结果对话框
 * 
 * 并发控制：
 * - 使用 shared_ptr 共享计数器和结果列表
 * - 每个请求完成时递增计数器
 * - 当计数器等于总数时，显示结果对话框
 * 
 * @note 所有请求并行发送，不保证完成顺序
 */
void ConfigGroupPanel::onTestAllConfigs()
{
    // 获取配置数据
    const auto& config = m_configManager->getConfig();
    
    // 检查是否有配置可测试
    if (config.proxyConfigs.empty()) {
        QMessageBox::information(this, tr("提示"), tr("没有配置可测试"));
        return;
    }
    
    // 发送测试开始日志
    emit logMessage(tr("========== 开始一键测活 =========="), LogLevel::Info);
    
    // 禁用测试按钮（防止重复点击）
    m_testAllButton->setEnabled(false);
    m_testButton->setEnabled(false);
    
    // 获取配置总数
    int total = static_cast<int>(config.proxyConfigs.size());
    
    // 使用 shared_ptr 共享计数器和结果列表（用于 Lambda 捕获）
    auto completedCount = std::make_shared<int>(0);           // 已完成计数
    auto results = std::make_shared<QList<TestResultItem>>(); // 结果列表
    
    // 遍历所有配置，发送测试请求
    for (const auto& cfg : config.proxyConfigs) {
        // 创建网络管理器
        auto* manager = new QNetworkAccessManager(this);
        
        // 构造测试 URL
        QString testUrl = QString::fromStdString(cfg.mappedUrl);
        if (!testUrl.endsWith('/')) testUrl += '/';
        testUrl += "v1/chat/completions";
        
        // ========== 构造请求体 ==========
        QJsonObject message;
        message["role"] = "user";
        message["content"] = "hi";
        
        QJsonArray messages;
        messages.append(message);
        
        QJsonObject body;
        body["model"] = QString::fromStdString(cfg.mappedModelName);
        body["messages"] = messages;
        body["max_tokens"] = 5;
        
        // ========== 创建网络请求 ==========
        QNetworkRequest request;
        request.setUrl(QUrl(testUrl));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + QString::fromStdString(cfg.apiKey)).toUtf8());
        manager->setTransferTimeout(30000);  // 30 秒超时
        
        // 保存配置名称
        QString name = QString::fromStdString(cfg.name);
        
        // 发送 POST 请求
        QNetworkReply* reply = manager->post(request, QJsonDocument(body).toJson());
        
        // 连接响应完成信号
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, manager, name, total, completedCount, results]() {
            // 获取响应状态
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
            
            // 构造测试结果对象
            TestResultItem item;
            item.name = name;
            item.success = ok;
            item.httpStatus = status;
            item.errorMessage = ok ? QString() : reply->errorString();
            results->append(item);  // 添加到结果列表
            
            // 递增完成计数
            (*completedCount)++;
            
            // 发送单个测试结果日志
            emit logMessage(tr("%1 %2: HTTP %3")
                           .arg(ok ? "✓" : "✗", name).arg(status),
                           ok ? LogLevel::Info : LogLevel::Error);
            
            // 检查是否所有测试都已完成
            if (*completedCount >= total) {
                // 重新启用测试按钮
                m_testAllButton->setEnabled(true);
                m_testButton->setEnabled(currentRow() >= 0);
                
                // 统计成功数量
                int successCount = 0;
                for (const auto& r : *results) {
                    if (r.success) successCount++;
                }
                
                // 发送汇总日志
                emit logMessage(tr("========== 测活完成: %1/%2 成功 ==========")
                               .arg(successCount).arg(total), LogLevel::Info);
                
                // 显示汇总结果对话框
                TestResultDialog dialog(*results, this);
                dialog.exec();
            }
            
            // 清理资源
            reply->deleteLater();
            manager->deleteLater();
        });
    }
}

/**
 * @brief 导出配置
 * 
 * 将所有配置导出为 JSON 文件。
 * 
 * @details
 * 导出流程：
 * 1. 检查是否有配置可导出
 * 2. 显示文件保存对话框
 * 3. 将配置转换为 JSON 格式
 * 4. 写入文件
 * 
 * 导出格式：
 * @code
 * [
 *   {
 *     "name": "配置名称",
 *     "localUrl": "api.openai.com",
 *     "mappedUrl": "https://api.example.com",
 *     "localModelName": "gpt-4",
 *     "mappedModelName": "claude-3",
 *     "authKey": "鉴权密钥",
 *     "apiKey": "API密钥"
 *   }
 * ]
 * @endcode
 * 
 * @warning 导出文件包含敏感信息（API Key），请妥善保管
 */
void ConfigGroupPanel::onExportConfig()
{
    // 获取配置数据
    const auto& config = m_configManager->getConfig();
    
    // 检查是否有配置可导出
    if (config.proxyConfigs.empty()) {
        QMessageBox::information(this, tr("提示"), tr("没有配置可导出"));
        return;
    }
    
    // 显示文件保存对话框
    QString path = QFileDialog::getSaveFileName(this, tr("导出配置"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/proxy_config.json",
        tr("JSON文件 (*.json)"));
    
    // 用户取消选择
    if (path.isEmpty()) return;
    
    // 构造 JSON 数组
    QJsonArray arr;
    for (const auto& item : config.proxyConfigs) {
        QJsonObject obj;
        obj["name"] = QString::fromStdString(item.name);
        obj["localUrl"] = QString::fromStdString(item.localUrl);
        obj["mappedUrl"] = QString::fromStdString(item.mappedUrl);
        obj["localModelName"] = QString::fromStdString(item.localModelName);
        obj["mappedModelName"] = QString::fromStdString(item.mappedModelName);
        obj["authKey"] = QString::fromStdString(item.authKey);
        obj["apiKey"] = QString::fromStdString(item.apiKey);
        arr.append(obj);
    }
    
    // 写入文件
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));  // 格式化输出
        emit logMessage(tr("配置导出成功: %1").arg(path), LogLevel::Info);
    }
}

/**
 * @brief 导入配置
 * 
 * 从 JSON 文件导入配置。
 * 
 * @details
 * 导入流程：
 * 1. 显示文件选择对话框
 * 2. 读取并解析 JSON 文件
 * 3. 验证每个配置项的必填字段
 * 4. 添加有效的配置到配置管理器
 * 5. 保存配置并刷新表格
 * 
 * 字段处理：
 * - name：如果为空，使用 mappedModelName
 * - localUrl：如果为空，默认 "api.openai.com"
 * - mappedUrl：如果为空，默认 "https://api.openai.com"
 * - localModelName：如果为空，使用 mappedModelName
 * 
 * 有效性检查：
 * - mappedModelName 不能为空
 * - apiKey 不能为空
 */
void ConfigGroupPanel::onImportConfig()
{
    // 显示文件选择对话框
    QString path = QFileDialog::getOpenFileName(this, tr("导入配置"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("JSON文件 (*.json)"));
    
    // 用户取消选择
    if (path.isEmpty()) return;
    
    // 打开并读取文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    // 解析 JSON
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;  // 必须是数组格式
    
    // 导入计数
    int imported = 0;
    
    // 遍历 JSON 数组
    for (const auto& value : doc.array()) {
        if (!value.isObject()) continue;  // 跳过非对象元素
        
        QJsonObject obj = value.toObject();
        
        // 构造配置项
        ProxyConfigItem item;
        item.name = obj["name"].toString().toStdString();
        item.localUrl = obj["localUrl"].toString().toStdString();
        item.mappedUrl = obj["mappedUrl"].toString().toStdString();
        item.localModelName = obj["localModelName"].toString().toStdString();
        item.mappedModelName = obj["mappedModelName"].toString().toStdString();
        item.authKey = obj["authKey"].toString().toStdString();
        item.apiKey = obj["apiKey"].toString().toStdString();
        
        // 处理空字段，设置默认值
        if (item.name.empty()) item.name = item.mappedModelName;
        if (item.localUrl.empty()) item.localUrl = "api.openai.com";
        if (item.mappedUrl.empty()) item.mappedUrl = "https://api.openai.com";
        if (item.localModelName.empty()) item.localModelName = item.mappedModelName;
        
        // 验证必填字段并添加配置
        if (!item.mappedModelName.empty() && !item.apiKey.empty()) {
            if (m_configManager->addProxyConfig(item).ok) imported++;
        }
    }
    
    // 如果有成功导入的配置，保存并刷新
    if (imported > 0) {
        m_configManager->save();
        refreshTable();
        emit configChanged();
    }
    
    // 发送导入结果日志
    emit logMessage(tr("导入完成: %1 个配置").arg(imported), LogLevel::Info);
}

/**
 * @brief 选择变化处理
 * 
 * 当表格选择变化时更新按钮状态。
 */
void ConfigGroupPanel::onSelectionChanged()
{
    updateButtonStates();
}

/**
 * @brief 更新按钮状态
 * 
 * 根据当前选择状态启用或禁用相关按钮。
 * 
 * @details
 * 按钮状态规则：
 * - 修改按钮：有选中行时启用
 * - 删除按钮：有选中行时启用
 * - 测活按钮：有选中行时启用
 * - 其他按钮：始终启用
 */
void ConfigGroupPanel::updateButtonStates()
{
    bool hasSelection = currentRow() >= 0;  // 检查是否有选中行
    
    m_editButton->setEnabled(hasSelection);    // 修改按钮
    m_deleteButton->setEnabled(hasSelection);  // 删除按钮
    m_testButton->setEnabled(hasSelection);    // 测活按钮
}

/**
 * @brief 获取当前选中行索引
 * 
 * @return int 选中行的索引，如果没有选中则返回 -1
 */
int ConfigGroupPanel::currentRow() const
{
    auto items = m_configTable->selectedItems();
    return items.isEmpty() ? -1 : items.first()->row();
}

/**
 * @brief 脱敏显示 API Key
 * 
 * 将 API Key 转换为脱敏格式，只显示首尾各 4 个字符。
 * 
 * @param apiKey 原始 API Key
 * @return QString 脱敏后的字符串
 * 
 * @details
 * 脱敏规则：
 * - 空字符串：显示 "(无)"
 * - 长度 <= 8：显示 "***"
 * - 长度 > 8：显示 "前4位***后4位"
 * 
 * 示例：
 * - "" -> "(无)"
 * - "12345678" -> "***"
 * - "sk-1234567890abcdef" -> "sk-1***cdef"
 */
QString ConfigGroupPanel::maskApiKey(const QString& apiKey) const
{
    if (apiKey.isEmpty()) return tr("(无)");           // 空字符串
    if (apiKey.length() <= 8) return "***";            // 短字符串
    return apiKey.left(4) + "***" + apiKey.right(4);   // 脱敏显示
}
