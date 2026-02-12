#include "log_panel.h"
#include "theme.h"
#include "core/log_manager.h"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QFont>

LogPanel::LogPanel(LogManager* logMgr, QWidget* parent)
    : QWidget(parent)
    , m_logText(new QTextEdit(this))
    , m_logMgr(logMgr)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_logText->setReadOnly(true);
    m_logText->setLineWrapMode(QTextEdit::NoWrap);

    QFont font(Theme::fontFamily, Theme::fontSize);
    font.setStyleHint(QFont::Monospace);
    m_logText->setFont(font);

    m_logText->setObjectName(QStringLiteral("logText"));

    layout->addWidget(m_logText);

    // connect to log manager signal
    if (m_logMgr) {
        connect(m_logMgr, &LogManager::logEntry,
                this, [this](int level, const QString& timestamp,
                             const QString& category, const QString& message) {
                    QString formatted = QString("[%1] [%2] [%3] %4")
                        .arg(timestamp,
                             level == 0 ? "DEBUG" : level == 1 ? "INFO" : level == 2 ? "WARN" : "ERROR",
                             category, message);
                    appendLog(formatted, level);
                });
    }
}

void LogPanel::appendLog(const QString& message, int level) {
    QString color = levelColor(level);
    QString escaped = message.toHtmlEscaped();
    QString html = QString("<span style=\"color: %1;\">%2</span>").arg(color, escaped);
    m_logText->append(html);
    scrollToBottom();
}

void LogPanel::clear() {
    m_logText->clear();
}

QString LogPanel::levelColor(int level) const {
    switch (level) {
    case 0: return Theme::textDim;   // Debug - gray
    case 1: return Theme::text;      // Info - dark
    case 2: return Theme::warning;   // Warning - orange
    case 3: return Theme::error;     // Error - red
    default: return Theme::text;
    }
}

void LogPanel::scrollToBottom() {
    QScrollBar* bar = m_logText->verticalScrollBar();
    bar->setValue(bar->maximum());
}
