#include "main_widget.h"
#include "config_group_panel.h"
#include "global_settings_page.h"
#include "runtime_options_panel.h"
#include "log_panel.h"
#include "core/bootstrap.h"
#include "config/config_store.h"
#include "core/log_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QFrame>

MainWidget::MainWidget(Bootstrap* bootstrap, ConfigStore* config,
                       LogManager* logMgr, QWidget* parent)
    : QWidget(parent)
    , m_bootstrap(bootstrap)
    , m_config(config)
    , m_logMgr(logMgr)
    , m_servicesRunning(false)
{
    setWindowTitle(QStringLiteral("上号器"));
    resize(900, 600);
    setupUi();
    connectSignals();
    LOG_INFO(QStringLiteral("主窗口初始化完成"));
}

MainWidget::~MainWidget() {
    cleanup();
}

void MainWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ======== Sidebar ========
    m_sidebar = new QWidget(this);
    m_sidebar->setFixedWidth(160);

    auto* sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(8, 8, 8, 8);
    sidebarLayout->setSpacing(4);

    // App title
    m_appTitle = new QLabel(QStringLiteral("上号器"), m_sidebar);
    m_appTitle->setAlignment(Qt::AlignCenter);
    m_appTitle->setFixedHeight(48);
    QFont titleFont = m_appTitle->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_appTitle->setFont(titleFont);
    sidebarLayout->addWidget(m_appTitle);

    // Separator
    auto* sep1 = new QFrame(m_sidebar);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(sep1);

    // Navigation buttons
    m_navConfig = new QPushButton(QStringLiteral("配置组"), m_sidebar);
    m_navSettings = new QPushButton(QStringLiteral("全局设置"), m_sidebar);
    m_navLogs = new QPushButton(QStringLiteral("日志"), m_sidebar);

    for (auto* btn : {m_navConfig, m_navSettings, m_navLogs}) {
        btn->setCheckable(true);
        btn->setFlat(true);
    }
    m_navConfig->setChecked(true);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    m_navGroup->addButton(m_navConfig, 0);
    m_navGroup->addButton(m_navSettings, 1);
    m_navGroup->addButton(m_navLogs, 2);

    sidebarLayout->addWidget(m_navConfig);
    sidebarLayout->addWidget(m_navSettings);
    sidebarLayout->addWidget(m_navLogs);

    sidebarLayout->addStretch();

    // Separator
    auto* sep2 = new QFrame(m_sidebar);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(sep2);

    // Status
    m_statusLabel = new QLabel(QStringLiteral("已停止"), m_sidebar);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    sidebarLayout->addWidget(m_statusLabel);

    // Action buttons
    m_btnStartAll = new QPushButton(QStringLiteral("启动服务"), m_sidebar);
    sidebarLayout->addWidget(m_btnStartAll);

    m_btnStop = new QPushButton(QStringLiteral("停止服务"), m_sidebar);
    m_btnStop->setEnabled(false);
    sidebarLayout->addWidget(m_btnStop);

    // Vertical separator between sidebar and content
    auto* vline = new QFrame(this);
    vline->setFrameShape(QFrame::VLine);
    vline->setFrameShadow(QFrame::Sunken);

    mainLayout->addWidget(m_sidebar);
    mainLayout->addWidget(vline);

    // ======== Content Area ========
    m_stack = new QStackedWidget(this);

    // Page 0: Config Groups
    m_configPanel = new ConfigGroupPanel(m_config, m_bootstrap, this);
    m_stack->addWidget(m_configPanel);

    // Page 1: Global Settings
    m_settingsPage = new GlobalSettingsPage(m_config, this);
    m_stack->addWidget(m_settingsPage);

    // Page 2: Logs
    auto* logPage = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logPage);
    logLayout->setContentsMargins(12, 12, 12, 12);
    logLayout->setSpacing(8);

    m_logPanel = new LogPanel(m_logMgr, logPage);
    logLayout->addWidget(m_logPanel, 1);

    auto* logBtnLayout = new QHBoxLayout();
    logBtnLayout->addStretch();
    m_btnClearLog = new QPushButton(QStringLiteral("清除日志"), logPage);
    logBtnLayout->addWidget(m_btnClearLog);
    logLayout->addLayout(logBtnLayout);

    m_stack->addWidget(logPage);

    m_stack->setCurrentIndex(0);
    mainLayout->addWidget(m_stack, 1);

    updateStatusLabel(false);
}

void MainWidget::connectSignals() {
    // Navigation
    connect(m_navGroup, &QButtonGroup::idClicked,
            m_stack, &QStackedWidget::setCurrentIndex);

    // Service controls
    connect(m_btnStartAll, &QPushButton::clicked,
            this, &MainWidget::onStartAllServices);
    connect(m_btnStop, &QPushButton::clicked,
            this, &MainWidget::onStopServices);

    if (m_bootstrap) {
        connect(m_bootstrap, &Bootstrap::proxyStatusChanged,
                this, &MainWidget::onProxyStatusChanged);
    }

    // Config panel log -> log panel
    if (m_configPanel && m_logPanel) {
        connect(m_configPanel, &ConfigGroupPanel::logMessage,
                m_logPanel, &LogPanel::appendLog);
    }

    // Config groups changed -> refresh group selector
    if (m_configPanel && m_settingsPage) {
        connect(m_configPanel, &ConfigGroupPanel::configChanged,
                m_settingsPage, &GlobalSettingsPage::refreshGroupSelector);
    }

    // Settings page log -> log panel
    if (m_settingsPage && m_logPanel) {
        connect(m_settingsPage, &GlobalSettingsPage::logMessage,
                m_logPanel, &LogPanel::appendLog);
    }

    // Clear log
    if (m_btnClearLog && m_logPanel) {
        connect(m_btnClearLog, &QPushButton::clicked,
                m_logPanel, &LogPanel::clear);
    }
}

void MainWidget::onStartAllServices() {
    m_btnStartAll->setEnabled(false);
    if (m_bootstrap) {
        // Switch to log page
        m_stack->setCurrentIndex(2);
        m_navLogs->setChecked(true);
        m_bootstrap->startAll();
    }
}

void MainWidget::onStopServices() {
    if (m_bootstrap)
        m_bootstrap->stopAll();
}

void MainWidget::onProxyStatusChanged(bool running) {
    m_servicesRunning = running;
    updateStatusLabel(running);
    updateButtonStates(running);
}

void MainWidget::updateStatusLabel(bool running) {
    if (!m_statusLabel) return;
    if (running) {
        m_statusLabel->setText(QStringLiteral("● 运行中"));
        QPalette pal = m_statusLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(0, 128, 0));
        m_statusLabel->setPalette(pal);
    } else {
        m_statusLabel->setText(QStringLiteral("○ 已停止"));
        QPalette pal = m_statusLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(128, 128, 128));
        m_statusLabel->setPalette(pal);
    }
}

void MainWidget::updateButtonStates(bool servicesRunning) {
    m_btnStartAll->setEnabled(!servicesRunning);
    m_btnStop->setEnabled(servicesRunning);
}

void MainWidget::closeEvent(QCloseEvent* event) {
    if (m_servicesRunning) {
        auto reply = QMessageBox::question(this,
            QStringLiteral("确认退出"),
            QStringLiteral("代理服务正在运行，确定要退出吗？\n"
                           "退出将自动停止服务并恢复 hosts 文件。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    cleanup();
    event->accept();
}

void MainWidget::cleanup() {
    if (m_bootstrap && m_servicesRunning) {
        m_bootstrap->stopAll();
        m_servicesRunning = false;
    }
}
