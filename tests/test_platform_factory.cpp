/**
 * @file test_platform_factory.cpp
 * @brief 平台工厂单元测试
 * 
 * 测试平台工厂的核心功能。
 * 
 * @details
 * 测试覆盖范围：
 * 
 * 工厂创建测试：
 * - 工厂实例创建
 * - 平台名称获取
 * - 平台支持检测
 * - 静态方法测试
 * 
 * 服务创建测试：
 * - ConfigManager 创建
 * - CertManager 创建
 * - PrivilegeManager 创建
 * - HostsManager 创建
 * 
 * 接口验证测试：
 * - ConfigManager 接口方法
 * - CertManager 接口方法
 * - HostsManager 接口方法
 * 
 * 多实例测试：
 * - 多个工厂实例独立性
 * - 多个服务实例独立性
 * 
 * 平台特定验证：
 * - Windows 平台名称
 * - UAC 提权方式
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

#include "platform/platform_factory.h"  // 被测试的类

// ============================================================================
// 测试类定义
// ============================================================================

/**
 * @class TestPlatformFactory
 * @brief 平台工厂单元测试类
 * 
 * 继承自 QObject，使用 Qt Test 框架进行单元测试。
 * 测试平台工厂的各项功能。
 */
class TestPlatformFactory : public QObject
{
    Q_OBJECT  // Qt 元对象系统宏

private:
    QTemporaryDir* m_tempDir;  ///< 临时目录指针，用于存放测试文件

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

    // ========== 工厂创建测试 ==========

    /**
     * @brief 测试工厂实例创建
     * 
     * 验证 PlatformFactory::create() 可以创建有效的工厂实例。
     */
    void testFactoryCreation()
    {
        auto factory = PlatformFactory::create();
        QVERIFY(factory != nullptr);  // 工厂实例不为空
    }

    /**
     * @brief 测试平台名称
     * 
     * 验证 platformName() 返回正确的平台名称。
     */
    void testPlatformName()
    {
        auto factory = PlatformFactory::create();
        std::string name = factory->platformName();
        
        // 平台名称不为空
        QVERIFY(!name.empty());
        
        // Windows 平台特定验证
#if defined(_WIN32) || defined(_WIN64)
        QCOMPARE(name, std::string("Windows"));
#endif
    }

    /**
     * @brief 测试平台支持检测
     * 
     * 验证 isSupported() 返回正确的支持状态。
     */
    void testIsSupported()
    {
        auto factory = PlatformFactory::create();
        
        // Windows 平台应该被支持
#if defined(_WIN32) || defined(_WIN64)
        QVERIFY(factory->isSupported());
#endif
    }

    /**
     * @brief 测试静态方法 - 当前平台
     * 
     * 验证 PlatformFactory::currentPlatform() 静态方法。
     */
    void testStaticCurrentPlatform()
    {
        std::string platform = PlatformFactory::currentPlatform();
        
        // 平台名称不为空
        QVERIFY(!platform.empty());
        
        // Windows 平台特定验证
#if defined(_WIN32) || defined(_WIN64)
        QCOMPARE(platform, std::string("Windows"));
#endif
    }

    /**
     * @brief 测试静态方法 - 平台支持检测
     * 
     * 验证 PlatformFactory::isPlatformSupported() 静态方法。
     */
    void testStaticIsPlatformSupported()
    {
        // Windows 平台应该被支持
#if defined(_WIN32) || defined(_WIN64)
        QVERIFY(PlatformFactory::isPlatformSupported());
#endif
    }

    // ========== 服务创建测试 ==========

    /**
     * @brief 测试创建 ConfigManager
     * 
     * 验证 createConfigManager() 可以创建有效的配置管理器实例。
     */
    void testCreateConfigManager()
    {
        auto factory = PlatformFactory::create();
        std::string configPath = m_tempDir->filePath("config.yaml").toStdString();
        
        // 创建配置管理器
        auto configManager = factory->createConfigManager(configPath);
        
        // 验证实例有效
        QVERIFY(configManager != nullptr);
        QCOMPARE(configManager->configPath(), configPath);  // 路径正确
    }

    /**
     * @brief 测试创建 CertManager
     * 
     * 验证 createCertManager() 可以创建有效的证书管理器实例。
     */
    void testCreateCertManager()
    {
        auto factory = PlatformFactory::create();
        std::string dataDir = m_tempDir->path().toStdString();
        
        // 创建证书管理器
        auto certManager = factory->createCertManager(dataDir);
        
        // 验证实例有效
        QVERIFY(certManager != nullptr);
        QCOMPARE(certManager->dataDir(), dataDir);  // 数据目录正确
    }

    /**
     * @brief 测试创建 PrivilegeManager
     * 
     * 验证 createPrivilegeManager() 可以创建有效的权限管理器实例。
     */
    void testCreatePrivilegeManager()
    {
        auto factory = PlatformFactory::create();
        
        // 创建权限管理器
        auto privilegeManager = factory->createPrivilegeManager();
        
        // 验证实例有效
        QVERIFY(privilegeManager != nullptr);
        
        // 验证平台名称
        std::string platformName = privilegeManager->platformName();
        QVERIFY(!platformName.empty());
        
        // 验证提权方式
        std::string elevationMethod = privilegeManager->elevationMethod();
        QVERIFY(!elevationMethod.empty());
        
        // Windows 平台特定验证
#if defined(_WIN32) || defined(_WIN64)
        // 验证包含 "UAC" 关键字
        QVERIFY(elevationMethod.find("UAC") != std::string::npos);
#endif
    }

    /**
     * @brief 测试创建 HostsManager
     * 
     * 验证 createHostsManager() 可以创建有效的 Hosts 管理器实例。
     */
    void testCreateHostsManager()
    {
        auto factory = PlatformFactory::create();
        std::string backupDir = m_tempDir->path().toStdString();
        
        // 创建 Hosts 管理器
        auto hostsManager = factory->createHostsManager(backupDir);
        
        // 验证实例有效
        QVERIFY(hostsManager != nullptr);
        
        // 验证 hosts 文件路径
        std::string hostsPath = hostsManager->hostsFilePath();
        QVERIFY(!hostsPath.empty());
        
        // 验证备份路径
        std::string backupPath = hostsManager->backupFilePath();
        QVERIFY(!backupPath.empty());
    }

    // ========== 接口验证测试 ==========

    /**
     * @brief 测试 ConfigManager 接口
     * 
     * 验证 ConfigManager 实现了所有接口方法，且使用纯 C++ 类型。
     */
    void testPureCppInterface_ConfigManager()
    {
        auto factory = PlatformFactory::create();
        std::string configPath = m_tempDir->filePath("pure_cpp.yaml").toStdString();
        
        auto configManager = factory->createConfigManager(configPath);
        
        // ========== 验证 std::string 返回值 ==========
        std::string path = configManager->configPath();
        QVERIFY(!path.empty());
        
        // ========== 验证 size_t 返回值 ==========
        size_t count = configManager->configCount();
        QCOMPARE(count, size_t(0));  // 初始为空
        
        // ========== 验证 OperationResult 返回值 ==========
        OperationResult result = configManager->load();
        QVERIFY(result.ok);
        
        // ========== 验证 std::function 回调 ==========
        bool callbackCalled = false;
        configManager->setLogCallback([&callbackCalled](const std::string&, LogLevel) {
            callbackCalled = true;
        });
        
        configManager->load();  // 触发回调
        QVERIFY(callbackCalled);
    }

    /**
     * @brief 测试 CertManager 接口
     * 
     * 验证 CertManager 实现了所有接口方法，且使用纯 C++ 类型。
     */
    void testPureCppInterface_CertManager()
    {
        auto factory = PlatformFactory::create();
        std::string dataDir = m_tempDir->path().toStdString();
        
        auto certManager = factory->createCertManager(dataDir);
        
        // ========== 验证所有路径方法返回 std::string ==========
        std::string caCertPath = certManager->caCertPath();
        std::string caKeyPath = certManager->caKeyPath();
        std::string serverCertPath = certManager->serverCertPath();
        std::string serverKeyPath = certManager->serverKeyPath();
        std::string dir = certManager->dataDir();
        
        // 路径不为空
        QVERIFY(!caCertPath.empty());
        QVERIFY(!caKeyPath.empty());
        QVERIFY(!serverCertPath.empty());
        QVERIFY(!serverKeyPath.empty());
        QCOMPARE(dir, dataDir);
        
        // ========== 验证布尔返回值 ==========
        bool caCertExists = certManager->caCertExists();
        bool serverCertExists = certManager->serverCertExists();
        bool allCertsExist = certManager->allCertsExist();
        
        // 使用 Q_UNUSED 避免未使用变量警告
        Q_UNUSED(caCertExists);
        Q_UNUSED(serverCertExists);
        Q_UNUSED(allCertsExist);
    }

    /**
     * @brief 测试 HostsManager 接口
     * 
     * 验证 HostsManager 实现了所有接口方法，且使用纯 C++ 类型。
     */
    void testPureCppInterface_HostsManager()
    {
        auto factory = PlatformFactory::create();
        std::string backupDir = m_tempDir->path().toStdString();
        
        auto hostsManager = factory->createHostsManager(backupDir);
        
        // ========== 验证 std::vector<std::string> 返回值 ==========
        std::vector<std::string> domains = hostsManager->hijackDomains();
        QVERIFY(!domains.empty());  // 默认域名不为空
        
        // ========== 验证 std::vector<std::string> 参数 ==========
        std::vector<std::string> newDomains = {"test1.com", "test2.com"};
        hostsManager->setHijackDomains(newDomains);
        
        // 验证设置生效
        std::vector<std::string> result = hostsManager->hijackDomains();
        QCOMPARE(result.size(), size_t(2));
        QCOMPARE(result[0], std::string("test1.com"));
        QCOMPARE(result[1], std::string("test2.com"));
    }

    // ========== 多实例测试 ==========

    /**
     * @brief 测试多个工厂实例
     * 
     * 验证可以创建多个独立的工厂实例。
     */
    void testMultipleFactoryInstances()
    {
        // 创建两个工厂实例
        auto factory1 = PlatformFactory::create();
        auto factory2 = PlatformFactory::create();
        
        // 验证实例有效且独立
        QVERIFY(factory1 != nullptr);
        QVERIFY(factory2 != nullptr);
        QVERIFY(factory1.get() != factory2.get());  // 不同的实例
        
        // 验证平台名称一致
        QCOMPARE(factory1->platformName(), factory2->platformName());
    }

    /**
     * @brief 测试多个服务实例
     * 
     * 验证可以创建多个独立的服务实例。
     */
    void testMultipleServiceInstances()
    {
        auto factory = PlatformFactory::create();
        
        // 创建两个配置管理器实例
        std::string path1 = m_tempDir->filePath("config1.yaml").toStdString();
        std::string path2 = m_tempDir->filePath("config2.yaml").toStdString();
        
        auto configManager1 = factory->createConfigManager(path1);
        auto configManager2 = factory->createConfigManager(path2);
        
        // 验证实例有效且独立
        QVERIFY(configManager1 != nullptr);
        QVERIFY(configManager2 != nullptr);
        QVERIFY(configManager1.get() != configManager2.get());  // 不同的实例
        
        // 验证路径独立
        QCOMPARE(configManager1->configPath(), path1);
        QCOMPARE(configManager2->configPath(), path2);
    }
};

// ============================================================================
// 测试入口
// ============================================================================

/**
 * @brief 测试主函数
 * 
 * QTEST_MAIN 宏生成 main() 函数，运行 TestPlatformFactory 中的所有测试。
 */
QTEST_MAIN(TestPlatformFactory)

// 包含 moc 生成的文件
#include "test_platform_factory.moc"
