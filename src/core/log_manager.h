#pragma once
#include <QObject>
#include <QFile>
#include <QVariantMap>
#include <QList>

class LogManager : public QObject {
    Q_OBJECT

public:
    static LogManager& instance();

    void initialize(const QString& logDir);

    enum Level { Debug, Info, Warning, Error };
    Q_ENUM(Level)

    void log(Level level, const QString& category, const QString& message);
    void debug(const QString& msg)   { log(Debug, "app", msg); }
    void info(const QString& msg)    { log(Info, "app", msg); }
    void warning(const QString& msg) { log(Warning, "app", msg); }
    void error(const QString& msg)   { log(Error, "app", msg); }

    QVariantList recentLogs(int count = 200) const;
    void clearLogs();

    static QString formatMessage(Level level, const QString& category, const QString& message);

signals:
    void logEntry(int level, const QString& timestamp,
                  const QString& category, const QString& message);

private:
    ~LogManager() override;
    LogManager() = default;
    QFile m_logFile;
    QList<QVariantMap> m_buffer;
    int m_maxBuffer = 2000;
};

#define LOG_DEBUG(msg) LogManager::instance().debug(msg)
#define LOG_INFO(msg) LogManager::instance().info(msg)
#define LOG_WARNING(msg) LogManager::instance().warning(msg)
#define LOG_ERROR(msg) LogManager::instance().error(msg)
