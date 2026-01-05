/**
 * @file test_log_manager.cpp
 * @brief LogManager 单元测试
 * 
 * 测试日志管理器的核心功能。
 * 
 * @details
 * 测试覆盖范围：
 * 
 * 初始化测试：
 * - 基本初始化
 * - 带错误日志文件初始化
 * - 自动创建目录
 * - 重复初始化
 * - 关闭日志系统
 * 
 * 日志记录测试：
 * - Info 级别日志
 * - Debug 级别日志
 * - Warning 级别日志
 * - Error 级别日志（同时写入错误日志文件）
 * - 指定级别日志
 * 
 * 日志宏测试：
 * - LOG_INFO、LOG_DEBUG、LOG_WARNING、LOG_ERROR
 * 
 * 回调测试：
 * - 日志回调触发
 * - 多级别回调
 * 
 * Qt 信号测试：
 * - logMessageSignal 信号触发
 * - 信号参数验证
 * 
 * 静态方法测试：
 * - formatMessage 格式化
 * - timestamp 时间戳格式
 * - levelToString 级别转换
 * 
 * 边界情况测试：
 * - 空消息
 * - 超长消息
 * - 特殊字符（中文、日文、韩文、emoji）
 * - 大量日志条目
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
#include <QFile>                   // 文件操作
#include <QTextStream>             // 文本流

#include "core/qt/log_manager.h"   // 被测试的类

// ============================================================================
// 测试类定义
// ============================================================================

/**
 * @class TestLogManager
 * @brief LogManager 单元测试类
 * 
 * 继承自 QObject，使用 Qt Test 框架进行单元测试。
 * 测试日志管理器的各项功能。
 */
class TestLogManager : public QObject
{
    Q_OBJECT  // Qt 元对象系统宏

private:
    QTemporaryDir* m_tempDir;  ///< 临时目录指针，用于存放测试日志文件

private slots:
    // ========== 测试生命周期方法 ==========

    /**
     * @brief 测试套件初始化
     * 
     * 在所有测试用例执行前调用一次。
     * 创建临时目录用于存放测试日志文件。
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
     * 关闭日志系统并清理临时文件。
     */
    void cleanup()
    {
        // 关闭日志系统，确保文件句柄释放
        LogManager::instance().shutdown();
        
        // 遍历并删除临时目录中的所有文件
        QDir dir(m_tempDir->path());
        for (const QString& file : dir.entryList(QDir::Files)) {
            dir.remove(file);
        }
    }

    // ========== 初始化测试 ==========

    /**
     * @brief 测试基本初始化
     * 
     * 验证 LogManager 可以正常初始化。
     */
    void testInitialize()
    {
        QString logPath = m_tempDir->filePath("test.log");
        
        // 初始化日志系统
        bool result = LogManager::instance().initialize(logPath.toStdString());
        
        // 验证初始化成功
        QVERIFY(result);
        QVERIFY(LogManager::instance().isInitialized());
    }

    /**
     * @brief 测试带错误日志文件初始化
     * 
     * 验证可以同时指定主日志文件和错误日志文件。
     */
    void testInitializeWithErrorLog()
    {
        QString logPath = m_tempDir->filePath("app.log");
        QString errorLogPath = m_tempDir->filePath("error.log");
        
        // 初始化日志系统（带错误日志文件）
        bool result = LogManager::instance().initialize(
            logPath.toStdString(),
            errorLogPath.toStdString()
        );
        
        // 验证初始化成功
        QVERIFY(result);
        QVERIFY(LogManager::instance().isInitialized());
        QVERIFY(LogManager::instance().hasErrorLogFile());  // 有错误日志文件
    }

    /**
     * @brief 测试自动创建目录
     * 
     * 验证当日志目录不存在时，初始化会自动创建。
     */
    void testInitializeCreatesDirectory()
    {
        // 指定一个不存在的嵌套目录
        QString subDir = m_tempDir->filePath("subdir/nested");
        QString logPath = subDir + "/test.log";
        
        // 初始化日志系统
        bool result = LogManager::instance().initialize(logPath.toStdString());
        
        // 验证初始化成功且目录已创建
        QVERIFY(result);
        QVERIFY(QDir(subDir).exists());
    }

    /**
     * @brief 测试重复初始化
     * 
     * 验证重复调用 initialize() 不会出错。
     */
    void testDoubleInitialize()
    {
        QString logPath = m_tempDir->filePath("test.log");
        
        // 第一次初始化
        LogManager::instance().initialize(logPath.toStdString());
        
        // 第二次初始化（应该返回 true，表示已初始化）
        bool result = LogManager::instance().initialize(logPath.toStdString());
        
        QVERIFY(result);
    }

    /**
     * @brief 测试关闭日志系统
     * 
     * 验证 shutdown() 方法可以正确关闭日志系统。
     */
    void testShutdown()
    {
        QString logPath = m_tempDir->filePath("test.log");
        
        // 初始化并关闭
        LogManager::instance().initialize(logPath.toStdString());
        LogManager::instance().shutdown();
        
        // 验证已关闭
        QVERIFY(!LogManager::instance().isInitialized());
    }

    // ========== 日志记录测试 ==========

    /**
     * @brief 测试 Info 级别日志
     * 
     * 验证 info() 方法可以正确记录日志。
     */
    void testLogInfo()
    {
        QString logPath = m_tempDir->filePath("test_info.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 记录日志
        LogManager::instance().info("Test info message");
        LogManager::instance().shutdown();  // 关闭以刷新缓冲区
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 验证日志内容
        QVERIFY(content.contains("INFO"));              // 包含级别标识
        QVERIFY(content.contains("Test info message")); // 包含消息内容
    }

    /**
     * @brief 测试 Debug 级别日志
     * 
     * 验证 debug() 方法可以正确记录日志。
     */
    void testLogDebug()
    {
        QString logPath = m_tempDir->filePath("test_debug.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 记录日志
        LogManager::instance().debug("Test debug message");
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        QVERIFY(content.contains("DEBUG"));
        QVERIFY(content.contains("Test debug message"));
    }

    /**
     * @brief 测试 Warning 级别日志
     * 
     * 验证 warning() 方法可以正确记录日志。
     */
    void testLogWarning()
    {
        QString logPath = m_tempDir->filePath("test_warning.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 记录日志
        LogManager::instance().warning("Test warning message");
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        QVERIFY(content.contains("WARNING"));
        QVERIFY(content.contains("Test warning message"));
    }

    /**
     * @brief 测试 Error 级别日志
     * 
     * 验证 error() 方法可以正确记录日志，
     * 并且同时写入主日志文件和错误日志文件。
     */
    void testLogError()
    {
        QString logPath = m_tempDir->filePath("test_error.log");
        QString errorLogPath = m_tempDir->filePath("error_only.log");
        LogManager::instance().initialize(logPath.toStdString(), errorLogPath.toStdString());
        
        // 记录错误日志
        LogManager::instance().error("Test error message");
        LogManager::instance().shutdown();
        
        // ========== 验证主日志文件 ==========
        QFile mainFile(logPath);
        QVERIFY(mainFile.open(QIODevice::ReadOnly));
        QString mainContent = QString::fromUtf8(mainFile.readAll());
        mainFile.close();
        
        QVERIFY(mainContent.contains("ERROR"));
        QVERIFY(mainContent.contains("Test error message"));
        
        // ========== 验证错误日志文件 ==========
        QFile errorFile(errorLogPath);
        QVERIFY(errorFile.open(QIODevice::ReadOnly));
        QString errorContent = QString::fromUtf8(errorFile.readAll());
        errorFile.close();
        
        QVERIFY(errorContent.contains("ERROR"));
        QVERIFY(errorContent.contains("Test error message"));
    }

    /**
     * @brief 测试指定级别日志
     * 
     * 验证 log() 方法可以记录指定级别的日志。
     */
    void testLogWithLevel()
    {
        QString logPath = m_tempDir->filePath("test_level.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 使用 log() 方法指定级别
        LogManager::instance().log("Custom level message", LogLevel::Warning);
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        QVERIFY(content.contains("WARNING"));
        QVERIFY(content.contains("Custom level message"));
    }

    // ========== 日志宏测试 ==========

    /**
     * @brief 测试日志宏
     * 
     * 验证 LOG_INFO、LOG_DEBUG、LOG_WARNING、LOG_ERROR 宏可以正常工作。
     */
    void testLogMacros()
    {
        QString logPath = m_tempDir->filePath("test_macros.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 使用日志宏
        LOG_INFO("Macro info");
        LOG_DEBUG("Macro debug");
        LOG_WARNING("Macro warning");
        LOG_ERROR("Macro error");
        
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 验证所有宏记录的消息都存在
        QVERIFY(content.contains("Macro info"));
        QVERIFY(content.contains("Macro debug"));
        QVERIFY(content.contains("Macro warning"));
        QVERIFY(content.contains("Macro error"));
    }

    // ========== 回调测试 ==========

    /**
     * @brief 测试日志回调
     * 
     * 验证设置日志回调后，记录日志会触发回调。
     */
    void testLogCallback()
    {
        QString logPath = m_tempDir->filePath("test_callback.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 设置日志回调，收集日志
        std::vector<std::pair<std::string, LogLevel>> receivedLogs;
        LogManager::instance().setLogCallback(
            [&receivedLogs](const std::string& msg, LogLevel level) {
                receivedLogs.push_back({msg, level});
            }
        );
        
        // 记录日志
        LogManager::instance().info("Callback test");
        
        // 验证回调被调用
        QCOMPARE(receivedLogs.size(), size_t(1));
        QVERIFY(receivedLogs[0].first.find("Callback test") != std::string::npos);
        QCOMPARE(receivedLogs[0].second, LogLevel::Info);
    }

    /**
     * @brief 测试多级别回调
     * 
     * 验证不同级别的日志都会触发回调，且级别正确。
     */
    void testLogCallbackMultipleLevels()
    {
        QString logPath = m_tempDir->filePath("test_callback_levels.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 设置日志回调，收集级别
        std::vector<LogLevel> receivedLevels;
        LogManager::instance().setLogCallback(
            [&receivedLevels](const std::string&, LogLevel level) {
                receivedLevels.push_back(level);
            }
        );
        
        // 记录不同级别的日志
        LogManager::instance().debug("Debug");
        LogManager::instance().info("Info");
        LogManager::instance().warning("Warning");
        LogManager::instance().error("Error");
        
        // 验证级别顺序
        QCOMPARE(receivedLevels.size(), size_t(4));
        QCOMPARE(receivedLevels[0], LogLevel::Debug);
        QCOMPARE(receivedLevels[1], LogLevel::Info);
        QCOMPARE(receivedLevels[2], LogLevel::Warning);
        QCOMPARE(receivedLevels[3], LogLevel::Error);
    }

    // ========== Qt 信号测试 ==========

    /**
     * @brief 测试日志信号
     * 
     * 验证记录日志时会发出 logMessageSignal 信号。
     */
    void testLogSignal()
    {
        QString logPath = m_tempDir->filePath("test_signal.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 创建信号监听器
        QSignalSpy spy(&LogManager::instance(), &LogManager::logMessageSignal);
        QVERIFY(spy.isValid());  // 验证信号存在
        
        // 记录日志
        LogManager::instance().info("Signal test");
        
        // 等待信号处理完成
        QVERIFY(spy.wait(1000) || spy.count() > 0);
        QVERIFY(spy.count() >= 1);
        
        // 验证信号参数
        QList<QVariant> arguments = spy.takeFirst();
        QString message = arguments.at(0).toString();
        LogLevel level = arguments.at(1).value<LogLevel>();
        
        QVERIFY(message.contains("Signal test"));
        QCOMPARE(level, LogLevel::Info);
    }

    // ========== 静态方法测试 ==========

    /**
     * @brief 测试消息格式化
     * 
     * 验证 formatMessage() 静态方法的输出格式。
     */
    void testFormatMessage()
    {
        // 格式化消息
        std::string formatted = ILogManager::formatMessage("Test message", LogLevel::Info);
        
        // 验证格式
        QVERIFY(formatted.find("[INFO]") != std::string::npos);      // 包含级别标识
        QVERIFY(formatted.find("Test message") != std::string::npos); // 包含消息内容
        QVERIFY(formatted.find(":") != std::string::npos);           // 包含时间戳分隔符
    }

    /**
     * @brief 测试时间戳格式
     * 
     * 验证 timestamp() 静态方法返回正确格式的时间戳。
     */
    void testTimestamp()
    {
        std::string ts = ILogManager::timestamp();
        
        // 验证格式：HH:MM:SS.mmm（12 个字符）
        QCOMPARE(ts.length(), size_t(12));
        QCOMPARE(ts[2], ':');   // 第 3 个字符是冒号
        QCOMPARE(ts[5], ':');   // 第 6 个字符是冒号
        QCOMPARE(ts[8], '.');   // 第 9 个字符是点号
    }

    /**
     * @brief 测试级别转字符串
     * 
     * 验证 levelToString() 静态方法的转换结果。
     */
    void testLevelToString()
    {
        QCOMPARE(ILogManager::levelToString(LogLevel::Debug), std::string("DEBUG"));
        QCOMPARE(ILogManager::levelToString(LogLevel::Info), std::string("INFO"));
        QCOMPARE(ILogManager::levelToString(LogLevel::Warning), std::string("WARNING"));
        QCOMPARE(ILogManager::levelToString(LogLevel::Error), std::string("ERROR"));
    }

    // ========== 边界情况测试 ==========

    /**
     * @brief 测试空消息
     * 
     * 验证可以记录空消息。
     */
    void testLogEmptyMessage()
    {
        QString logPath = m_tempDir->filePath("test_empty.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 记录空消息
        LogManager::instance().info("");
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 空消息也应该被记录（带时间戳和级别）
        QVERIFY(content.contains("[INFO]"));
    }

    /**
     * @brief 测试超长消息
     * 
     * 验证可以记录超长消息（10000 字符）。
     */
    void testLogLongMessage()
    {
        QString logPath = m_tempDir->filePath("test_long.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 创建 10000 字符的消息
        std::string longMessage(10000, 'X');
        LogManager::instance().info(longMessage);
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 验证完整消息被记录
        QVERIFY(content.contains(QString(10000, 'X')));
    }

    /**
     * @brief 测试特殊字符
     * 
     * 验证可以记录包含特殊字符的消息（中文、日文、韩文、emoji）。
     */
    void testLogSpecialCharacters()
    {
        QString logPath = m_tempDir->filePath("test_special.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 记录包含特殊字符的消息
        LogManager::instance().info("Special: 中文 日本語 한국어 émojis 🎉");
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 验证特殊字符被正确记录
        QVERIFY(content.contains("中文"));
        QVERIFY(content.contains("日本語"));
    }

    /**
     * @brief 测试大量日志条目
     * 
     * 验证可以记录大量日志条目（压力测试）。
     */
    void testMultipleLogEntries()
    {
        QString logPath = m_tempDir->filePath("test_multiple.log");
        LogManager::instance().initialize(logPath.toStdString());
        
        // 记录 100 条日志
        for (int i = 0; i < 100; ++i) {
            LogManager::instance().info("Log entry " + std::to_string(i));
        }
        LogManager::instance().shutdown();
        
        // 读取并验证日志文件内容
        QFile file(logPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString content = QString::fromUtf8(file.readAll());
        file.close();
        
        // 验证首尾条目都存在
        QVERIFY(content.contains("Log entry 0"));
        QVERIFY(content.contains("Log entry 99"));
    }
};

// ============================================================================
// 测试入口
// ============================================================================

/**
 * @brief 测试主函数
 * 
 * QTEST_MAIN 宏生成 main() 函数，运行 TestLogManager 中的所有测试。
 */
QTEST_MAIN(TestLogManager)

// 包含 moc 生成的文件
#include "test_log_manager.moc"
