#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>

class ConfigStore;
class RuntimeOptionsPanel;

class GlobalSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit GlobalSettingsPage(ConfigStore* config, QWidget* parent = nullptr);

    void refreshGroupSelector();
    RuntimeOptionsPanel* runtimePanel() const { return m_runtimePanel; }

signals:
    void logMessage(const QString& msg, int level);

private:
    void setupUi();
    void connectSignals();

    ConfigStore* m_config;
    RuntimeOptionsPanel* m_runtimePanel;
    QComboBox*   m_groupSelector;
    QLineEdit*   m_globalAuthKeyEdit;
};
