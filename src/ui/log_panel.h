#pragma once
#include <QWidget>
#include <QTextEdit>

class LogManager;

class LogPanel : public QWidget {
    Q_OBJECT

public:
    explicit LogPanel(LogManager* logMgr, QWidget* parent = nullptr);
    ~LogPanel() override = default;

public slots:
    void appendLog(const QString& message, int level);
    void clear();

private:
    QString levelColor(int level) const;
    void scrollToBottom();

    QTextEdit* m_logText;
    LogManager* m_logMgr;
};
