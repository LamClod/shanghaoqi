#include "global_settings_page.h"
#include "runtime_options_panel.h"
#include "config/config_store.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QSignalBlocker>

GlobalSettingsPage::GlobalSettingsPage(ConfigStore* config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
{
    setupUi();
    connectSignals();
}

void GlobalSettingsPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(16);

    // ---- 全局配置 ----
    auto* globalGroup = new QGroupBox(QStringLiteral("全局配置"), scrollContent);
    auto* globalLayout = new QFormLayout(globalGroup);

    m_groupSelector = new QComboBox(scrollContent);
    if (m_config) {
        const auto& groups = m_config->groups();
        for (int i = 0; i < groups.size(); ++i)
            m_groupSelector->addItem(groups[i].name, i);
        m_groupSelector->setCurrentIndex(m_config->currentGroupIndex());
    }
    globalLayout->addRow(QStringLiteral("当前配置组:"), m_groupSelector);

    m_globalAuthKeyEdit = new QLineEdit(scrollContent);
    m_globalAuthKeyEdit->setEchoMode(QLineEdit::Password);
    if (m_config) m_globalAuthKeyEdit->setText(m_config->authKey());
    globalLayout->addRow(QStringLiteral("本地鉴权Key:"), m_globalAuthKeyEdit);

    auto* noteLabel = new QLabel(
        QStringLiteral("劫持域名自动从配置组的\"供应商\"字段推导，无需手动配置。"),
        scrollContent);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet(QStringLiteral("QLabel { color: #808080; font-size: 11px; }"));
    globalLayout->addRow(noteLabel);

    contentLayout->addWidget(globalGroup);

    // ---- 运行时选项 ----
    m_runtimePanel = new RuntimeOptionsPanel(m_config, scrollContent);
    contentLayout->addWidget(m_runtimePanel);

    contentLayout->addStretch();

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
}

void GlobalSettingsPage::connectSignals() {
    if (!m_config) return;

    connect(m_groupSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (m_config && index >= 0)
                    m_config->setCurrentGroupIndex(index);
            });

    connect(m_globalAuthKeyEdit, &QLineEdit::editingFinished,
            this, [this]() {
                if (m_config)
                    m_config->setAuthKey(m_globalAuthKeyEdit->text().trimmed());
            });
}

void GlobalSettingsPage::refreshGroupSelector() {
    if (!m_config) return;

    QSignalBlocker blocker(m_groupSelector);
    int prevIndex = m_groupSelector->currentIndex();
    m_groupSelector->clear();

    const auto& groups = m_config->groups();
    for (int i = 0; i < groups.size(); ++i)
        m_groupSelector->addItem(groups[i].name, i);

    if (prevIndex >= 0 && prevIndex < m_groupSelector->count())
        m_groupSelector->setCurrentIndex(prevIndex);
    else if (m_groupSelector->count() > 0)
        m_groupSelector->setCurrentIndex(0);
}
