/**
 * @file linux_hosts_manager.cpp
 * @brief Linux Hosts 文件管理器实现
 * 
 * 实现 IHostsManager 接口，管理 Linux 系统的 hosts 文件。
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "linux_hosts_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>

// ============================================================================
// 构造函数
// ============================================================================

LinuxHostsManager::LinuxHostsManager(const QString& backupDir, QObject* parent)
    : QObject(parent)
    , m_hostsPath(getDefaultHostsPath())
    , m_backupDir(backupDir)
    , m_logCallback(nullptr)
{
    m_hijackDomains << "api.openai.com"
                    << "api.anthropic.com";
    
    QDir dir(backupDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_backupPath = QDir(backupDir).filePath("hosts.backup");
}

// ============================================================================
// 日志辅助方法
// ============================================================================

void LinuxHostsManager::emitLog(const std::string& message, LogLevel level)
{
    if (m_logCallback) {
        m_logCallback(message, level);
    }
    emit logMessage(QString::fromStdString(message), level);
}

void LinuxHostsManager::setLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

// ============================================================================
// 文件操作
// ============================================================================

bool LinuxHostsManager::readFileContent(const QString& filePath, QString& content)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emitLog("无法打开文件: " + filePath.toStdString(), LogLevel::Error);
        return false;
    }
    
    content = QString::fromUtf8(file.readAll());
    file.close();
    return true;
}

bool LinuxHostsManager::writeFileContent(const QString& filePath, const QString& content)
{
    // 先写入临时文件
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    
    if (!tempFile.open()) {
        emitLog("无法创建临时文件", LogLevel::Error);
        return false;
    }
    
    tempFile.write(content.toUtf8());
    QString tempPath = tempFile.fileName();
    tempFile.close();
    
    // 使用 sudo 复制到目标位置
    QProcess process;
    process.start("sudo", {"cp", tempPath, filePath});
    process.waitForFinished(30000);
    
    // 清理临时文件
    QFile::remove(tempPath);
    
    if (process.exitCode() != 0) {
        QString error = QString::fromUtf8(process.readAllStandardError());
        emitLog("写入文件失败: " + error.toStdString(), LogLevel::Error);
        return false;
    }
    
    return true;
}

// ============================================================================
// 条目处理
// ============================================================================

QString LinuxHostsManager::generateEntries() const
{
    QString entries;
    
    entries += QStringLiteral("\n# ShangHaoQi Proxy - Added automatically\n");
    
    for (const QString& domain : m_hijackDomains) {
        entries += QStringLiteral("127.0.0.1 %1\n").arg(domain);
        entries += QStringLiteral("::1 %1\n").arg(domain);
    }
    
    entries += QStringLiteral("# End ShangHaoQi Proxy\n");
    
    return entries;
}

QString LinuxHostsManager::removeEntriesFromContent(const QString& content) const
{
    QString result = content;
    
    // 移除标记块
    QRegularExpression blockRegex(
        R"(\n?# ShangHaoQi Proxy - Added automatically\n.*?# End ShangHaoQi Proxy\n?)",
        QRegularExpression::DotMatchesEverythingOption
    );
    result.remove(blockRegex);
    
    // 移除单独的条目
    for (const QString& domain : m_hijackDomains) {
        QRegularExpression ipv4Regex(
            QStringLiteral(R"(\n?127\.0\.0\.1\s+%1\s*\n?)")
                .arg(QRegularExpression::escape(domain))
        );
        
        QRegularExpression ipv6Regex(
            QStringLiteral(R"(\n?::1\s+%1\s*\n?)")
                .arg(QRegularExpression::escape(domain))
        );
        
        result.remove(ipv4Regex);
        result.remove(ipv6Regex);
    }
    
    return result;
}

// ============================================================================
// 主要操作
// ============================================================================

bool LinuxHostsManager::backup()
{
    emitLog("正在备份 hosts 文件到: " + m_backupPath.toStdString(), LogLevel::Info);
    
    QString content;
    if (!readFileContent(m_hostsPath, content)) {
        emitLog("备份失败: 无法读取 hosts 文件", LogLevel::Error);
        return false;
    }
    
    QFile backupFile(m_backupPath);
    if (!backupFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitLog("备份失败: 无法写入备份文件", LogLevel::Error);
        return false;
    }
    
    backupFile.write(content.toUtf8());
    backupFile.close();
    
    emitLog("hosts 文件备份成功", LogLevel::Info);
    return true;
}

bool LinuxHostsManager::restore()
{
    emitLog("正在恢复 hosts 文件...", LogLevel::Info);
    
    QFileInfo backupInfo(m_backupPath);
    if (!backupInfo.exists()) {
        emitLog("恢复失败: 备份文件不存在", LogLevel::Error);
        return false;
    }
    
    QString content;
    if (!readFileContent(m_backupPath, content)) {
        emitLog("恢复失败: 无法读取备份文件", LogLevel::Error);
        return false;
    }
    
    if (!writeFileContent(m_hostsPath, content)) {
        emitLog("恢复失败: 无法写入 hosts 文件", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 文件恢复成功", LogLevel::Info);
    return true;
}

bool LinuxHostsManager::addEntry()
{
    emitLog("正在添加 hosts 条目...", LogLevel::Info);
    
    if (!backup()) {
        return false;
    }
    
    QString content;
    if (!readFileContent(m_hostsPath, content)) {
        return false;
    }
    
    if (hasEntry()) {
        emitLog("hosts 条目已存在，跳过添加", LogLevel::Info);
        return true;
    }
    
    content += generateEntries();
    
    if (!writeFileContent(m_hostsPath, content)) {
        emitLog("添加 hosts 条目失败", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 条目添加成功", LogLevel::Info);
    return true;
}

bool LinuxHostsManager::removeEntry()
{
    emitLog("正在删除 hosts 条目...", LogLevel::Info);
    
    QString content;
    if (!readFileContent(m_hostsPath, content)) {
        return false;
    }
    
    QString newContent = removeEntriesFromContent(content);
    
    if (newContent == content) {
        emitLog("未找到需要删除的 hosts 条目", LogLevel::Info);
        return true;
    }
    
    if (!writeFileContent(m_hostsPath, newContent)) {
        emitLog("删除 hosts 条目失败", LogLevel::Error);
        return false;
    }
    
    emitLog("hosts 条目删除成功", LogLevel::Info);
    return true;
}

bool LinuxHostsManager::hasEntry() const
{
    QFile file(m_hostsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    for (const QString& domain : m_hijackDomains) {
        QRegularExpression regex(
            QStringLiteral(R"((127\.0\.0\.1|::1)\s+%1)")
                .arg(QRegularExpression::escape(domain))
        );
        
        if (!regex.match(content).hasMatch()) {
            return false;
        }
    }
    
    return true;
}

bool LinuxHostsManager::openInEditor()
{
    emitLog("正在打开 hosts 文件...", LogLevel::Info);
    
    QProcess* process = new QProcess(this);
    
    // 尝试使用 xdg-open 或常见编辑器
    QStringList editors = {"xdg-open", "gedit", "kate", "nano", "vim"};
    
    for (const QString& editor : editors) {
        QProcess checkProcess;
        checkProcess.start("which", {editor});
        checkProcess.waitForFinished(1000);
        
        if (checkProcess.exitCode() == 0) {
            // 使用 pkexec 或 sudo 以 root 权限打开
            if (QFile::exists("/usr/bin/pkexec")) {
                process->setProgram("pkexec");
                process->setArguments({editor, m_hostsPath});
            } else {
                process->setProgram("sudo");
                process->setArguments({editor, m_hostsPath});
            }
            
            bool started = process->startDetached();
            if (started) {
                emitLog("已启动编辑器编辑 hosts 文件", LogLevel::Info);
                return true;
            }
        }
    }
    
    emitLog("无法启动编辑器", LogLevel::Error);
    return false;
}

// ============================================================================
// 路径和配置
// ============================================================================

std::string LinuxHostsManager::hostsFilePath() const
{
    return m_hostsPath.toStdString();
}

std::string LinuxHostsManager::backupFilePath() const
{
    return m_backupPath.toStdString();
}

std::vector<std::string> LinuxHostsManager::hijackDomains() const
{
    std::vector<std::string> result;
    result.reserve(m_hijackDomains.size());
    
    for (const QString& domain : m_hijackDomains) {
        result.push_back(domain.toStdString());
    }
    
    return result;
}

void LinuxHostsManager::setHijackDomains(const std::vector<std::string>& domains)
{
    m_hijackDomains.clear();
    
    for (const std::string& domain : domains) {
        m_hijackDomains.append(QString::fromStdString(domain));
    }
}
