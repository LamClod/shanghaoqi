/**
 * @file test_hosts_manager.cpp
 * @brief WindowsHostsManager 单元测试
 * 
 * 测试 Hosts 文件管理器的核心功能。
 * 
 * @details
 * 测试覆盖范围：
 * 
 * 构造和路径测试：
 * - 构造函数初始化
 * - hosts 文件路径
 * - 备份文件路径
 * - 备份目录自动创建
 * 
 * 域名配置测试：
 * - 默认劫持域名
 * - 设置自定义域名
 * - 设置空域名列表
 * 
 * 回调测试：
 * - 日志回调触发
 * 
 * Qt 信号测试：
 * - logMessage 信号触发
 * - 信号参数验证
 * 
 * 接口一致性测试：
 * - IHostsManager 接口完整实现
 * 
 * 边界情况测试：
 * - 特殊字符域名
 * - Unicode 域名
 * - 大量域名列表
 * 
 * 多实例测试：
 * - 多个管理器实例独立性
 * 
 * @note 实际的 hosts 文件操作需要管理员权限，
 *       因此这些测试主要验证逻辑正确性，而非实际文件操作。
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

#include "platform/windows/windows_hosts_manager.h"  // 被测试的类

// ============================================================================
// 测试类定义
// ============================================================================

/**
 * @class TestHostsManager
 * @brief WindowsHostsManager 单元测试类
 * 
 * 继承自 QObject，使用 Qt Test 框架进行单元测试。
 * 测试 Hosts 文件管理器的各项功能。
 */
class TestHostsManager : public QObject
{
    Q_OBJECT  // Qt 元对象系统宏

private:
    QTemporaryDir* m_tempDir;  ///< 临时目录指针，用于存放测试备份文件

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
     * 清理临时目录中的所有文件。
     */
    void cleanup()
    {
        // 遍历并删除临时目录中的所有文件
        QDir dir(m_tempDir->path());
        for (const QString& file : dir.entryList(QDir::Files)) {
            dir.remove(file);
        }
    }

    // ========== 构造和路径测试 ==========

    /**
     * @brief 测试构造函数
     * 
     * 验证 WindowsHostsManager 构造后：
     * - hosts 文件路径不为空
     * - 路径包含 "hosts" 关键字
     */
    void testConstruction()
    {
        // 创建管理器实例
        WindowsHostsManager manager(m_tempDir->path());
        
        // 验证 hosts 文件路径
        std::string hostsPath = manager.hostsFilePath();
        QVERIFY(!hostsPath.empty());                                    // 路径不为空
        QVERIFY(QString::fromStdString(hostsPath).contains("hosts"));   // 包含 "hosts"
    }

    /**
     * @brief 测试备份文件路径
     * 
     * 验证备份文件路径：
     * - 在指定的备份目录下
     * - 包含 "hosts" 关键字
     */
    void testBackupPath()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 获取备份路径
        std::string backupPath = manager.backupFilePath();
        
        // 验证备份路径在指定目录下
        QVERIFY(QString::fromStdString(backupPath).startsWith(m_tempDir->path()));
        QVERIFY(QString::fromStdString(backupPath).contains("hosts"));
    }

    /**
     * @brief 测试备份目录自动创建
     * 
     * 验证当备份目录不存在时，构造函数会自动创建。
     */
    void testBackupDirectoryCreation()
    {
        // 指定一个不存在的嵌套目录
        QString subDir = m_tempDir->filePath("backup/nested");
        
        // 创建管理器（应该自动创建目录）
        WindowsHostsManager manager(subDir);
        
        // 验证目录已创建
        QVERIFY(QDir(subDir).exists());
    }

    // ========== 域名配置测试 ==========

    /**
     * @brief 测试默认劫持域名
     * 
     * 验证默认的劫持域名列表包含预设的 API 域名。
     */
    void testDefaultHijackDomains()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 获取默认域名列表
        std::vector<std::string> domains = manager.hijackDomains();
        
        // 验证默认域名
        QCOMPARE(domains.size(), size_t(2));
        QCOMPARE(domains[0], std::string("api.openai.com"));
        QCOMPARE(domains[1], std::string("api.anthropic.com"));
    }

    /**
     * @brief 测试设置自定义劫持域名
     * 
     * 验证可以设置自定义的劫持域名列表。
     */
    void testSetHijackDomains()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 设置自定义域名列表
        std::vector<std::string> customDomains = {
            "custom.api.com",
            "another.api.com",
            "third.api.com"
        };
        
        manager.setHijackDomains(customDomains);
        
        // 验证域名已更新
        std::vector<std::string> result = manager.hijackDomains();
        QCOMPARE(result.size(), size_t(3));
        QCOMPARE(result[0], std::string("custom.api.com"));
        QCOMPARE(result[1], std::string("another.api.com"));
        QCOMPARE(result[2], std::string("third.api.com"));
    }

    /**
     * @brief 测试设置空域名列表
     * 
     * 验证可以设置空的劫持域名列表。
     */
    void testSetEmptyHijackDomains()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 设置空列表
        std::vector<std::string> emptyDomains;
        manager.setHijackDomains(emptyDomains);
        
        // 验证列表为空
        std::vector<std::string> result = manager.hijackDomains();
        QVERIFY(result.empty());
    }

    // ========== 回调测试 ==========

    /**
     * @brief 测试日志回调
     * 
     * 验证设置日志回调后，操作会触发回调。
     */
    void testLogCallback()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 设置日志回调，收集日志
        std::vector<std::pair<std::string, LogLevel>> logs;
        manager.setLogCallback([&logs](const std::string& msg, LogLevel level) {
            logs.push_back({msg, level});
        });
        
        // 触发一个会产生日志的操作
        // backup() 会尝试读取 hosts 文件并记录日志
        manager.backup();
        
        // 验证回调被调用（无论操作成功与否都应该有日志）
        QVERIFY(!logs.empty());
    }

    // ========== Qt 信号测试 ==========

    /**
     * @brief 测试日志消息信号
     * 
     * 验证操作时会发出 logMessage 信号。
     */
    void testLogMessageSignal()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 创建信号监听器
        QSignalSpy spy(&manager, &WindowsHostsManager::logMessage);
        QVERIFY(spy.isValid());  // 验证信号存在
        
        // 触发日志
        manager.backup();
        
        // 验证信号被发出
        QVERIFY(spy.count() > 0);
    }

    /**
     * @brief 测试信号包含消息内容
     * 
     * 验证 logMessage 信号携带的消息不为空。
     */
    void testSignalContainsMessage()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 创建信号监听器
        QSignalSpy spy(&manager, &WindowsHostsManager::logMessage);
        
        // 触发日志
        manager.backup();
        
        // 验证信号参数
        if (spy.count() > 0) {
            QList<QVariant> arguments = spy.first();
            QString message = arguments.at(0).toString();
            
            // 消息不应为空
            QVERIFY(!message.isEmpty());
        }
    }

    // ========== hasEntry 测试 ==========

    /**
     * @brief 测试 hasEntry 方法存在
     * 
     * 验证 hasEntry() 方法可以正常调用。
     * 
     * @note 不验证返回值，因为结果取决于系统状态
     */
    void testHasEntryMethodExists()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 只验证方法可调用
        bool result = manager.hasEntry();
        Q_UNUSED(result);  // 忽略返回值
        
        QVERIFY(true);  // 方法调用成功即通过
    }

    // ========== 接口一致性测试 ==========

    /**
     * @brief 测试接口完整性
     * 
     * 验证 WindowsHostsManager 实现了 IHostsManager 接口的所有方法。
     */
    void testInterfaceCompliance()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // ========== 路径方法 ==========
        QVERIFY(!manager.hostsFilePath().empty());   // hosts 文件路径
        QVERIFY(!manager.backupFilePath().empty());  // 备份文件路径
        
        // ========== 域名方法 ==========
        std::vector<std::string> domains = manager.hijackDomains();
        QVERIFY(!domains.empty());  // 默认域名不为空
        
        manager.setHijackDomains({"test.com"});
        QCOMPARE(manager.hijackDomains().size(), size_t(1));  // 设置后生效
        
        // ========== 回调方法 ==========
        manager.setLogCallback([](const std::string&, LogLevel) {});  // 可以设置回调
        
        // ========== 状态查询 ==========
        (void)manager.hasEntry();  // 方法可调用
    }

    // ========== 边界情况测试 ==========

    /**
     * @brief 测试特殊字符域名
     * 
     * 验证可以设置包含特殊字符的域名。
     */
    void testSpecialCharactersInDomain()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 设置包含特殊字符的域名
        std::vector<std::string> domains = {
            "api.test-domain.com",   // 连字符
            "api.test_domain.com",   // 下划线
            "api.123.com"            // 数字
        };
        
        manager.setHijackDomains(domains);
        
        // 验证设置成功
        std::vector<std::string> result = manager.hijackDomains();
        QCOMPARE(result.size(), size_t(3));
    }

    /**
     * @brief 测试 Unicode 域名
     * 
     * 验证可以设置包含 Unicode 字符的域名（如中文）。
     */
    void testUnicodeDomain()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 设置中文域名
        std::vector<std::string> domains = {
            "api.测试.com"  // 中文域名
        };
        
        manager.setHijackDomains(domains);
        
        // 验证设置成功
        std::vector<std::string> result = manager.hijackDomains();
        QCOMPARE(result.size(), size_t(1));
        QCOMPARE(result[0], std::string("api.测试.com"));
    }

    /**
     * @brief 测试大量域名列表
     * 
     * 验证可以设置大量域名（压力测试）。
     */
    void testLongDomainList()
    {
        WindowsHostsManager manager(m_tempDir->path());
        
        // 生成 100 个域名
        std::vector<std::string> domains;
        for (int i = 0; i < 100; ++i) {
            domains.push_back("api" + std::to_string(i) + ".test.com");
        }
        
        manager.setHijackDomains(domains);
        
        // 验证所有域名都已设置
        std::vector<std::string> result = manager.hijackDomains();
        QCOMPARE(result.size(), size_t(100));
    }

    // ========== 多实例测试 ==========

    /**
     * @brief 测试多个管理器实例独立性
     * 
     * 验证多个 WindowsHostsManager 实例之间相互独立。
     */
    void testMultipleInstances()
    {
        // 创建两个独立的管理器实例
        WindowsHostsManager manager1(m_tempDir->filePath("backup1"));
        WindowsHostsManager manager2(m_tempDir->filePath("backup2"));
        
        // 分别设置不同的域名
        manager1.setHijackDomains({"domain1.com"});
        manager2.setHijackDomains({"domain2.com", "domain3.com"});
        
        // 验证各实例独立
        QCOMPARE(manager1.hijackDomains().size(), size_t(1));
        QCOMPARE(manager2.hijackDomains().size(), size_t(2));
        
        // 验证备份路径不同
        QVERIFY(manager1.backupFilePath() != manager2.backupFilePath());
    }
};

// ============================================================================
// 测试入口
// ============================================================================

/**
 * @brief 测试主函数
 * 
 * QTEST_MAIN 宏生成 main() 函数，运行 TestHostsManager 中的所有测试。
 */
QTEST_MAIN(TestHostsManager)

// 包含 moc 生成的文件
#include "test_hosts_manager.moc"
