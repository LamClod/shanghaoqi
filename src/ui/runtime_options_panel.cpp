#include "runtime_options_panel.h"
#include "config/config_store.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>

RuntimeOptionsPanel::RuntimeOptionsPanel(ConfigStore* config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
{
    setupUi();
    loadFromConfig();
}

void RuntimeOptionsPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Basic options
    auto* basicGroup = new QGroupBox(QStringLiteral("基本选项"), this);
    auto* basicLayout = new QVBoxLayout(basicGroup);

    m_chkDebugMode = new QCheckBox(QStringLiteral("开启调试模式"), this);
    m_chkDebugMode->setToolTip(QStringLiteral("输出详细的请求/响应日志"));
    basicLayout->addWidget(m_chkDebugMode);

    m_chkDisableSslStrict = new QCheckBox(QStringLiteral("关闭SSL严格模式"), this);
    m_chkDisableSslStrict->setToolTip(QStringLiteral("不验证上游SSL证书"));
    basicLayout->addWidget(m_chkDisableSslStrict);

    mainLayout->addWidget(basicGroup);

    // Network options
    auto* netGroup = new QGroupBox(QStringLiteral("网络选项"), this);
    auto* netLayout = new QVBoxLayout(netGroup);

    m_chkHttp2 = new QCheckBox(QStringLiteral("启用HTTP/2"), this);
    m_chkHttp2->setChecked(true);
    netLayout->addWidget(m_chkHttp2);

    m_chkConnPool = new QCheckBox(QStringLiteral("启用连接池"), this);
    m_chkConnPool->setChecked(true);
    netLayout->addWidget(m_chkConnPool);

    auto* poolLayout = new QHBoxLayout();
    poolLayout->addWidget(new QLabel(QStringLiteral("连接池大小:"), this));
    m_spinPoolSize = new QSpinBox(this);
    m_spinPoolSize->setRange(1, 200);
    m_spinPoolSize->setValue(10);
    poolLayout->addWidget(m_spinPoolSize);
    poolLayout->addStretch();
    netLayout->addLayout(poolLayout);

    mainLayout->addWidget(netGroup);

    // Stream mode
    auto* streamGroup = new QGroupBox(QStringLiteral("流模式"), this);
    auto* streamLayout = new QFormLayout(streamGroup);

    m_comboUpstream = new QComboBox(this);
    m_comboUpstream->addItem(QStringLiteral("跟随客户端"), static_cast<int>(StreamMode::FollowClient));
    m_comboUpstream->addItem(QStringLiteral("强制开启"), static_cast<int>(StreamMode::ForceOn));
    m_comboUpstream->addItem(QStringLiteral("强制关闭"), static_cast<int>(StreamMode::ForceOff));
    streamLayout->addRow(QStringLiteral("上游:"), m_comboUpstream);

    m_comboDownstream = new QComboBox(this);
    m_comboDownstream->addItem(QStringLiteral("跟随客户端"), static_cast<int>(StreamMode::FollowClient));
    m_comboDownstream->addItem(QStringLiteral("强制开启"), static_cast<int>(StreamMode::ForceOn));
    m_comboDownstream->addItem(QStringLiteral("强制关闭"), static_cast<int>(StreamMode::ForceOff));
    streamLayout->addRow(QStringLiteral("下游:"), m_comboDownstream);

    mainLayout->addWidget(streamGroup);

    // Advanced options
    auto* advGroup = new QGroupBox(QStringLiteral("高级选项"), this);
    auto* advLayout = new QFormLayout(advGroup);

    m_spinProxyPort = new QSpinBox(this);
    m_spinProxyPort->setRange(1, 65535);
    m_spinProxyPort->setValue(443);
    advLayout->addRow(QStringLiteral("代理端口:"), m_spinProxyPort);

    m_spinRequestTimeout = new QSpinBox(this);
    m_spinRequestTimeout->setRange(1000, 600000);
    m_spinRequestTimeout->setSuffix(QStringLiteral(" ms"));
    m_spinRequestTimeout->setSingleStep(5000);
    m_spinRequestTimeout->setValue(120000);
    advLayout->addRow(QStringLiteral("请求超时:"), m_spinRequestTimeout);

    m_spinConnectionTimeout = new QSpinBox(this);
    m_spinConnectionTimeout->setRange(500, 300000);
    m_spinConnectionTimeout->setSuffix(QStringLiteral(" ms"));
    m_spinConnectionTimeout->setSingleStep(1000);
    m_spinConnectionTimeout->setValue(30000);
    advLayout->addRow(QStringLiteral("连接超时:"), m_spinConnectionTimeout);

    mainLayout->addWidget(advGroup);

    // Connect signals
    connect(m_chkDebugMode, &QCheckBox::toggled, this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_chkDisableSslStrict, &QCheckBox::toggled, this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_chkHttp2, &QCheckBox::toggled, this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_chkConnPool, &QCheckBox::toggled, this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_spinPoolSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_comboUpstream, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_comboDownstream, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_spinProxyPort, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_spinRequestTimeout, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
    connect(m_spinConnectionTimeout, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RuntimeOptionsPanel::onOptionChanged);
}

void RuntimeOptionsPanel::onOptionChanged() {
    saveToConfig();
    emit optionsChanged();
}

void RuntimeOptionsPanel::loadFromConfig() {
    if (!m_config) return;
    auto opts = m_config->runtimeConfig();

    QSignalBlocker b1(m_chkDebugMode);
    QSignalBlocker b2(m_chkDisableSslStrict);
    QSignalBlocker b3(m_chkHttp2);
    QSignalBlocker b4(m_chkConnPool);
    QSignalBlocker b5(m_spinPoolSize);
    QSignalBlocker b6(m_comboUpstream);
    QSignalBlocker b7(m_comboDownstream);
    QSignalBlocker b8(m_spinProxyPort);
    QSignalBlocker b9(m_spinRequestTimeout);
    QSignalBlocker b10(m_spinConnectionTimeout);

    m_chkDebugMode->setChecked(opts.debugMode);
    m_chkDisableSslStrict->setChecked(opts.disableSslStrict);
    m_chkHttp2->setChecked(opts.enableHttp2);
    m_chkConnPool->setChecked(opts.enableConnectionPool);
    m_spinPoolSize->setValue(opts.connectionPoolSize);

    int upIdx = m_comboUpstream->findData(static_cast<int>(opts.upstreamStreamMode));
    if (upIdx >= 0) m_comboUpstream->setCurrentIndex(upIdx);

    int downIdx = m_comboDownstream->findData(static_cast<int>(opts.downstreamStreamMode));
    if (downIdx >= 0) m_comboDownstream->setCurrentIndex(downIdx);

    m_spinProxyPort->setValue(opts.proxyPort);
    m_spinRequestTimeout->setValue(opts.requestTimeout);
    m_spinConnectionTimeout->setValue(opts.connectionTimeout);
}

void RuntimeOptionsPanel::saveToConfig() {
    if (!m_config) return;
    QVariantMap opts;
    opts["debug_mode"] = m_chkDebugMode->isChecked();
    opts["disable_ssl_strict"] = m_chkDisableSslStrict->isChecked();
    opts["enable_http2"] = m_chkHttp2->isChecked();
    opts["enable_connection_pool"] = m_chkConnPool->isChecked();
    opts["connection_pool_size"] = m_spinPoolSize->value();
    opts["upstream_stream_mode"] = m_comboUpstream->currentData().toInt();
    opts["downstream_stream_mode"] = m_comboDownstream->currentData().toInt();
    opts["proxy_port"] = m_spinProxyPort->value();
    opts["request_timeout"] = m_spinRequestTimeout->value();
    opts["connection_timeout"] = m_spinConnectionTimeout->value();
    m_config->setRuntimeOptions(opts);
}
