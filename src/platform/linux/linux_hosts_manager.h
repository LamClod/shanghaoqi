#pragma once
#include "platform/interfaces/i_hosts_manager.h"

class LinuxHostsManager : public IHostsManager {
public:
    bool addEntry(const QString& ip, const QString& domain) override;
    bool removeEntry(const QString& domain) override;
    bool hasEntry(const QString& domain) override;
    QList<QPair<QString, QString>> listEntries() override;
    bool flush() override;

private:
    QString hostsFilePath() const;
    QStringList readHostsFile() const;
    bool writeHostsFile(const QStringList& lines) const;

    static constexpr const char* MARKER = "# ShangHaoQi";
};
