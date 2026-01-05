/**
 * @file macos_hosts_manager.h
 * @brief macOS Hosts 文件管理器
 * 
 * 实现 IHostsManager 接口，提供 macOS 平台的 hosts 文件管理功能。
 * 
 * @details
 * 功能概述：
 * - 管理 macOS 系统的 hosts 文件（/etc/hosts）
 * - 支持 UTF-8 编码
 * - 修改前自动备份，支持一键恢复
 * - 使用 TextEdit 或系统默认编辑器打开
 * 
 * @note 修改 hosts 文件需要 root 权限
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#ifndef MACOS_HOSTS_MANAGER_H
#define MACOS_HOSTS_MANAGER_H

#include "../interfaces/i_hosts_manager.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>

/**
 * @brief macOS Hosts 文件管理器实现
 */
class MacOSHostsManager : public QObject, public IHostsManager {
    Q_OBJECT
    
public:
    static QString getDefaultHostsPath() { return "/etc/hosts"; }
    
    explicit MacOSHostsManager(const QString& backupDir, QObject* parent = nullptr);
    ~MacOSHostsManager() override = default;
    
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
    void logMessage(const QString& message, LogLevel level);
    
private:
    void emitLog(const std::string& message, LogLevel level);
    bool readFileContent(const QString& filePath, QString& content);
    bool writeFileContent(const QString& filePath, const QString& content);
    QString generateEntries() const;
    QString removeEntriesFromContent(const QString& content) const;
    
    QString m_hostsPath;
    QString m_backupPath;
    QString m_backupDir;
    QStringList m_hijackDomains;
    LogCallback m_logCallback;
};

#endif // MACOS_HOSTS_MANAGER_H
