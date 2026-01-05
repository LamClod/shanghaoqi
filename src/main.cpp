/**
 * @file main.cpp
 * @brief 应用程序入口点
 * 
 * 本文件是应用程序的主入口，负责初始化整个应用程序环境并启动主界面。
 * 
 * @details
 * 应用程序启动流程：
 * 
 * 1. 创建 QApplication 实例
 *    - 设置应用程序名称、版本号和组织信息
 *    - 初始化 Qt 事件循环系统
 * 
 * 2. 检查管理员权限
 *    - 本应用需要修改系统 hosts 文件和安装根证书
 *    - 这些操作在 Windows 上需要管理员权限
 *    - 如果权限不足，将触发 UAC 对话框请求提权
 * 
 * 3. 权限提升处理
 *    - 如果用户同意提权，以管理员身份重新启动应用
 *    - 如果用户拒绝，显示错误提示并退出
 * 
 * 4. 初始化日志系统
 *    - 在用户数据目录创建日志文件
 *    - 配置主日志文件和错误日志文件
 *    - 记录应用启动信息
 * 
 * 5. 创建并显示主窗口
 *    - 实例化 MainWidget 主界面
 *    - 进入 Qt 事件循环
 *    - 等待用户交互
 * 
 * 6. 应用退出
 *    - 记录退出日志
 *    - 关闭日志系统
 *    - 清理资源
 * 
 * @note 本应用必须以管理员权限运行才能正常工作
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

#include "ui/main_widget.h"
#include "core/qt/log_manager.h"
#include "platform/platform_factory.h"

/**
 * @brief 检查并处理管理员权限
 * 
 * 本函数负责检测当前进程是否具有管理员权限，如果没有则尝试提权重启。
 * 
 * @details
 * 权限检查流程：
 * 
 * 1. 通过平台工厂创建权限管理器实例
 * 2. 调用 isRunningAsAdmin() 检查当前权限状态
 * 3. 如果已有管理员权限，直接返回 true 继续运行
 * 4. 如果没有管理员权限：
 *    - 获取当前可执行文件路径
 *    - 调用 restartAsAdmin() 触发 UAC 提权对话框
 *    - 如果提权成功，返回 false 让当前进程退出
 *    - 如果提权失败（用户取消或其他原因），显示错误对话框
 * 
 * @return true  当前进程具有管理员权限，可以继续运行
 * @return false 已启动管理员进程或用户取消提权，当前进程应退出
 * 
 * @exception std::exception 平台初始化失败时抛出异常
 * 
 * @note 在 Windows 上，此函数会触发 UAC（用户账户控制）对话框
 * @note 如果用户取消 UAC 对话框，应用将无法正常运行
 * 
 * @see IPrivilegeManager::isRunningAsAdmin()
 * @see IPrivilegeManager::restartAsAdmin()
 */
bool checkAndElevatePrivileges()
{
    try {
        auto factory = PlatformFactory::create();
        auto privilegeManager = factory->createPrivilegeManager();
        
        // 检查是否已有管理员权限
        if (privilegeManager->isRunningAsAdmin()) {
            return true;
        }
        
        // 尝试以管理员权限重启
        std::string exePath = QCoreApplication::applicationFilePath().toStdString();
        if (privilegeManager->restartAsAdmin(exePath)) {
            // 成功启动管理员进程，当前进程应退出
            return false;
        }
        
        // 无法提权，显示错误
        QMessageBox::critical(
            nullptr,
            QStringLiteral("权限不足"),
            QStringLiteral("此应用程序需要管理员权限才能运行。\n"
                           "请以管理员身份运行程序，或在 UAC 对话框中点击是。")
        );
        return false;
        
    } catch (const std::exception& e) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("错误"),
            QStringLiteral("平台初始化失败: %1").arg(e.what())
        );
        return false;
    }
}

/**
 * @brief 初始化日志系统
 * 
 * 在用户数据目录创建并配置日志文件，为应用程序提供日志记录能力。
 * 
 * @details
 * 初始化步骤：
 * 
 * 1. 获取用户数据目录路径
 *    - 使用 QStandardPaths::AppDataLocation 获取平台特定的数据目录
 *    - Windows 上通常为 %APPDATA%/应用名称
 * 
 * 2. 确保目录存在
 *    - 如果目录不存在，自动创建
 * 
 * 3. 配置日志文件
 *    - 主日志文件：app.log，记录所有级别的日志
 *    - 错误日志文件：error.log，仅记录错误级别的日志
 * 
 * 4. 初始化 LogManager 单例
 *    - 调用 initialize() 方法完成初始化
 *    - 日志系统开始工作
 * 
 * @return QString 日志文件所在的目录路径
 * 
 * @note 日志文件采用追加模式，不会覆盖之前的日志
 * @note 日志格式为：[时间戳] [级别] 消息内容
 * 
 * @see LogManager::initialize()
 */
QString initializeLogging()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString logFilePath = dataDir + "/app.log";
    QString errorLogFilePath = dataDir + "/error.log";
    
    LogManager::instance().initialize(logFilePath.toStdString(), errorLogFilePath.toStdString());
    
    return dataDir;
}

/**
 * @brief 应用程序主函数入口点
 * 
 * C++ 程序的标准入口函数，负责启动整个应用程序。
 * 
 * @details
 * 执行流程：
 * 
 * 1. 创建 QApplication 实例并配置应用信息
 * 2. 检查管理员权限，必要时触发提权
 * 3. 初始化日志系统
 * 4. 记录启动信息（版本、平台、数据目录）
 * 5. 创建并显示主窗口
 * 6. 进入事件循环等待用户操作
 * 7. 事件循环结束后记录退出日志
 * 8. 关闭日志系统并返回退出码
 * 
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * 
 * @return int 应用程序退出码
 * @retval 0 正常退出或已启动管理员进程
 * @retval 非0 异常退出
 * 
 * @note 如果权限检查失败，函数会提前返回 0
 */
int main(int argc, char* argv[])
{
    // 创建应用程序实例
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("上号器"));
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("ShangHaoQi");
    
    // 检查管理员权限
    if (!checkAndElevatePrivileges()) {
        return 0;  // 已启动管理员进程或用户取消
    }
    
    // 初始化日志系统
    QString dataDir = initializeLogging();
    
    LOG_INFO(QStringLiteral("========== 应用程序启动 =========="));
    LOG_INFO(QStringLiteral("版本: %1").arg(app.applicationVersion()));
    LOG_INFO(QStringLiteral("平台: %1").arg(QString::fromStdString(PlatformFactory::currentPlatform())));
    LOG_INFO(QStringLiteral("数据目录: %1").arg(dataDir));
    
    // 创建并显示主窗口
    MainWidget mainWidget;
    mainWidget.show();
    
    // 进入事件循环
    int result = app.exec();
    
    LOG_INFO(QStringLiteral("========== 应用程序退出 =========="));
    LogManager::instance().shutdown();
    
    return result;
}
