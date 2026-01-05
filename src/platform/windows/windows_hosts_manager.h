/**
 * @file windows_hosts_manager.h
 * @brief Windows Hosts 文件管理器
 * 
 * 实现 IHostsManager 接口，提供 Windows 平台的 hosts 文件管理功能。
 * 
 * @details
 * 功能概述：
 * - 管理 Windows 系统的 hosts 文件（C:\Windows\System32\drivers\etc\hosts）
 * - 支持多种编码格式（UTF-8、GBK、UTF-16LE、UTF-16BE）
 * - 自动检测文件编码，保持原有编码格式
 * - 修改前自动备份，支持一键恢复
 * - 使用记事本打开编辑
 * 
 * 编码检测逻辑：
 * 1. 检查 BOM（字节顺序标记）
 * 2. 验证 UTF-8 多字节序列
 * 3. 如有高字节但非有效 UTF-8，假设为 GBK
 * 4. 纯 ASCII 默认使用 UTF-8
 * 
 * 条目格式：
 * @code
 * # ShangHaoQi Proxy - Added automatically
 * 127.0.0.1 api.openai.com
 * ::1 api.openai.com
 * # End ShangHaoQi Proxy
 * @endcode
 * 
 * @note 修改 hosts 文件需要管理员权限
 * @warning 不当修改 hosts 文件可能导致网络问题
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef WINDOWS_HOSTS_MANAGER_H
#define WINDOWS_HOSTS_MANAGER_H

#include "../interfaces/i_hosts_manager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>

/**
 * @brief Windows Hosts 文件管理器实现
 * 
 * 实现 IHostsManager 接口，同时继承 QObject 以支持 Qt 信号槽。
 * 
 * @details
 * 特点：
 * - 支持多种编码格式（UTF-8、GBK、UTF-16）
 * - 自动检测文件编码
 * - 修改前自动备份
 * - 使用记事本打开编辑
 * 
 * 内部实现：
 * - 使用 QStringList 存储域名列表（便于 Qt 操作）
 * - 接口方法返回 std::vector<std::string>（符合接口规范）
 * - 同时支持回调和 Qt 信号两种日志机制
 * 
 * @note 修改 hosts 文件需要管理员权限
 */
class WindowsHostsManager : public QObject, public IHostsManager {
    Q_OBJECT
    
public:
    /**
     * @brief 获取 Windows hosts 文件默认路径
     * 
     * 使用环境变量 SystemRoot 获取 Windows 目录，避免硬编码。
     * 
     * @return hosts 文件的完整路径
     */
    static QString getDefaultHostsPath() {
        QString systemRoot = qEnvironmentVariable("SystemRoot", "C:\\Windows");
        return QDir(systemRoot).filePath("System32/drivers/etc/hosts");
    }
    
    /**
     * @brief 构造函数
     * @param backupDir 备份文件存储目录
     * @param parent Qt 父对象
     */
    explicit WindowsHostsManager(const QString& backupDir, QObject* parent = nullptr);
    ~WindowsHostsManager() override = default;
    
    // ========== IHostsManager 接口实现 ==========
    
    bool addEntry() override;
    bool removeEntry() override;
    [[nodiscard]] bool hasEntry() const override;
    bool backup() override;
    bool restore() override;
    bool openInEditor() override;
    [[nodiscard]] std::string hostsFilePath() const override;
    [[nodiscard]] std::string backupFilePath() const override;
    [[nodiscard]] std::vector<std::string> hijackDomains() const override;
    void setHijackDomains(const std::vector<std::string>& domains) override;
    void setLogCallback(LogCallback callback) override;

signals:
    /**
     * @brief Qt 信号：日志消息
     * 
     * 用于与 Qt UI 组件集成。
     */
    void logMessage(const QString& message, LogLevel level);
    
private:
    // ========== 日志辅助 ==========
    
    /**
     * @brief 发送日志消息
     * 
     * 同时调用纯 C++ 回调和发送 Qt 信号。
     */
    void emitLog(const std::string& message, LogLevel level);
    
    // ========== 编码处理 ==========
    
    /**
     * @brief 检测文件编码
     * @param filePath 文件路径
     * @return 编码名称（UTF-8、GBK、UTF-16LE、UTF-16BE）
     */
    QString detectEncoding(const QString& filePath);
    
    /**
     * @brief 读取文件内容（自动检测编码）
     * @param filePath 文件路径
     * @param content 输出内容
     * @return 是否成功
     */
    bool readFileContent(const QString& filePath, QString& content);
    
    /**
     * @brief 以指定编码写入文件
     * @param filePath 文件路径
     * @param content 内容
     * @param encoding 编码名称
     * @return 是否成功
     */
    bool writeWithEncoding(const QString& filePath, const QString& content,
                           const QString& encoding);
    
    // ========== 条目处理 ==========
    
    /**
     * @brief 生成要添加的 hosts 条目
     * @return 条目文本
     */
    QString generateEntries() const;
    
    /**
     * @brief 从内容中移除相关条目
     * @param content 原始内容
     * @return 移除后的内容
     */
    QString removeEntriesFromContent(const QString& content) const;
    
    // ========== 成员变量 ==========
    
    QString m_hostsPath;            ///< hosts 文件路径
    QString m_backupPath;           ///< 备份文件路径
    QString m_backupDir;            ///< 备份目录
    QString m_detectedEncoding;     ///< 检测到的编码
    QStringList m_hijackDomains;    ///< 要劫持的域名列表（内部使用 Qt 类型）
    LogCallback m_logCallback;      ///< 日志回调
};

#endif // WINDOWS_HOSTS_MANAGER_H
