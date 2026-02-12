#pragma once
#include <QString>
#include <QList>
#include <QPair>
#include <memory>

class IHostsManager {
public:
    virtual ~IHostsManager() = default;
    virtual bool addEntry(const QString& ip, const QString& domain) = 0;
    virtual bool removeEntry(const QString& domain) = 0;
    virtual bool hasEntry(const QString& domain) = 0;
    virtual QList<QPair<QString, QString>> listEntries() = 0;
    virtual bool flush() = 0;
};

using IHostsManagerPtr = std::unique_ptr<IHostsManager>;
