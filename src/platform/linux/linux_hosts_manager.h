/**
 * @file linux_hosts_manager.h
 * @brief Linux Hosts 文件管理器
 * 
 * 实现 IHostsManager 接口，提供 Linux 平台的 hosts 文件管理功能。
 * 
 * @details
 * 功能概述：
 * - 管理 Linux 系统的 hosts 文件（/etc/hosts）
 * - 支持 UTF-8 编码
 * - 修改前自动备份，支持一键恢复
 * - 使用系统默认编辑器打开
 * 
 * 条目格式：
 * @code
 * # ShangHaoQi Proxy - Added automatically
 * 127.0.0.1 api.openai.com
 * ::1 api.openai.com
 * # End ShangHaoQi Proxy
 * @endcode
 * 
 * @note 修改 hosts 文件需要 root 权限
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef LINUX_HOSTS_MANAGER_H
#define LINUX_HOSTS_MANAGER_H

#include "../interfaces/i_hosts_manager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>

/**
 * @brief Linux Hosts 文件管理器实现
 * 
 * 实现 IHostsManager 接口，同时继承 QObject 以支持 Qt 信号槽。
 * 
 * @note 修改 hosts 文件需要 root 权限
 */
class LinuxHostsManager : public QObject, public IHostsManager {
    Q_OBJECT
    
public:
    /**
     * @brief 获取 Linux hosts 文件默认路径
     * @return hosts 文件的完整路径
     */
    static QString getDefaultHostsPath() {
        return "/etc/hosts";
    }
    
    /**
     * @brief 构造函数
     * @param backupDir 备份文件存储目录
     * @param parent Qt 父对象
     */
    explicit LinuxHostsManager(const QString& backupDir, QObject* parent = nullptr);
    ~LinuxHostsManager() override = default;
    
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
     */
    void logMessage(const QString& message, LogLevel level);
    
private:
    // ========== 日志辅助 ==========
    
    void emitLog(const std::string& message, LogLevel level);
    
    // ========== 文件操作 ==========
    
    /**
     * @brief 读取文件内容
     */
    bool readFileContent(const QString& filePath, QString& content);
    
    /**
     * @brief 写入文件内容（需要 root 权限）
     */
    bool writeFileContent(const QString& filePath, const QString& content);
    
    // ========== 条目处理 ==========
    
    /**
     * @brief 生成要添加的 hosts 条目
     */
    QString generateEntries() const;
    
    /**
     * @brief 从内容中移除相关条目
     */
    QString removeEntriesFromContent(const QString& content) const;
    
    // ========== 成员变量 ==========
    
    QString m_hostsPath;            ///< hosts 文件路径
    QString m_backupPath;           ///< 备份文件路径
    QString m_backupDir;            ///< 备份目录
    QStringList m_hijackDomains;    ///< 要劫持的域名列表
    LogCallback m_logCallback;      ///< 日志回调
};

#endif // LINUX_HOSTS_MANAGER_H
