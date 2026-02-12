#include "log_manager.h"
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QDebug>

LogManager& LogManager::instance() {
    static LogManager s_instance;
    return s_instance;
}

LogManager::~LogManager()
{
    if (m_logFile.isOpen()) {
        m_logFile.flush();
        m_logFile.close();
    }
}

void LogManager::initialize(const QString& logDir) {
    QDir().mkpath(logDir);
    QString logPath = logDir + "/shanghaoqi.log";
    m_logFile.setFileName(logPath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "LogManager: failed to open log file:" << logPath;
        m_logFile.close();
    }
}

void LogManager::log(Level level, const QString& category, const QString& message) {
    if (level < Debug || level > Error) {
        level = Error;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    static const char* levelNames[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    QString formatted = QString("[%1] [%2] [%3] %4")
        .arg(timestamp, levelNames[level], category, message);

    // file output
    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << formatted << "\n";
        stream.flush();
    }

    // buffer for UI
    QVariantMap entry;
    entry["level"] = static_cast<int>(level);
    entry["timestamp"] = timestamp;
    entry["category"] = category;
    entry["message"] = message;
    m_buffer.append(entry);
    while (m_buffer.size() > m_maxBuffer)
        m_buffer.removeFirst();

    // signal
    emit logEntry(static_cast<int>(level), timestamp, category, message);
}

QVariantList LogManager::recentLogs(int count) const {
    QVariantList result;
    int start = qMax(0, m_buffer.size() - count);
    for (int i = start; i < m_buffer.size(); ++i)
        result.append(m_buffer[i]);
    return result;
}

void LogManager::clearLogs() {
    m_buffer.clear();
}

QString LogManager::formatMessage(Level level, const QString& category, const QString& message) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    static const char* levelNames[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    return QString("[%1] [%2] [%3] %4")
        .arg(timestamp, levelNames[level], category, message);
}
