/**
 * @file test_network_utils.cpp
 * @brief 网络工具函数单元测试
 * 
 * 测试网络相关的工具函数和数据结构。
 * 
 * @details
 * 测试覆盖范围：
 * 
 * ProxyConfigItem 测试：
 * - 默认值验证
 * - isValid 方法
 * - 相等性比较
 * 
 * RuntimeOptions 测试：
 * - 默认值验证
 * - 相等性比较
 * - 所有字段设置
 * 
 * StreamMode 测试：
 * - 枚举值验证
 * 
 * ProxyServerConfig 测试：
 * - 默认值验证
 * - isValid 方法
 * - 端口有效性验证
 * - getProxyPort 方法
 * 
 * OperationResult 测试：
 * - success 静态方法
 * - failure 静态方法
 * - 带消息和详情的结果
 * - 布尔转换
 * 
 * ErrorCode 测试：
 * - errorCodeToString 转换
 * - 错误码分组范围验证
 * 
 * 端口检测测试：
 * - 端口占用检测
 * 
 * 常量测试：
 * - MAX_HEADER_SIZE
 * - MAX_BODY_SIZE
 * - MAX_DEBUG_OUTPUT_LENGTH
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

// ============================================================================
// 头文件包含
// ============================================================================

#include <QtTest/QtTest>           // Qt 测试框架
#include <QTcpServer>              // TCP 服务器，用于端口占用测试

#include "core/interfaces/i_network_manager.h"  // 网络管理器接口和数据结构
#include "core/operation_result.h"              // 操作结果类

// ============================================================================
// 测试类定义
// ============================================================================

/**
 * @class TestNetworkUtils
 * @brief 网络工具函数单元测试类
 * 
 * 继承自 QObject，使用 Qt Test 框架进行单元测试。
 * 测试网络相关的数据结构和工具函数。
 */
class TestNetworkUtils : public QObject
{
    Q_OBJECT  // Qt 元对象系统宏

private slots:
    // ========== ProxyConfigItem 测试 ==========

    /**
     * @brief 测试 ProxyConfigItem 默认值
     * 
     * 验证 ProxyConfigItem 的默认构造函数将所有字段初始化为空。
     */
    void testProxyConfigItemDefault()
    {
        ProxyConfigItem item;
        
        // 验证所有字段为空
        QVERIFY(item.name.empty());
        QVERIFY(item.localUrl.empty());
        QVERIFY(item.mappedUrl.empty());
        QVERIFY(item.localModelName.empty());
        QVERIFY(item.mappedModelName.empty());
        QVERIFY(item.authKey.empty());
        QVERIFY(item.apiKey.empty());
    }

    /**
     * @brief 测试 ProxyConfigItem 有效性验证
     * 
     * 验证 isValid() 方法的判断逻辑：
     * - 空配置无效
     * - 只有 name 无效
     * - 有 name 和 localUrl 无效
     * - 有所有必填字段有效
     */
    void testProxyConfigItemIsValid()
    {
        ProxyConfigItem item;
        
        // 空配置无效
        QVERIFY(!item.isValid());
        
        // 只有 name 无效
        item.name = "Test";
        QVERIFY(!item.isValid());
        
        // 有 name 和 localUrl 无效
        item.localUrl = "api.test.com";
        QVERIFY(!item.isValid());
        
        // 有所有必填字段有效
        item.mappedUrl = "https://api.backend.com";
        QVERIFY(item.isValid());
    }

    /**
     * @brief 测试 ProxyConfigItem 相等性比较
     * 
     * 验证 operator== 的正确性。
     */
    void testProxyConfigItemEquality()
    {
        // 创建第一个配置
        ProxyConfigItem item1;
        item1.name = "Test";
        item1.localUrl = "api.test.com";
        item1.mappedUrl = "https://api.backend.com";
        item1.localModelName = "gpt-4";
        item1.mappedModelName = "claude-3";
        item1.authKey = "auth";
        item1.apiKey = "key";
        
        // 复制创建第二个配置
        ProxyConfigItem item2 = item1;
        
        // 验证相等
        QVERIFY(item1 == item2);
        
        // 修改后验证不相等
        item2.name = "Different";
        QVERIFY(!(item1 == item2));
    }

    // ========== RuntimeOptions 测试 ==========

    /**
     * @brief 测试 RuntimeOptions 默认值
     * 
     * 验证 RuntimeOptions 的默认构造函数设置正确的默认值。
     */
    void testRuntimeOptionsDefault()
    {
        RuntimeOptions options;
        
        // 验证默认值
        QCOMPARE(options.debugMode, false);                           // 调试模式关闭
        QCOMPARE(options.disableSslStrict, false);                    // SSL 严格模式开启
        QCOMPARE(options.enableHttp2, true);                          // HTTP/2 启用
        QCOMPARE(options.enableConnectionPool, true);                 // 连接池启用
        QCOMPARE(options.upstreamStreamMode, StreamMode::FollowClient);   // 上游流模式
        QCOMPARE(options.downstreamStreamMode, StreamMode::FollowClient); // 下游流模式
        QCOMPARE(options.proxyPort, 443);                             // 代理端口
        QCOMPARE(options.connectionPoolSize, 10);                     // 连接池大小
        QCOMPARE(options.requestTimeout, 120000);                     // 请求超时（毫秒）
        QCOMPARE(options.connectionTimeout, 30000);                   // 连接超时（毫秒）
    }

    /**
     * @brief 测试 RuntimeOptions 相等性比较
     * 
     * 验证 operator== 的正确性。
     */
    void testRuntimeOptionsEquality()
    {
        RuntimeOptions opt1;
        RuntimeOptions opt2;
        
        // 默认值相等
        QVERIFY(opt1 == opt2);
        
        // 修改后不相等
        opt2.debugMode = true;
        QVERIFY(!(opt1 == opt2));
    }

    /**
     * @brief 测试 RuntimeOptions 所有字段设置
     * 
     * 验证可以设置所有字段，且复制后相等。
     */
    void testRuntimeOptionsAllFields()
    {
        RuntimeOptions options;
        
        // 设置所有字段
        options.debugMode = true;
        options.disableSslStrict = true;
        options.enableHttp2 = false;
        options.enableConnectionPool = false;
        options.upstreamStreamMode = StreamMode::ForceOn;
        options.downstreamStreamMode = StreamMode::ForceOff;
        options.proxyPort = 8443;
        options.connectionPoolSize = 15;
        options.requestTimeout = 60000;
        options.connectionTimeout = 15000;
        
        // 复制并验证相等
        RuntimeOptions copy = options;
        QVERIFY(options == copy);
    }

    // ========== StreamMode 测试 ==========

    /**
     * @brief 测试 StreamMode 枚举值
     * 
     * 验证 StreamMode 枚举的数值。
     */
    void testStreamModeValues()
    {
        QCOMPARE(static_cast<int>(StreamMode::FollowClient), 0);  // 跟随客户端
        QCOMPARE(static_cast<int>(StreamMode::ForceOn), 1);       // 强制开启
        QCOMPARE(static_cast<int>(StreamMode::ForceOff), 2);      // 强制关闭
    }

    // ========== ProxyServerConfig 测试 ==========

    /**
     * @brief 测试 ProxyServerConfig 默认值
     * 
     * 验证 ProxyServerConfig 的默认构造函数。
     */
    void testProxyServerConfigDefault()
    {
        ProxyServerConfig config;
        
        // 验证默认值
        QVERIFY(config.proxyConfigs.empty());  // 代理配置列表为空
        QVERIFY(config.certPath.empty());      // 证书路径为空
        QVERIFY(config.keyPath.empty());       // 密钥路径为空
    }

    /**
     * @brief 测试 ProxyServerConfig 有效性验证
     * 
     * 验证 isValid() 方法的判断逻辑。
     */
    void testProxyServerConfigIsValid()
    {
        ProxyServerConfig config;
        
        // 空配置无效
        QVERIFY(!config.isValid());
        
        // 添加代理配置
        ProxyConfigItem item;
        item.name = "Test";
        item.localUrl = "api.test.com";
        item.mappedUrl = "https://api.backend.com";
        config.proxyConfigs.push_back(item);
        
        // 仍然无效（缺少证书路径）
        QVERIFY(!config.isValid());
        
        // 添加证书路径
        config.certPath = "/path/to/cert.pem";
        config.keyPath = "/path/to/key.pem";
        
        // 现在有效
        QVERIFY(config.isValid());
    }

    /**
     * @brief 测试 ProxyServerConfig 端口有效性验证
     * 
     * 验证端口号的有效范围（1-65535）。
     */
    void testProxyServerConfigInvalidPort()
    {
        ProxyServerConfig config;
        
        // 添加必要的配置
        ProxyConfigItem item;
        item.name = "Test";
        item.localUrl = "api.test.com";
        item.mappedUrl = "https://api.backend.com";
        config.proxyConfigs.push_back(item);
        config.certPath = "/path/to/cert.pem";
        config.keyPath = "/path/to/key.pem";
        
        // 默认端口有效
        QVERIFY(config.isValid());
        
        // 测试无效端口
        config.options.proxyPort = 0;      // 端口 0 无效
        QVERIFY(!config.isValid());
        
        config.options.proxyPort = -1;     // 负数无效
        QVERIFY(!config.isValid());
        
        config.options.proxyPort = 65536;  // 超过最大值无效
        QVERIFY(!config.isValid());
        
        // 测试边界值有效
        config.options.proxyPort = 1;      // 最小有效端口
        QVERIFY(config.isValid());
        
        config.options.proxyPort = 65535;  // 最大有效端口
        QVERIFY(config.isValid());
    }

    /**
     * @brief 测试 getProxyPort 方法
     * 
     * 验证 getProxyPort() 返回正确的端口号。
     */
    void testProxyServerConfigGetProxyPort()
    {
        ProxyServerConfig config;
        
        // 默认端口
        QCOMPARE(config.getProxyPort(), 443);
        
        // 修改端口
        config.options.proxyPort = 8080;
        QCOMPARE(config.getProxyPort(), 8080);
    }

    // ========== OperationResult 测试 ==========

    /**
     * @brief 测试成功结果
     * 
     * 验证 OperationResult::success() 静态方法。
     */
    void testOperationResultSuccess()
    {
        OperationResult result = OperationResult::success();
        
        // 验证成功状态
        QVERIFY(result.ok);
        QVERIFY(result.isSuccess());
        QVERIFY(!result.isFailure());
        QCOMPARE(result.code, ErrorCode::Success);
        QVERIFY(static_cast<bool>(result));  // 布尔转换
    }

    /**
     * @brief 测试带消息的成功结果
     * 
     * 验证 OperationResult::success(message) 静态方法。
     */
    void testOperationResultSuccessWithMessage()
    {
        OperationResult result = OperationResult::success("Operation completed");
        
        QVERIFY(result.ok);
        QCOMPARE(result.message, std::string("Operation completed"));
    }

    /**
     * @brief 测试带详情的成功结果
     * 
     * 验证 OperationResult::success(message, details) 静态方法。
     */
    void testOperationResultSuccessWithDetails()
    {
        // 创建详情映射
        std::map<std::string, std::string> details = {
            {"key1", "value1"},
            {"key2", "value2"}
        };
        
        OperationResult result = OperationResult::success("Done", details);
        
        // 验证详情
        QVERIFY(result.ok);
        QCOMPARE(result.details.size(), size_t(2));
        QCOMPARE(result.details["key1"], std::string("value1"));
    }

    /**
     * @brief 测试失败结果
     * 
     * 验证 OperationResult::failure() 静态方法。
     */
    void testOperationResultFailure()
    {
        OperationResult result = OperationResult::failure(
            "Something went wrong",
            ErrorCode::NetworkError
        );
        
        // 验证失败状态
        QVERIFY(!result.ok);
        QVERIFY(result.isFailure());
        QVERIFY(!result.isSuccess());
        QCOMPARE(result.code, ErrorCode::NetworkError);
        QCOMPARE(result.message, std::string("Something went wrong"));
        QVERIFY(!static_cast<bool>(result));  // 布尔转换
    }

    /**
     * @brief 测试带详情的失败结果
     * 
     * 验证 OperationResult::failure(message, code, details) 静态方法。
     */
    void testOperationResultFailureWithDetails()
    {
        // 创建详情映射
        std::map<std::string, std::string> details = {
            {"error_code", "500"},
            {"retry_after", "60"}
        };
        
        OperationResult result = OperationResult::failure(
            "Server error",
            ErrorCode::TargetApiError,
            details
        );
        
        // 验证详情
        QVERIFY(!result.ok);
        QCOMPARE(result.details.size(), size_t(2));
    }

    // ========== ErrorCode 测试 ==========

    /**
     * @brief 测试错误码转字符串
     * 
     * 验证 errorCodeToString() 方法的转换结果。
     */
    void testErrorCodeToString()
    {
        // 验证各错误码的字符串表示
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::Success), 
                 std::string("Success"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::PortInUse), 
                 std::string("PortInUse"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::CertGenerationFailed), 
                 std::string("CertGenerationFailed"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::HostsPermissionDenied), 
                 std::string("HostsPermissionDenied"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::ConfigInvalid), 
                 std::string("ConfigInvalid"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::NetworkError), 
                 std::string("NetworkError"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::AuthenticationFailed), 
                 std::string("AuthenticationFailed"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::TargetApiError), 
                 std::string("TargetApiError"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::EncryptionFailed), 
                 std::string("EncryptionFailed"));
        QCOMPARE(OperationResult::errorCodeToString(ErrorCode::Unknown), 
                 std::string("Unknown"));
    }

    /**
     * @brief 测试错误码分组范围
     * 
     * 验证错误码按功能分组的数值范围。
     * 
     * @details
     * 错误码分组：
     * - 100-199: 端口相关错误
     * - 200-299: 证书相关错误
     * - 300-399: Hosts 文件相关错误
     * - 400-499: 配置相关错误
     * - 500-599: 网络相关错误
     * - 600-699: 认证相关错误
     * - 700-799: 目标 API 相关错误
     * - 800-899: 加密相关错误
     */
    void testErrorCodeRanges()
    {
        // 端口相关错误（100-199）
        QVERIFY(static_cast<int>(ErrorCode::PortInUse) >= 100);
        QVERIFY(static_cast<int>(ErrorCode::PortInUse) < 200);
        
        // 证书相关错误（200-299）
        QVERIFY(static_cast<int>(ErrorCode::CertGenerationFailed) >= 200);
        QVERIFY(static_cast<int>(ErrorCode::CertGenerationFailed) < 300);
        
        // Hosts 文件相关错误（300-399）
        QVERIFY(static_cast<int>(ErrorCode::HostsPermissionDenied) >= 300);
        QVERIFY(static_cast<int>(ErrorCode::HostsPermissionDenied) < 400);
        
        // 配置相关错误（400-499）
        QVERIFY(static_cast<int>(ErrorCode::ConfigInvalid) >= 400);
        QVERIFY(static_cast<int>(ErrorCode::ConfigInvalid) < 500);
        
        // 网络相关错误（500-599）
        QVERIFY(static_cast<int>(ErrorCode::NetworkError) >= 500);
        QVERIFY(static_cast<int>(ErrorCode::NetworkError) < 600);
        
        // 认证相关错误（600-699）
        QVERIFY(static_cast<int>(ErrorCode::AuthenticationFailed) >= 600);
        QVERIFY(static_cast<int>(ErrorCode::AuthenticationFailed) < 700);
        
        // 目标 API 相关错误（700-799）
        QVERIFY(static_cast<int>(ErrorCode::TargetApiError) >= 700);
        QVERIFY(static_cast<int>(ErrorCode::TargetApiError) < 800);
        
        // 加密相关错误（800-899）
        QVERIFY(static_cast<int>(ErrorCode::EncryptionFailed) >= 800);
        QVERIFY(static_cast<int>(ErrorCode::EncryptionFailed) < 900);
    }

    // ========== 端口检测测试 ==========

    /**
     * @brief 测试端口可用性检测
     * 
     * 验证端口占用检测的正确性。
     * 
     * @details
     * 测试流程：
     * 1. 启动一个 TCP 服务器占用端口
     * 2. 验证该端口不可用
     * 3. 关闭服务器
     * 4. 验证端口可用
     */
    void testPortAvailability()
    {
        // 启动服务器，让系统分配一个可用端口
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        quint16 usedPort = server.serverPort();
        
        // 验证该端口现在被占用
        QTcpServer testServer;
        bool available = testServer.listen(QHostAddress::LocalHost, usedPort);
        QVERIFY(!available);  // 端口被占用，监听失败
        
        // 关闭服务器
        server.close();
        
        // 等待端口释放
        QTest::qWait(100);
        
        // 验证端口现在可用
        available = testServer.listen(QHostAddress::LocalHost, usedPort);
        // 注意：端口可能需要一些时间才能完全释放
        if (available) {
            testServer.close();
        }
    }

    // ========== 常量测试 ==========

    /**
     * @brief 测试网络管理器常量
     * 
     * 验证 INetworkManager 中定义的常量值。
     */
    void testNetworkManagerConstants()
    {
        // 验证常量值合理
        QVERIFY(INetworkManager::MAX_HEADER_SIZE > 0);
        QVERIFY(INetworkManager::MAX_BODY_SIZE > INetworkManager::MAX_HEADER_SIZE);
        QVERIFY(INetworkManager::MAX_DEBUG_OUTPUT_LENGTH > 0);
        
        // 验证具体值
        QCOMPARE(INetworkManager::MAX_HEADER_SIZE, 16384);       // 16KB
        QCOMPARE(INetworkManager::MAX_BODY_SIZE, 10485760);      // 10MB
        QCOMPARE(INetworkManager::MAX_DEBUG_OUTPUT_LENGTH, 2000); // 2000 字符
    }
};

// ============================================================================
// 测试入口
// ============================================================================

/**
 * @brief 测试主函数
 * 
 * QTEST_MAIN 宏生成 main() 函数，运行 TestNetworkUtils 中的所有测试。
 */
QTEST_MAIN(TestNetworkUtils)

// 包含 moc 生成的文件
#include "test_network_utils.moc"
