/**
 * @file macos_hosts_manager.cpp
 * @brief macOS Hosts 文件管理器实现
 * 
 * @copyright Copyright (c) 2026 LAMCLOD. All rights reserved.
 * @author LAMCLOD
 * @version 2.0.0
 */

#include "macos_hosts_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>

// ============================================================================
// 构造函数
// ============================================================================

MacOSHostsManager::MacOSHostsManager(const QString& backupDir, QObject* parent)
    : QObject(parent)
    , m_hostsPath(getDefaultHostsPath())
    , m_backupDir(backupDir)
    , m_logCallback(nullptr)
{
    m_hijackDomains << "api.openai.com" << "api.anthropic.com";
    
    QDir dir(backupDir);
    if (!dir.exists()) dir.mkpath(".");
    
    m_backupPath = QDir(backupDir).filePath("hosts.backup");
}

void MacOSHostsManager::emitLog(const std::string& message, LogLevel level)
{
    if (m_logCallback) m_logCallback(message, level);
    emit logMessage(QString::fromStdString(message), level);
}

void MacOSHostsManager::setLogCallback(LogCallback callback)
{
    m_logCallback = callback;
}

// ============================================================================
// 文件操作
// ============================================================================

bool MacOSHostsManager::readFileContent(const QString& filePath, QString& content)
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

bool MacOSHostsManager::writeFileContent(const QString& filePath, const QString& content)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    
    if (!tempFile.open()) {
        emitLog("无法创建临时文件", LogLevel::Error);
        return false;
    }
    
    tempFile.write(content.toUtf8());
    QString tempPath = tempFile.fileName();
    tempFile.close();
    
    // 使用 osascript 请求管理员权限
    QString script = QString(
        "do shell script \"cp '%1' '%2'\" with administrator privileges"
    ).arg(tempPath, filePath);
    
    QProcess process;
    process.start("osascript", {"-e", script});
    process.waitForFinished(30000);
    
    QFile::remove(tempPath);
    
    if (process.exitCode() != 0) {
        emitLog("写入文件失败", LogLevel::Error);
        return false;
    }
    
    return true;
}

// ============================================================================
// 条目处理
// ============================================================================

QString MacOSHostsManager::generateEntries() const
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

QString MacOSHostsManager::removeEntriesFromContent(const QString& content) const
{
    QString result = content;
    
    QRegularExpression blockRegex(
        R"(\n?# ShangHaoQi Proxy - Added automatically\n.*?# End ShangHaoQi Proxy\n?)",
        QRegularExpression::DotMatchesEverythingOption
    );
    result.remove(blockRegex);
    
    for (const QString& domain : m_hijackDomains) {
        QRegularExpression ipv4Regex(
            QStringLiteral(R"(\n?127\.0\.0\.1\s+%1\s*\n?)").arg(QRegularExpression::escape(domain))
        );
        QRegularExpression ipv6Regex(
            QStringLiteral(R"(\n?::1\s+%1\s*\n?)").arg(QRegularExpression::escape(domain))
        );
        result.remove(ipv4Regex);
        result.remove(ipv6Regex);
    }
    
    return result;
}

// ============================================================================
// 主要操作
// ============================================================================

bool MacOSHostsManager::backup()
{
    emitLog("正在备份 hosts 文件...", LogLevel::Info);
    
    QString content;
    if (!readFileContent(m_hostsPath, content)) return false;
    
    QFile backupFile(m_backupPath);
    if (!backupFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitLog("备份失败", LogLevel::Error);
        return false;
    }
    
    backupFile.write(content.toUtf8());
    backupFile.close();
    
    emitLog("hosts 文件备份成功", LogLevel::Info);
    return true;
}

bool MacOSHostsManager::restore()
{
    emitLog("正在恢复 hosts 文件...", LogLevel::Info);
    
    if (!QFile::exists(m_backupPath)) {
        emitLog("备份文件不存在", LogLevel::Error);
        return false;
    }
    
    QString content;
    if (!readFileContent(m_backupPath, content)) return false;
    
    if (!writeFileContent(m_hostsPath, content)) return false;
    
    emitLog("hosts 文件恢复成功", LogLevel::Info);
    return true;
}

bool MacOSHostsManager::addEntry()
{
    emitLog("正在添加 hosts 条目...", LogLevel::Info);
    
    if (!backup()) return false;
    
    QString content;
    if (!readFileContent(m_hostsPath, content)) return false;
    
    if (hasEntry()) {
        emitLog("hosts 条目已存在，跳过", LogLevel::Info);
        return true;
    }
    
    content += generateEntries();
    
    if (!writeFileContent(m_hostsPath, content)) return false;
    
    emitLog("hosts 条目添加成功", LogLevel::Info);
    return true;
}

bool MacOSHostsManager::removeEntry()
{
    emitLog("正在删除 hosts 条目...", LogLevel::Info);
    
    QString content;
    if (!readFileContent(m_hostsPath, content)) return false;
    
    QString newContent = removeEntriesFromContent(content);
    
    if (newContent == content) {
        emitLog("未找到需要删除的条目", LogLevel::Info);
        return true;
    }
    
    if (!writeFileContent(m_hostsPath, newContent)) return false;
    
    emitLog("hosts 条目删除成功", LogLevel::Info);
    return true;
}

bool MacOSHostsManager::hasEntry() const
{
    QFile file(m_hostsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    for (const QString& domain : m_hijackDomains) {
        QRegularExpression regex(
            QStringLiteral(R"((127\.0\.0\.1|::1)\s+%1)").arg(QRegularExpression::escape(domain))
        );
        if (!regex.match(content).hasMatch()) return false;
    }
    
    return true;
}

bool MacOSHostsManager::openInEditor()
{
    emitLog("正在打开 hosts 文件...", LogLevel::Info);
    
    // 使用 osascript 以管理员权限打开 TextEdit
    QString script = QString(
        "do shell script \"open -e '%1'\" with administrator privileges"
    ).arg(m_hostsPath);
    
    QProcess* process = new QProcess(this);
    process->start("osascript", {"-e", script});
    
    bool started = process->waitForStarted(5000);
    if (started) {
        emitLog("已启动编辑器", LogLevel::Info);
    }
    return started;
}

// ============================================================================
// 路径和配置
// ============================================================================

std::string MacOSHostsManager::hostsFilePath() const { return m_hostsPath.toStdString(); }
std::string MacOSHostsManager::backupFilePath() const { return m_backupPath.toStdString(); }

std::vector<std::string> MacOSHostsManager::hijackDomains() const
{
    std::vector<std::string> result;
    result.reserve(m_hijackDomains.size());
    for (const QString& domain : m_hijackDomains) {
        result.push_back(domain.toStdString());
    }
    return result;
}

void MacOSHostsManager::setHijackDomains(const std::vector<std::string>& domains)
{
    m_hijackDomains.clear();
    for (const std::string& domain : domains) {
        m_hijackDomains.append(QString::fromStdString(domain));
    }
}
