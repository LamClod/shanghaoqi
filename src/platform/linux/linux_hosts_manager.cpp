#include "linux_hosts_manager.h"
#include "core/log_manager.h"

#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>

QString LinuxHostsManager::hostsFilePath() const
{
    return QStringLiteral("/etc/hosts");
}

QStringList LinuxHostsManager::readHostsFile() const
{
    QFile file(hostsFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Cannot open hosts file for reading: %1").arg(hostsFilePath()));
        return QStringList();
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();
    return lines;
}

bool LinuxHostsManager::writeHostsFile(const QStringList& lines) const
{
    QFile file(hostsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        LOG_ERROR(QString("Cannot open hosts file for writing: %1 "
                          "(root privileges may be required)")
                      .arg(hostsFilePath()));
        return false;
    }

    QTextStream out(&file);
    for (int i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i < lines.size() - 1) {
            out << "\n";
        }
    }
    if (!lines.isEmpty() && !lines.last().isEmpty()) {
        out << "\n";
    }
    file.close();
    return true;
}

bool LinuxHostsManager::addEntry(const QString& ip, const QString& domain)
{
    LOG_INFO(QString("Adding hosts entry: %1 -> %2").arg(ip, domain));

    if (hasEntry(domain)) {
        LOG_WARNING(QString("Hosts entry already exists for domain: %1, removing first").arg(domain));
        if (!removeEntry(domain)) {
            LOG_ERROR(QString("Failed to remove existing entry for: %1").arg(domain));
            return false;
        }
    }

    QStringList lines = readHostsFile();
    QString newLine = QString("%1 %2 %3").arg(ip, domain, QLatin1String(MARKER));
    lines.append(newLine);

    if (!writeHostsFile(lines)) {
        LOG_ERROR("Failed to write hosts file when adding entry");
        return false;
    }

    LOG_INFO(QString("Hosts entry added successfully: %1 %2").arg(ip, domain));
    return true;
}

bool LinuxHostsManager::removeEntry(const QString& domain)
{
    LOG_INFO(QString("Removing hosts entry for domain: %1").arg(domain));

    QStringList lines = readHostsFile();
    if (lines.isEmpty()) {
        LOG_WARNING("Hosts file is empty or unreadable");
        return false;
    }

    QStringList filtered;
    bool found = false;

    for (const auto& line : lines) {
        QString trimmed = line.trimmed();

        bool isDomainMatch = false;
        if (trimmed.contains(QLatin1String(MARKER))) {
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            for (const auto& part : parts) {
                if (part.compare(domain, Qt::CaseInsensitive) == 0) {
                    isDomainMatch = true;
                    break;
                }
            }
        }

        if (isDomainMatch) {
            found = true;
            LOG_DEBUG(QString("Removing line: %1").arg(trimmed));
        } else {
            filtered.append(line);
        }
    }

    if (!found) {
        LOG_WARNING(QString("No ShangHaoQi hosts entry found for domain: %1").arg(domain));
        return false;
    }

    if (!writeHostsFile(filtered)) {
        LOG_ERROR("Failed to write hosts file when removing entry");
        return false;
    }

    LOG_INFO(QString("Hosts entry removed successfully for domain: %1").arg(domain));
    return true;
}

bool LinuxHostsManager::hasEntry(const QString& domain)
{
    QStringList lines = readHostsFile();

    for (const auto& line : lines) {
        QString trimmed = line.trimmed();

        if (!trimmed.contains(QLatin1String(MARKER))) {
            continue;
        }

        QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        for (const auto& part : parts) {
            if (part.compare(domain, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    return false;
}

QList<QPair<QString, QString>> LinuxHostsManager::listEntries()
{
    QList<QPair<QString, QString>> entries;
    QStringList lines = readHostsFile();

    for (const auto& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.isEmpty()) {
            continue;
        }
        if (!trimmed.contains(QLatin1String(MARKER))) {
            continue;
        }

        int markerIdx = trimmed.indexOf(QLatin1String(MARKER));
        QString withoutMarker = trimmed.left(markerIdx).trimmed();
        QStringList parts = withoutMarker.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (parts.size() >= 2) {
            entries.append(qMakePair(parts[0], parts[1]));
        }
    }

    LOG_DEBUG(QString("Listed %1 ShangHaoQi hosts entries").arg(entries.size()));
    return entries;
}

bool LinuxHostsManager::flush()
{
    LOG_INFO("Flushing DNS cache");

    // Try systemd-resolved first (modern systemd-based distros)
    QProcess resolvectl;
    resolvectl.start("resolvectl", {"flush-caches"});
    if (resolvectl.waitForStarted(3000) && resolvectl.waitForFinished(10000) && resolvectl.exitCode() == 0) {
        LOG_INFO("DNS cache flushed via resolvectl");
        return true;
    }

    // Try systemd-resolve (older systemd)
    QProcess systemdResolve;
    systemdResolve.start("systemd-resolve", {"--flush-caches"});
    if (systemdResolve.waitForStarted(3000) && systemdResolve.waitForFinished(10000) && systemdResolve.exitCode() == 0) {
        LOG_INFO("DNS cache flushed via systemd-resolve");
        return true;
    }

    // Try nscd (Name Service Cache Daemon)
    QProcess nscd;
    nscd.start("nscd", {"-i", "hosts"});
    if (nscd.waitForStarted(3000) && nscd.waitForFinished(10000) && nscd.exitCode() == 0) {
        LOG_INFO("DNS cache flushed via nscd");
        return true;
    }

    // If none of the above worked, try restarting systemd-resolved service
    QProcess restart;
    restart.start("systemctl", {"restart", "systemd-resolved"});
    if (restart.waitForStarted(3000) && restart.waitForFinished(15000) && restart.exitCode() == 0) {
        LOG_INFO("DNS cache flushed by restarting systemd-resolved");
        return true;
    }

    LOG_WARNING("No supported DNS cache flush method found. "
                "DNS changes may take effect after the cache TTL expires.");
    return true;
}
