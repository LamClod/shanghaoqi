#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>

class ConfigGroupPanel;
class GlobalSettingsPage;
class LogPanel;
class Bootstrap;
class ConfigStore;
class LogManager;

class MainWidget : public QWidget {
    Q_OBJECT

public:
    explicit MainWidget(Bootstrap* bootstrap, ConfigStore* config,
                        LogManager* logMgr, QWidget* parent = nullptr);
    ~MainWidget() override;

    MainWidget(const MainWidget&) = delete;
    MainWidget& operator=(const MainWidget&) = delete;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onStartAllServices();
    void onStopServices();
    void onProxyStatusChanged(bool running);

private:
    void setupUi();
    void connectSignals();
    void updateStatusLabel(bool running);
    void updateButtonStates(bool servicesRunning);
    void cleanup();

    Bootstrap*  m_bootstrap;
    ConfigStore* m_config;
    LogManager*  m_logMgr;

    // Sidebar
    QWidget*      m_sidebar;
    QLabel*       m_appTitle;
    QPushButton*  m_navConfig;
    QPushButton*  m_navSettings;
    QPushButton*  m_navLogs;
    QButtonGroup* m_navGroup;

    // Content
    QStackedWidget*      m_stack;
    ConfigGroupPanel*    m_configPanel;
    GlobalSettingsPage*  m_settingsPage;
    LogPanel*            m_logPanel;

    // Actions
    QPushButton* m_btnStartAll;
    QPushButton* m_btnStop;
    QPushButton* m_btnClearLog;
    QLabel*      m_statusLabel;

    bool m_servicesRunning = false;
};
