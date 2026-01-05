/**
 * @file test_config_manager.cpp
 * @brief WindowsConfigManager 单元测试
 * 
 * 测试配置管理器的核心功能。
 * 
 * @details
 * 测试覆盖范围：
 * 
 * 基础功能测试：
 * - 加载不存在的配置文件
 * - 保存和加载配置
 * - DPAPI 加密/解密验证
 * 
 * 配置项操作测试：
 * - 添加单个/多个配置
 * - 更新配置（正常/无效索引）
 * - 删除配置（正常/无效索引）
 * - 获取配置（正常/无效索引）
 * 
 * 配置验证测试：
 * - 缺少必填字段（名称、本地URL、映射URL）
 * - 所有字段为空
 * - 仅空白字符
 * 
 * Qt 信号测试：
 * - configChanged 信号触发
 * - 添加/更新/删除时的信号
 * 
 * 回调测试：
 * - 日志回调
 * - 配置变化回调
 * 
 * AppConfig 辅助方法测试：
 * - getValidConfigs 过滤无效配置
 * - 相等性比较
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

// ============================================================================
// 头文件包含
// ============================================================================

#include <QtTest/QtTest>           // Qt 测试框架
#include <QTemporaryDir>           // 临时目录，用于测试文件隔离
#include <QSignalSpy>              // 信号监听器，用于验证 Qt 信号

#include "platform/windows/windows_config_manager.h"  // 被测试的类

// ============================================================================
// 测试类定义
// ============================================================================

/**
 * @class TestConfigManager
 * @brief WindowsConfigManager 单元测试类
 * 
 * 继承自 QObject，使用 Qt Test 框架进行单元测试。
 * 每个测试方法都是独立的测试用例。
 */
class TestConfigManager : public QObject
{
    Q_OBJECT  // Qt 元对象系统宏，启用信号槽机制

private:
    QTemporaryDir* m_tempDir;  ///< 临时目录指针，用于存放测试配置文件
    
    /**
     * @brief 创建有效的测试配置项
     * 
     * 辅助方法，用于创建包含所有必填字段的有效配置项。
     * 
     * @param name 配置名称，默认为 "Test Config"
     * @return ProxyConfigItem 填充了测试数据的配置项
     */
    ProxyConfigItem createValidConfig(const std::string& name = "Test Config") {
        ProxyConfigItem item;
        item.name = name;                              // 配置名称
        item.localUrl = "api.test.com";                // 本地 URL
        item.mappedUrl = "https://api.backend.com";    // 映射 URL
        item.localModelName = "gpt-4";                 // 本地模型名
        item.mappedModelName = "claude-3";             // 映射模型名
        item.authKey = "test-auth-key";                // 鉴权密钥
        item.apiKey = "test-api-key";                  // API 密钥
        return item;
    }

private slots:
    // ========== 测试生命周期方法 ==========
    
    /**
     * @brief 测试套件初始化
     * 
     * 在所有测试用例执行前调用一次。
     * 创建临时目录用于存放测试文件。
     */
    void initTestCase()
    {
        m_tempDir = new QTemporaryDir();  // 创建临时目录
        QVERIFY(m_tempDir->isValid());    // 验证目录创建成功
    }

    /**
     * @brief 测试套件清理
     * 
     * 在所有测试用例执行后调用一次。
     * 删除临时目录及其内容。
     */
    void cleanupTestCase()
    {
        delete m_tempDir;  // 删除临时目录
    }

    /**
     * @brief 单个测试用例清理
     * 
     * 在每个测试用例执行后调用。
     * 清理临时目录中的所有文件，确保测试隔离。
     */
    void cleanup()
    {
        // 遍历并删除临时目录中的所有文件
        QDir dir(m_tempDir->path());
        for (const QString& file : dir.entryList(QDir::Files)) {
            dir.remove(file);
        }
    }

    // ========== 基础功能测试 ==========

    /**
     * @brief 测试加载不存在的配置文件
     * 
     * 验证当配置文件不存在时，load() 方法应该：
     * - 返回成功（ok = true）
     * - 配置列表为空
     */
    void testLoadNonExistentFile()
    {
        // 准备：创建指向不存在文件的配置管理器
        QString configPath = m_tempDir->filePath("nonexistent.yaml");
        WindowsConfigManager manager(configPath);
        
        // 执行：加载配置
        OperationResult result = manager.load();
        
        // 验证：加载成功但配置为空
        QVERIFY(result.ok);                        // 操作成功
        QCOMPARE(manager.configCount(), size_t(0)); // 配置数量为 0
        QVERIFY(manager.getConfig().isEmpty());    // 配置列表为空
    }

    /**
     * @brief 测试保存和加载配置
     * 
     * 验证配置的持久化功能：
     * 1. 创建配置管理器并添加配置
     * 2. 保存配置到文件
     * 3. 创建新的配置管理器并加载配置
     * 4. 验证加载的配置与原始配置一致
     * 
     * @note 此测试同时验证了 DPAPI 加密/解密功能
     */
    void testSaveAndLoad()
    {
        QString configPath = m_tempDir->filePath("test_config.yaml");
        
        // ========== 阶段 1：创建并保存配置 ==========
        {
            WindowsConfigManager manager(configPath);
            
            // 添加测试配置
            ProxyConfigItem item = createValidConfig();
            OperationResult addResult = manager.addProxyConfig(item);
            QVERIFY(addResult.ok);  // 验证添加成功
            
            // 保存配置到文件
            OperationResult saveResult = manager.save();
            QVERIFY(saveResult.ok);  // 验证保存成功
        }
        
        // ========== 阶段 2：重新加载并验证 ==========
        {
            WindowsConfigManager manager(configPath);
            OperationResult loadResult = manager.load();
            
            // 验证加载成功
            QVERIFY(loadResult.ok);
            QCOMPARE(manager.configCount(), size_t(1));  // 应该有 1 个配置
            
            // 获取加载的配置并验证各字段
            const ProxyConfigItem* loaded = manager.getProxyConfig(0);
            QVERIFY(loaded != nullptr);
            QCOMPARE(loaded->name, std::string("Test Config"));
            QCOMPARE(loaded->localUrl, std::string("api.test.com"));
            QCOMPARE(loaded->mappedUrl, std::string("https://api.backend.com"));
            QCOMPARE(loaded->localModelName, std::string("gpt-4"));
            QCOMPARE(loaded->mappedModelName, std::string("claude-3"));
            
            // 验证敏感字段（经过 DPAPI 加密/解密后应该能正确还原）
            QCOMPARE(loaded->authKey, std::string("test-auth-key"));
            QCOMPARE(loaded->apiKey, std::string("test-api-key"));
        }
    }

    // ========== 配置项操作测试 ==========

    /**
     * @brief 测试添加代理配置
     * 
     * 验证 addProxyConfig() 方法的基本功能。
     */
    void testAddProxyConfig()
    {
        QString configPath = m_tempDir->filePath("test_add.yaml");
        WindowsConfigManager manager(configPath);
        
        // 添加配置
        ProxyConfigItem item = createValidConfig("New Config");
        OperationResult result = manager.addProxyConfig(item);
        
        // 验证添加成功
        QVERIFY(result.ok);
        QCOMPARE(manager.configCount(), size_t(1));
        
        // 验证配置内容
        const ProxyConfigItem* added = manager.getProxyConfig(0);
        QVERIFY(added != nullptr);
        QCOMPARE(added->name, std::string("New Config"));
    }

    /**
     * @brief 测试添加多个配置
     * 
     * 验证可以连续添加多个配置，且顺序正确。
     */
    void testAddMultipleConfigs()
    {
        QString configPath = m_tempDir->filePath("test_multiple.yaml");
        WindowsConfigManager manager(configPath);
        
        // 添加 3 个配置
        manager.addProxyConfig(createValidConfig("Config 1"));
        manager.addProxyConfig(createValidConfig("Config 2"));
        manager.addProxyConfig(createValidConfig("Config 3"));
        
        // 验证配置数量和顺序
        QCOMPARE(manager.configCount(), size_t(3));
        QCOMPARE(manager.getProxyConfig(0)->name, std::string("Config 1"));
        QCOMPARE(manager.getProxyConfig(1)->name, std::string("Config 2"));
        QCOMPARE(manager.getProxyConfig(2)->name, std::string("Config 3"));
    }

    /**
     * @brief 测试更新代理配置
     * 
     * 验证 updateProxyConfig() 方法可以正确更新指定索引的配置。
     */
    void testUpdateProxyConfig()
    {
        QString configPath = m_tempDir->filePath("test_update.yaml");
        WindowsConfigManager manager(configPath);
        
        // 添加原始配置
        manager.addProxyConfig(createValidConfig("Original"));
        
        // 创建更新后的配置
        ProxyConfigItem updated = createValidConfig("Updated");
        updated.localUrl = "api.updated.com";
        
        // 执行更新
        OperationResult result = manager.updateProxyConfig(0, updated);
        
        // 验证更新成功
        QVERIFY(result.ok);
        QCOMPARE(manager.getProxyConfig(0)->name, std::string("Updated"));
        QCOMPARE(manager.getProxyConfig(0)->localUrl, std::string("api.updated.com"));
    }

    /**
     * @brief 测试更新无效索引
     * 
     * 验证当索引超出范围时，updateProxyConfig() 返回错误。
     */
    void testUpdateInvalidIndex()
    {
        QString configPath = m_tempDir->filePath("test_update_invalid.yaml");
        WindowsConfigManager manager(configPath);
        
        // 尝试更新不存在的索引
        ProxyConfigItem item = createValidConfig();
        OperationResult result = manager.updateProxyConfig(999, item);
        
        // 验证操作失败
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
    }

    /**
     * @brief 测试删除代理配置
     * 
     * 验证 removeProxyConfig() 方法可以正确删除指定索引的配置。
     */
    void testRemoveProxyConfig()
    {
        QString configPath = m_tempDir->filePath("test_remove.yaml");
        WindowsConfigManager manager(configPath);
        
        // 添加 3 个配置
        manager.addProxyConfig(createValidConfig("Config 1"));
        manager.addProxyConfig(createValidConfig("Config 2"));
        manager.addProxyConfig(createValidConfig("Config 3"));
        
        QCOMPARE(manager.configCount(), size_t(3));
        
        // 删除中间的配置（索引 1）
        OperationResult result = manager.removeProxyConfig(1);
        
        // 验证删除成功
        QVERIFY(result.ok);
        QCOMPARE(manager.configCount(), size_t(2));
        
        // 验证剩余配置（Config 2 被删除，Config 3 移动到索引 1）
        QCOMPARE(manager.getProxyConfig(0)->name, std::string("Config 1"));
        QCOMPARE(manager.getProxyConfig(1)->name, std::string("Config 3"));
    }

    /**
     * @brief 测试删除无效索引
     * 
     * 验证当索引超出范围时，removeProxyConfig() 返回错误。
     */
    void testRemoveInvalidIndex()
    {
        QString configPath = m_tempDir->filePath("test_remove_invalid.yaml");
        WindowsConfigManager manager(configPath);
        
        // 尝试删除不存在的索引
        OperationResult result = manager.removeProxyConfig(0);
        
        // 验证操作失败
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
    }

    /**
     * @brief 测试获取无效索引的配置
     * 
     * 验证当索引超出范围时，getProxyConfig() 返回 nullptr。
     */
    void testGetProxyConfigInvalidIndex()
    {
        QString configPath = m_tempDir->filePath("test_get_invalid.yaml");
        WindowsConfigManager manager(configPath);
        
        // 尝试获取不存在的索引
        const ProxyConfigItem* item = manager.getProxyConfig(999);
        
        // 验证返回 nullptr
        QVERIFY(item == nullptr);
    }

    // ========== 配置验证测试 ==========

    /**
     * @brief 测试验证 - 缺少名称
     * 
     * 验证当配置名称为空时，添加操作应该失败。
     */
    void testValidateProxyConfig_MissingName()
    {
        QString configPath = m_tempDir->filePath("test_validate_name.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建缺少名称的配置
        ProxyConfigItem item;
        item.name = "";  // 名称为空
        item.localUrl = "api.test.com";
        item.mappedUrl = "https://api.backend.com";
        
        // 尝试添加
        OperationResult result = manager.addProxyConfig(item);
        
        // 验证操作失败
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
        QVERIFY(result.message.find("配置名称") != std::string::npos);  // 错误消息包含字段名
    }

    /**
     * @brief 测试验证 - 缺少本地 URL
     * 
     * 验证当本地 URL 为空时，添加操作应该失败。
     */
    void testValidateProxyConfig_MissingLocalUrl()
    {
        QString configPath = m_tempDir->filePath("test_validate_local.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建缺少本地 URL 的配置
        ProxyConfigItem item;
        item.name = "Test";
        item.localUrl = "";  // 本地 URL 为空
        item.mappedUrl = "https://api.backend.com";
        
        // 尝试添加
        OperationResult result = manager.addProxyConfig(item);
        
        // 验证操作失败
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
        QVERIFY(result.message.find("本地URL") != std::string::npos);
    }

    /**
     * @brief 测试验证 - 缺少映射 URL
     * 
     * 验证当映射 URL 为空时，添加操作应该失败。
     */
    void testValidateProxyConfig_MissingMappedUrl()
    {
        QString configPath = m_tempDir->filePath("test_validate_mapped.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建缺少映射 URL 的配置
        ProxyConfigItem item;
        item.name = "Test";
        item.localUrl = "api.test.com";
        item.mappedUrl = "";  // 映射 URL 为空
        
        // 尝试添加
        OperationResult result = manager.addProxyConfig(item);
        
        // 验证操作失败
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
        QVERIFY(result.message.find("映射URL") != std::string::npos);
    }

    /**
     * @brief 测试验证 - 所有字段为空
     * 
     * 验证当所有字段都为空时，添加操作应该失败。
     */
    void testValidateProxyConfig_AllMissing()
    {
        QString configPath = m_tempDir->filePath("test_validate_all.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建空配置
        ProxyConfigItem item;  // 所有字段默认为空
        
        // 尝试添加
        OperationResult result = manager.addProxyConfig(item);
        
        // 验证操作失败
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
    }

    /**
     * @brief 测试验证 - 仅空白字符
     * 
     * 验证当字段只包含空白字符时，应该被视为空。
     */
    void testValidateProxyConfig_WhitespaceOnly()
    {
        QString configPath = m_tempDir->filePath("test_validate_whitespace.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建只有空白字符的配置
        ProxyConfigItem item;
        item.name = "   ";       // 仅空格
        item.localUrl = "\t\n";  // 仅制表符和换行
        item.mappedUrl = "  \r\n  ";  // 空格和回车换行
        
        // 尝试添加
        OperationResult result = manager.addProxyConfig(item);
        
        // 验证操作失败（空白字符应该被视为空）
        QVERIFY(!result.ok);
        QCOMPARE(result.code, ErrorCode::ConfigInvalid);
    }

    // ========== 配置设置测试 ==========

    /**
     * @brief 测试设置完整配置
     * 
     * 验证 setConfig() 方法可以一次性设置整个配置。
     */
    void testSetConfig()
    {
        QString configPath = m_tempDir->filePath("test_set.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建包含多个配置的 AppConfig
        AppConfig config;
        config.proxyConfigs.push_back(createValidConfig("Config A"));
        config.proxyConfigs.push_back(createValidConfig("Config B"));
        
        // 设置配置
        manager.setConfig(config);
        
        // 验证配置已设置
        QCOMPARE(manager.configCount(), size_t(2));
        QCOMPARE(manager.getConfig().proxyConfigs[0].name, std::string("Config A"));
        QCOMPARE(manager.getConfig().proxyConfigs[1].name, std::string("Config B"));
    }

    // ========== Qt 信号测试 ==========

    /**
     * @brief 测试添加配置时的信号
     * 
     * 验证添加配置时会发出 configChanged 信号。
     */
    void testConfigChangedSignal()
    {
        QString configPath = m_tempDir->filePath("test_signal.yaml");
        WindowsConfigManager manager(configPath);
        
        // 创建信号监听器
        QSignalSpy spy(&manager, &WindowsConfigManager::configChanged);
        QVERIFY(spy.isValid());  // 验证信号存在
        
        // 添加配置
        manager.addProxyConfig(createValidConfig());
        
        // 验证信号被发出
        QCOMPARE(spy.count(), 1);
    }

    /**
     * @brief 测试更新配置时的信号
     * 
     * 验证更新配置时会发出 configChanged 信号。
     */
    void testConfigChangedSignalOnUpdate()
    {
        QString configPath = m_tempDir->filePath("test_signal_update.yaml");
        WindowsConfigManager manager(configPath);
        
        // 先添加一个配置
        manager.addProxyConfig(createValidConfig());
        
        // 创建信号监听器
        QSignalSpy spy(&manager, &WindowsConfigManager::configChanged);
        
        // 更新配置
        manager.updateProxyConfig(0, createValidConfig("Updated"));
        
        // 验证信号被发出
        QCOMPARE(spy.count(), 1);
    }

    /**
     * @brief 测试删除配置时的信号
     * 
     * 验证删除配置时会发出 configChanged 信号。
     */
    void testConfigChangedSignalOnRemove()
    {
        QString configPath = m_tempDir->filePath("test_signal_remove.yaml");
        WindowsConfigManager manager(configPath);
        
        // 先添加一个配置
        manager.addProxyConfig(createValidConfig());
        
        // 创建信号监听器
        QSignalSpy spy(&manager, &WindowsConfigManager::configChanged);
        
        // 删除配置
        manager.removeProxyConfig(0);
        
        // 验证信号被发出
        QCOMPARE(spy.count(), 1);
    }

    // ========== 回调测试 ==========

    /**
     * @brief 测试日志回调
     * 
     * 验证设置日志回调后，操作会触发回调。
     */
    void testLogCallback()
    {
        QString configPath = m_tempDir->filePath("test_callback.yaml");
        WindowsConfigManager manager(configPath);
        
        // 设置日志回调，收集日志消息
        std::vector<std::string> logMessages;
        manager.setLogCallback([&logMessages](const std::string& msg, LogLevel) {
            logMessages.push_back(msg);
        });
        
        // 执行操作
        manager.load();
        
        // 验证回调被调用
        QVERIFY(!logMessages.empty());
    }

    /**
     * @brief 测试配置变化回调
     * 
     * 验证设置配置变化回调后，增删改操作会触发回调。
     */
    void testConfigChangedCallback()
    {
        QString configPath = m_tempDir->filePath("test_changed_callback.yaml");
        WindowsConfigManager manager(configPath);
        
        // 设置配置变化回调，计数调用次数
        int callCount = 0;
        manager.setConfigChangedCallback([&callCount]() {
            callCount++;
        });
        
        // 执行增删改操作
        manager.addProxyConfig(createValidConfig());     // +1
        manager.updateProxyConfig(0, createValidConfig("Updated"));  // +1
        manager.removeProxyConfig(0);                    // +1
        
        // 验证回调被调用 3 次
        QCOMPARE(callCount, 3);
    }

    // ========== 路径测试 ==========

    /**
     * @brief 测试配置文件路径
     * 
     * 验证 configPath() 方法返回正确的路径。
     */
    void testConfigPath()
    {
        QString configPath = m_tempDir->filePath("test_path.yaml");
        WindowsConfigManager manager(configPath);
        
        // 验证路径一致
        QCOMPARE(QString::fromStdString(manager.configPath()), configPath);
    }

    // ========== AppConfig 辅助方法测试 ==========

    /**
     * @brief 测试获取有效配置
     * 
     * 验证 getValidConfigs() 方法可以过滤掉无效配置。
     */
    void testAppConfigGetValidConfigs()
    {
        AppConfig config;
        
        // 添加有效配置
        config.proxyConfigs.push_back(createValidConfig("Valid 1"));
        config.proxyConfigs.push_back(createValidConfig("Valid 2"));
        
        // 添加无效配置（缺少必填字段）
        ProxyConfigItem invalid;
        invalid.name = "Invalid";
        config.proxyConfigs.push_back(invalid);
        
        // 获取有效配置
        auto validConfigs = config.getValidConfigs();
        
        // 验证只返回有效配置
        QCOMPARE(validConfigs.size(), size_t(2));
    }

    /**
     * @brief 测试配置相等性比较
     * 
     * 验证 AppConfig 的相等性运算符。
     */
    void testAppConfigEquality()
    {
        // 创建两个相同的配置
        AppConfig config1;
        config1.proxyConfigs.push_back(createValidConfig());
        
        AppConfig config2;
        config2.proxyConfigs.push_back(createValidConfig());
        
        // 验证相等
        QVERIFY(config1 == config2);
        
        // 修改其中一个
        config2.proxyConfigs[0].name = "Different";
        
        // 验证不相等
        QVERIFY(config1 != config2);
    }
};

// ============================================================================
// 测试入口
// ============================================================================

/**
 * @brief 测试主函数
 * 
 * QTEST_MAIN 宏生成 main() 函数，运行 TestConfigManager 中的所有测试。
 */
QTEST_MAIN(TestConfigManager)

// 包含 moc 生成的文件（Qt 元对象编译器）
#include "test_config_manager.moc"
