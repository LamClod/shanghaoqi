#include "config_group_panel.h"
#include "theme.h"
#include "test_result_dialog.h"
#include "config/config_store.h"
#include "config/model_list_utils.h"
#include "config/provider_routing.h"
#include "core/bootstrap.h"
#include "core/log_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QGroupBox>
#include <QTabWidget>
#include <QSpinBox>
#include <QListWidget>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <functional>
#include <memory>

namespace {
using provider_routing::ModelListProvider;

struct AdapterEntry { QString displayName; QString id; };

const QList<AdapterEntry> INBOUND_ADAPTERS = {
    {QStringLiteral("OpenAI"),           QStringLiteral("openai")},
    {QStringLiteral("OpenAI Responses"), QStringLiteral("openai.responses")},
    {QStringLiteral("Anthropic"),        QStringLiteral("anthropic")},
    {QStringLiteral("Gemini"),           QStringLiteral("gemini")},
    {QStringLiteral("AI SDK"),           QStringLiteral("aisdk")},
    {QStringLiteral("Jina"),             QStringLiteral("jina")},
    {QStringLiteral("Codex"),            QStringLiteral("codex")},
    {QStringLiteral("Claude Code"),      QStringLiteral("claudecode")},
    {QStringLiteral("Antigravity"),      QStringLiteral("antigravity")},
};

struct TestRequest {
    QNetworkRequest request;
    QByteArray body;
};

TestRequest buildTestRequest(const ConfigGroup& config) {
    TestRequest result;

    // Determine effective adapter for test format
    QString adapter = config.outboundAdapter;
    if (adapter.isEmpty())
        adapter = config.provider; // fallback to inbound adapter

    const bool isAnthropic = (adapter == QStringLiteral("anthropic")
                           || adapter == QStringLiteral("claudecode"));
    const bool isGemini    = (adapter == QStringLiteral("gemini"));

    QString middleRoute = config.middleRoute;
    if (middleRoute.isEmpty())
        middleRoute = QStringLiteral("/v1");

    if (isGemini) {
        QUrl url(config.baseUrl + middleRoute
                 + QStringLiteral("/models/%1:generateContent").arg(config.modelId));
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("key"), config.apiKey);
        url.setQuery(query);
        result.request.setUrl(url);
        result.request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject body;
        QJsonArray contents;
        QJsonObject content;
        QJsonArray parts;
        QJsonObject part;
        part["text"] = "hi";
        parts.append(part);
        content["parts"] = parts;
        contents.append(content);
        body["contents"] = contents;
        QJsonObject genConfig;
        genConfig["maxOutputTokens"] = 5;
        body["generationConfig"] = genConfig;
        result.body = QJsonDocument(body).toJson();
    } else if (isAnthropic) {
        QString url = config.baseUrl + middleRoute + QStringLiteral("/messages");
        result.request.setUrl(QUrl(url));
        result.request.setRawHeader("x-api-key", config.apiKey.toUtf8());
        result.request.setRawHeader("anthropic-version", "2023-06-01");
        result.request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject body;
        body["model"] = config.modelId;
        QJsonArray msgs;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "hi";
        msgs.append(msg);
        body["messages"] = msgs;
        body["max_tokens"] = 5;
        result.body = QJsonDocument(body).toJson();
    } else {
        // OpenAI-compatible (default)
        QString url = config.baseUrl + middleRoute + QStringLiteral("/chat/completions");
        result.request.setUrl(QUrl(url));
        result.request.setRawHeader("Authorization", ("Bearer " + config.apiKey).toUtf8());
        result.request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject body;
        body["model"] = config.modelId;
        QJsonArray msgs;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "hi";
        msgs.append(msg);
        body["messages"] = msgs;
        body["max_tokens"] = 5;
        result.body = QJsonDocument(body).toJson();
    }

    result.request.setTransferTimeout(30000);

    // Custom headers from config
    for (auto it = config.customHeaders.cbegin(); it != config.customHeaders.cend(); ++it)
        result.request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    return result;
}

const QList<AdapterEntry> OUTBOUND_ADAPTERS = {
    {QStringLiteral("(自动)"),              QStringLiteral("")},
    {QStringLiteral("OpenAI"),           QStringLiteral("openai")},
    {QStringLiteral("Anthropic"),        QStringLiteral("anthropic")},
    {QStringLiteral("Gemini"),           QStringLiteral("gemini")},
    {QStringLiteral("ZAI"),              QStringLiteral("zai")},
    {QStringLiteral("Bailian"),          QStringLiteral("bailian")},
    {QStringLiteral("ModelScope"),       QStringLiteral("modelscope")},
    {QStringLiteral("Codex"),            QStringLiteral("codex")},
    {QStringLiteral("Claude Code"),      QStringLiteral("claudecode")},
    {QStringLiteral("Antigravity"),      QStringLiteral("antigravity")},
    {QStringLiteral("OpenRouter"),       QStringLiteral("openrouter")},
    {QStringLiteral("xAI"),              QStringLiteral("xai")},
    {QStringLiteral("DeepSeek"),         QStringLiteral("deepseek")},
    {QStringLiteral("Doubao"),           QStringLiteral("doubao")},
    {QStringLiteral("Moonshot"),         QStringLiteral("moonshot")},
};

}

// ============================================================================
// ProxyConfigDialog
// ============================================================================

ProxyConfigDialog::ProxyConfigDialog(QWidget* parent)
    : QDialog(parent)
    , m_fetchTimer(nullptr)
    , m_nam(nullptr)
{
    setupUi();
}

ProxyConfigDialog::ProxyConfigDialog(const ConfigGroup& config, QWidget* parent)
    : QDialog(parent)
    , m_fetchTimer(nullptr)
    , m_nam(nullptr)
{
    setupUi();
    populateFields(config);
}

ProxyConfigDialog::~ProxyConfigDialog() {
    stopModelFetchTimer();
}

void ProxyConfigDialog::setupUi() {
    setWindowTitle(QStringLiteral("配置编辑"));
    setMinimumWidth(500);

    auto* layout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    // ==== Tab "基本" ====
    auto* basicTab = new QWidget(this);
    auto* form = new QFormLayout(basicTab);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("可选，留空则使用模型名称"));
    form->addRow(QStringLiteral("配置名称:"), m_nameEdit);

    m_localUrlCombo = new QComboBox(this);
    m_localUrlCombo->setEditable(false);
    m_localUrlCombo->setMinimumWidth(350);
    for (const auto& a : INBOUND_ADAPTERS)
        m_localUrlCombo->addItem(a.displayName, a.id);
    m_localUrlCombo->setCurrentIndex(0);
    form->addRow(QStringLiteral("入站适配器:"), m_localUrlCombo);

    m_outboundAdapterCombo = new QComboBox(this);
    m_outboundAdapterCombo->setEditable(false);
    m_outboundAdapterCombo->setMinimumWidth(350);
    for (const auto& a : OUTBOUND_ADAPTERS)
        m_outboundAdapterCombo->addItem(a.displayName, a.id);
    m_outboundAdapterCombo->setCurrentIndex(0);
    form->addRow(QStringLiteral("出站适配器:"), m_outboundAdapterCombo);

    m_mappedUrlEdit = new QLineEdit(this);
    m_mappedUrlEdit->setMinimumWidth(350);
    m_mappedUrlEdit->setPlaceholderText(QStringLiteral("如 https://api.openai.com"));
    m_mappedUrlEdit->setText("https://api.openai.com");
    form->addRow(QStringLiteral("供应商URL:"), m_mappedUrlEdit);

    m_mappedModelCombo = new QComboBox(this);
    m_mappedModelCombo->setEditable(true);
    m_mappedModelCombo->setMinimumWidth(350);
    form->addRow(QStringLiteral("供应商模型名称:"), m_mappedModelCombo);

    m_middleRouteEdit = new QLineEdit(this);
    m_middleRouteEdit->setPlaceholderText(QStringLiteral("/v1"));
    form->addRow(QStringLiteral("中间路由:"), m_middleRouteEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(QStringLiteral("必填"));
    form->addRow(QStringLiteral("API Key:"), m_apiKeyEdit);

    m_tabWidget->addTab(basicTab, QStringLiteral("基本"));

    // ==== Tab "高级" ====
    auto* advancedTab = new QWidget(this);
    auto* advLayout = new QVBoxLayout(advancedTab);

    // Max retry attempts
    auto* retryLayout = new QHBoxLayout();
    retryLayout->addWidget(new QLabel(QStringLiteral("最大重试次数:"), this));
    m_maxRetrySpin = new QSpinBox(this);
    m_maxRetrySpin->setRange(1, 10);
    m_maxRetrySpin->setValue(3);
    retryLayout->addWidget(m_maxRetrySpin);
    retryLayout->addStretch();
    advLayout->addLayout(retryLayout);

    // Hijack domain override
    auto* hijackLayout = new QHBoxLayout();
    hijackLayout->addWidget(new QLabel(QStringLiteral("劫持域名覆盖:"), this));
    m_hijackDomainEdit = new QLineEdit(this);
    m_hijackDomainEdit->setPlaceholderText(QStringLiteral("留空则自动从入站适配器派生"));
    hijackLayout->addWidget(m_hijackDomainEdit);
    advLayout->addLayout(hijackLayout);

    // Custom headers group
    auto* headersGroup = new QGroupBox(QStringLiteral("自定义请求头"), this);
    auto* headersLayout = new QVBoxLayout(headersGroup);
    m_customHeadersTable = new QTableWidget(0, 2, this);
    m_customHeadersTable->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("值")});
    m_customHeadersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_customHeadersTable->setMaximumHeight(120);
    headersLayout->addWidget(m_customHeadersTable);
    auto* headerBtnLayout = new QHBoxLayout();
    m_btnAddHeader = new QPushButton(QStringLiteral("+"), this);
    m_btnRemoveHeader = new QPushButton(QStringLiteral("-"), this);
    headerBtnLayout->addWidget(m_btnAddHeader);
    headerBtnLayout->addWidget(m_btnRemoveHeader);
    headerBtnLayout->addStretch();
    headersLayout->addLayout(headerBtnLayout);
    advLayout->addWidget(headersGroup);

    connect(m_btnAddHeader, &QPushButton::clicked, this, [this]() {
        int row = m_customHeadersTable->rowCount();
        m_customHeadersTable->insertRow(row);
    });
    connect(m_btnRemoveHeader, &QPushButton::clicked, this, [this]() {
        int row = m_customHeadersTable->currentRow();
        if (row >= 0)
            m_customHeadersTable->removeRow(row);
    });

    // Base URL candidates group
    auto* candidatesGroup = new QGroupBox(QStringLiteral("备选URL列表"), this);
    auto* candidatesLayout = new QVBoxLayout(candidatesGroup);
    m_candidatesList = new QListWidget(this);
    m_candidatesList->setMaximumHeight(80);
    candidatesLayout->addWidget(m_candidatesList);
    auto* candidateBtnLayout = new QHBoxLayout();
    m_candidateUrlEdit = new QLineEdit(this);
    m_candidateUrlEdit->setPlaceholderText(QStringLiteral("https://..."));
    candidateBtnLayout->addWidget(m_candidateUrlEdit);
    m_btnAddCandidate = new QPushButton(QStringLiteral("添加"), this);
    m_btnRemoveCandidate = new QPushButton(QStringLiteral("移除"), this);
    candidateBtnLayout->addWidget(m_btnAddCandidate);
    candidateBtnLayout->addWidget(m_btnRemoveCandidate);
    candidatesLayout->addLayout(candidateBtnLayout);
    advLayout->addWidget(candidatesGroup);

    connect(m_btnAddCandidate, &QPushButton::clicked, this, [this]() {
        QString url = m_candidateUrlEdit->text().trimmed();
        if (!url.isEmpty()) {
            m_candidatesList->addItem(url);
            m_candidateUrlEdit->clear();
        }
    });
    connect(m_btnRemoveCandidate, &QPushButton::clicked, this, [this]() {
        auto* item = m_candidatesList->currentItem();
        if (item)
            delete m_candidatesList->takeItem(m_candidatesList->row(item));
    });

    advLayout->addStretch();
    m_tabWidget->addTab(advancedTab, QStringLiteral("高级"));

    layout->addWidget(m_tabWidget);

    m_statusLabel = new QLabel(QStringLiteral("输入供应商URL和API Key后可获取模型列表"), this);
    layout->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProxyConfigDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // model fetch
    m_nam = new QNetworkAccessManager(this);
    connect(m_mappedUrlEdit, &QLineEdit::textChanged, this, &ProxyConfigDialog::onMappedUrlChanged);
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, &ProxyConfigDialog::onApiKeyChanged);
    connect(m_outboundAdapterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { if (canFetchModels()) m_fetchTimer->start(); });

    m_fetchTimer = new QTimer(this);
    m_fetchTimer->setInterval(1500);
    m_fetchTimer->setSingleShot(true);
    connect(m_fetchTimer, &QTimer::timeout, this, &ProxyConfigDialog::fetchModelsFromApi);
}

void ProxyConfigDialog::populateFields(const ConfigGroup& config) {
    m_nameEdit->setText(config.name);

    int inIdx = m_localUrlCombo->findData(config.provider);
    if (inIdx >= 0)
        m_localUrlCombo->setCurrentIndex(inIdx);
    else
        m_localUrlCombo->setCurrentIndex(0);

    int outIdx = m_outboundAdapterCombo->findData(config.outboundAdapter);
    if (outIdx >= 0)
        m_outboundAdapterCombo->setCurrentIndex(outIdx);
    else
        m_outboundAdapterCombo->setCurrentIndex(0);

    m_mappedUrlEdit->setText(config.baseUrl);
    m_mappedModelCombo->addItem(config.modelId);
    m_mappedModelCombo->setCurrentText(config.modelId);
    m_middleRouteEdit->setText(config.middleRoute);
    m_apiKeyEdit->setText(config.apiKey);

    // Advanced fields
    m_maxRetrySpin->setValue(config.maxRetryAttempts);
    m_hijackDomainEdit->setText(config.hijackDomainOverride);

    // Populate custom headers table
    m_customHeadersTable->setRowCount(0);
    for (auto it = config.customHeaders.cbegin(); it != config.customHeaders.cend(); ++it) {
        int row = m_customHeadersTable->rowCount();
        m_customHeadersTable->insertRow(row);
        m_customHeadersTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_customHeadersTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }

    // Populate candidates list
    m_candidatesList->clear();
    for (const auto& url : config.baseUrlCandidates) {
        m_candidatesList->addItem(url);
    }
}

ConfigGroup ProxyConfigDialog::getConfig() const {
    ConfigGroup g;
    g.name = m_nameEdit->text().trimmed();
    g.provider = m_localUrlCombo->currentData().toString();
    g.outboundAdapter = m_outboundAdapterCombo->currentData().toString();
    g.baseUrl = m_mappedUrlEdit->text().trimmed();
    g.modelId = m_mappedModelCombo->currentText().trimmed();
    g.apiKey = m_apiKeyEdit->text().trimmed();
    g.middleRoute = m_middleRouteEdit->text().trimmed();
    if (g.middleRoute.isEmpty())
        g.middleRoute = "/v1";
    g.maxRetryAttempts = m_maxRetrySpin->value();
    g.hijackDomainOverride = m_hijackDomainEdit->text().trimmed();

    // Read custom headers from table
    for (int row = 0; row < m_customHeadersTable->rowCount(); ++row) {
        auto* keyItem = m_customHeadersTable->item(row, 0);
        auto* valItem = m_customHeadersTable->item(row, 1);
        if (keyItem && valItem) {
            QString key = keyItem->text().trimmed();
            QString val = valItem->text().trimmed();
            if (!key.isEmpty())
                g.customHeaders[key] = val;
        }
    }

    // Read candidates list
    for (int i = 0; i < m_candidatesList->count(); ++i) {
        QString url = m_candidatesList->item(i)->text().trimmed();
        if (!url.isEmpty())
            g.baseUrlCandidates.append(url);
    }

    if (g.name.isEmpty())
        g.name = g.modelId;
    return g;
}

void ProxyConfigDialog::onAccept() {
    if (m_mappedUrlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("供应商URL不能为空"));
        return;
    }
    if (m_mappedModelCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("供应商模型名称不能为空"));
        return;
    }
    if (m_apiKeyEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("验证失败"), QStringLiteral("API Key不能为空"));
        return;
    }
    accept();
}

bool ProxyConfigDialog::canFetchModels() const {
    return !m_mappedUrlEdit->text().trimmed().isEmpty()
        && !m_apiKeyEdit->text().trimmed().isEmpty();
}

void ProxyConfigDialog::onMappedUrlChanged() {
    if (canFetchModels()) m_fetchTimer->start();
}

void ProxyConfigDialog::onApiKeyChanged() {
    if (canFetchModels()) m_fetchTimer->start();
}

void ProxyConfigDialog::stopModelFetchTimer() {
    if (m_fetchTimer && m_fetchTimer->isActive())
        m_fetchTimer->stop();
}

void ProxyConfigDialog::fetchModelsFromApi() {
    QString baseUrl = m_mappedUrlEdit->text().trimmed();
    QString apiKey = m_apiKeyEdit->text().trimmed();
    if (baseUrl.isEmpty() || apiKey.isEmpty()) return;

    m_lastFetchedUrl = baseUrl;
    m_lastFetchedKey = apiKey;

    // Determine provider type from outbound adapter; fall back to URL detection
    const QString outAdapter = m_outboundAdapterCombo->currentData().toString();
    const ModelListProvider provider = outAdapter.isEmpty()
        ? provider_routing::detectModelListProvider(QString(), baseUrl)
        : provider_routing::detectModelListProvider(outAdapter, baseUrl);

    QString middleRoute = provider_routing::effectiveMiddleRoute(m_middleRouteEdit->text(), provider);
    if (!middleRoute.isEmpty() && baseUrl.endsWith(middleRoute)) {
        middleRoute.clear();
    }

    QUrl url(baseUrl + middleRoute + QStringLiteral("/models"));
    if (provider == ModelListProvider::Gemini) {
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("key"), apiKey);
        url.setQuery(query);
    }

    QMap<QString, QString> customHeaders;
    for (int row = 0; row < m_customHeadersTable->rowCount(); ++row) {
        const auto* keyItem = m_customHeadersTable->item(row, 0);
        const auto* valItem = m_customHeadersTable->item(row, 1);
        const QString key = keyItem ? keyItem->text().trimmed() : QString();
        const QString val = valItem ? valItem->text().trimmed() : QString();
        if (!key.isEmpty() && !val.isEmpty()) {
            customHeaders.insert(key, val);
        }
    }

    auto buildRequest = [&](const QString& authMode) {
        QNetworkRequest req{url};
        if (authMode == QStringLiteral("anthropic")) {
            req.setRawHeader("x-api-key", apiKey.toUtf8());
            req.setRawHeader("anthropic-version", "2023-06-01");
        } else if (authMode == QStringLiteral("gemini")) {
            req.setRawHeader("x-goog-api-key", apiKey.toUtf8());
        } else {
            req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
        }
        for (auto it = customHeaders.cbegin(); it != customHeaders.cend(); ++it) {
            req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }
        req.setTransferTimeout(15000);
        return req;
    };

    const QStringList authModes = provider_routing::authModesForModelList(provider);

    m_statusLabel->setText(QStringLiteral("Fetching model list..."));

    struct ModelFetchState : public std::enable_shared_from_this<ModelFetchState> {
        ProxyConfigDialog* dialog = nullptr;
        QStringList authModes;
        int modeIndex = 0;
        std::function<QNetworkRequest(const QString&)> buildRequest;

        void run() {
            if (!dialog || !dialog->m_nam) {
                return;
            }
            if (modeIndex >= authModes.size()) {
                dialog->m_statusLabel->setText(
                    QStringLiteral("Fetch failed; please input model manually."));
                return;
            }

            const QString authMode = authModes.at(modeIndex);
            QNetworkRequest req = buildRequest(authMode);
            auto* reply = dialog->m_nam->get(req);
            reply->ignoreSslErrors();

            QObject::connect(reply, &QNetworkReply::finished, dialog,
                             [state = shared_from_this(), reply, authMode]() {
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = reply->readAll();
                const bool ok = reply->error() == QNetworkReply::NoError
                                && status >= 200 && status < 300;

                if (ok) {
                    QStringList models = model_list_utils::parseModelIds(body);

                    QString prevText = state->dialog->m_mappedModelCombo->currentText();
                    state->dialog->m_mappedModelCombo->clear();
                    for (const auto& m : models)
                        state->dialog->m_mappedModelCombo->addItem(m);

                    int idx = state->dialog->m_mappedModelCombo->findText(prevText);
                    if (idx >= 0)
                        state->dialog->m_mappedModelCombo->setCurrentIndex(idx);
                    else if (!prevText.isEmpty())
                        state->dialog->m_mappedModelCombo->setCurrentText(prevText);

                    state->dialog->m_statusLabel->setText(
                        QStringLiteral("Fetched %1 models").arg(models.size()));
                    reply->deleteLater();
                    return;
                }

                const QString bodyPreview = QString::fromUtf8(body.left(256));
                LOG_WARNING(QStringLiteral("ProxyConfigDialog: fetch models auth=%1 status=%2 err=%3 body=%4")
                                .arg(authMode)
                                .arg(status)
                                .arg(reply->errorString())
                                .arg(bodyPreview));

                const bool authFailure = (status == 401 || status == 403);
                const bool canRetry = (state->modeIndex + 1) < state->authModes.size();
                reply->deleteLater();
                if (authFailure && canRetry) {
                    ++state->modeIndex;
                    state->run();
                    return;
                }

                state->dialog->m_statusLabel->setText(
                    QStringLiteral("Fetch failed; please input model manually."));
            });
        }
    };

    auto state = std::make_shared<ModelFetchState>();
    state->dialog = this;
    state->authModes = authModes;
    state->buildRequest = buildRequest;
    state->run();

}

// ============================================================================
// ConfigGroupPanel
// ============================================================================

ConfigGroupPanel::ConfigGroupPanel(ConfigStore* config, Bootstrap* bootstrap,
                                   QWidget* parent)
    : QWidget(parent)
    , m_config(config)
    , m_bootstrap(bootstrap)
{
    setupUi();
    refreshTable();
}

void ConfigGroupPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("名称"),
        QStringLiteral("入站/出站"),
        QStringLiteral("基础URL"),
        QStringLiteral("模型"),
        QStringLiteral("中间路由"),
        QStringLiteral("API Key"),
        QStringLiteral("状态")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ConfigGroupPanel::onSelectionChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, &ConfigGroupPanel::onEditConfig);

    layout->addWidget(m_table);

    // buttons
    auto* btnLayout = new QHBoxLayout();

    m_btnAdd = new QPushButton(QStringLiteral("新增"), this);
    m_btnEdit = new QPushButton(QStringLiteral("修改"), this);
    m_btnDelete = new QPushButton(QStringLiteral("删除"), this);

    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnEdit);
    btnLayout->addWidget(m_btnDelete);
    btnLayout->addStretch();

    m_btnImport = new QPushButton(QStringLiteral("导入"), this);
    m_btnExport = new QPushButton(QStringLiteral("导出"), this);
    m_btnRefresh = new QPushButton(QStringLiteral("刷新"), this);
    m_btnTest = new QPushButton(QStringLiteral("测活"), this);
    m_btnTestAll = new QPushButton(QStringLiteral("一键测活"), this);

    btnLayout->addWidget(m_btnImport);
    btnLayout->addWidget(m_btnExport);
    btnLayout->addWidget(m_btnRefresh);
    btnLayout->addWidget(m_btnTest);
    btnLayout->addWidget(m_btnTestAll);

    layout->addLayout(btnLayout);

    // connections
    connect(m_btnAdd, &QPushButton::clicked, this, &ConfigGroupPanel::onAddConfig);
    connect(m_btnEdit, &QPushButton::clicked, this, &ConfigGroupPanel::onEditConfig);
    connect(m_btnDelete, &QPushButton::clicked, this, &ConfigGroupPanel::onDeleteConfig);
    connect(m_btnRefresh, &QPushButton::clicked, this, &ConfigGroupPanel::onRefresh);
    connect(m_btnTest, &QPushButton::clicked, this, &ConfigGroupPanel::onTestConfig);
    connect(m_btnTestAll, &QPushButton::clicked, this, &ConfigGroupPanel::onTestAllConfigs);
    connect(m_btnExport, &QPushButton::clicked, this, &ConfigGroupPanel::onExportConfig);
    connect(m_btnImport, &QPushButton::clicked, this, &ConfigGroupPanel::onImportConfig);

    updateButtonStates();
}

void ConfigGroupPanel::refreshTable() {
    m_table->setRowCount(0);
    if (!m_config) return;

    const auto& groups = m_config->groups();

    // sort by name
    QList<int> indices;
    for (int i = 0; i < groups.size(); ++i)
        indices.append(i);
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return groups[a].name.toLower() < groups[b].name.toLower();
    });

    m_table->setRowCount(indices.size());
    for (int row = 0; row < indices.size(); ++row) {
        const auto& g = groups[indices[row]];
        auto setItem = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            item->setData(Qt::UserRole, indices[row]); // store real index
            m_table->setItem(row, col, item);
        };
        setItem(0, g.name);
        setItem(1, g.provider + QStringLiteral(" → ") + (g.outboundAdapter.isEmpty() ? QStringLiteral("自动") : g.outboundAdapter));
        setItem(2, g.baseUrl);
        setItem(3, g.modelId);
        setItem(4, g.middleRoute);
        setItem(5, maskApiKey(g.apiKey));
        QString status = m_statusCache.value(indices[row], QStringLiteral("就绪"));
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setData(Qt::UserRole, indices[row]);
        m_table->setItem(row, 6, statusItem);
    }

    updateButtonStates();
}

void ConfigGroupPanel::onAddConfig() {
    ProxyConfigDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto config = dlg.getConfig();
        QVariantMap map;
        map["name"] = config.name;
        map["provider"] = config.provider;
        map["outbound_adapter"] = config.outboundAdapter;
        map["base_url"] = config.baseUrl;
        map["model_id"] = config.modelId;
        map["api_key"] = config.apiKey;
        map["middle_route"] = config.middleRoute;
        map["max_retry_attempts"] = config.maxRetryAttempts;
        map["hijack_domain_override"] = config.hijackDomainOverride;
        // custom headers
        QVariantMap headerMap;
        for (auto it = config.customHeaders.cbegin(); it != config.customHeaders.cend(); ++it)
            headerMap[it.key()] = it.value();
        if (!headerMap.isEmpty())
            map["custom_headers"] = headerMap;
        // base URL candidates
        if (!config.baseUrlCandidates.isEmpty())
            map["base_url_candidates"] = QVariant(config.baseUrlCandidates);
        m_config->addGroup(map);
        refreshTable();
        emit configChanged();
        emit logMessage(QStringLiteral("已添加配置: %1").arg(config.name), 1);
    }
}

void ConfigGroupPanel::onEditConfig() {
    int row = currentRow();
    if (row < 0) return;

    int realIndex = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    auto config = m_config->groupAt(realIndex);

    ProxyConfigDialog dlg(config, this);
    if (dlg.exec() == QDialog::Accepted) {
        auto newConfig = dlg.getConfig();
        QVariantMap map;
        map["name"] = newConfig.name;
        map["provider"] = newConfig.provider;
        map["outbound_adapter"] = newConfig.outboundAdapter;
        map["base_url"] = newConfig.baseUrl;
        map["model_id"] = newConfig.modelId;
        map["api_key"] = newConfig.apiKey;
        map["middle_route"] = newConfig.middleRoute;
        map["max_retry_attempts"] = newConfig.maxRetryAttempts;
        map["hijack_domain_override"] = newConfig.hijackDomainOverride;
        // custom headers
        QVariantMap headerMap;
        for (auto it = newConfig.customHeaders.cbegin(); it != newConfig.customHeaders.cend(); ++it)
            headerMap[it.key()] = it.value();
        if (!headerMap.isEmpty())
            map["custom_headers"] = headerMap;
        // base URL candidates
        if (!newConfig.baseUrlCandidates.isEmpty())
            map["base_url_candidates"] = QVariant(newConfig.baseUrlCandidates);
        m_config->updateGroup(realIndex, map);
        refreshTable();
        emit configChanged();
        emit logMessage(QStringLiteral("已修改配置: %1").arg(newConfig.name), 1);
    }
}

void ConfigGroupPanel::onDeleteConfig() {
    int row = currentRow();
    if (row < 0) return;

    int realIndex = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    auto config = m_config->groupAt(realIndex);

    auto reply = QMessageBox::question(this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除配置 \"%1\" 吗？").arg(config.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_config->removeGroup(realIndex);
        refreshTable();
        emit configChanged();
        emit logMessage(QStringLiteral("已删除配置: %1").arg(config.name), 1);
    }
}

void ConfigGroupPanel::onRefresh() {
    if (m_config) {
        m_config->load(QString());
        refreshTable();
    }
}

void ConfigGroupPanel::onTestConfig() {
    int row = currentRow();
    if (row < 0) return;

    int realIndex = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    auto config = m_config->groupAt(realIndex);

    m_btnTest->setEnabled(false);

    auto* nam = new QNetworkAccessManager(this);
    auto tr = buildTestRequest(config);

    auto* reply = nam->post(tr.request, tr.body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, config, nam, realIndex]() {
        m_btnTest->setEnabled(true);
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        QString err = ok ? QString() : reply->errorString();

        TestResultItem item;
        item.name = config.name;
        item.success = ok;
        item.httpStatus = status;
        item.errorMessage = err;

        TestResultDialog dlg(item, this);
        dlg.exec();
        updateTestStatus(realIndex, ok, status);

        reply->deleteLater();
        nam->deleteLater();
    });
}

void ConfigGroupPanel::onTestAllConfigs() {
    if (!m_config) return;
    const auto& groups = m_config->groups();
    int total = groups.size();
    if (total == 0) return;

    m_btnTestAll->setEnabled(false);
    m_btnTest->setEnabled(false);

    auto results = std::make_shared<QList<TestResultItem>>();
    auto counter = std::make_shared<int>(0);

    for (int i = 0; i < total; ++i) {
        const auto& g = groups[i];

        auto* nam = new QNetworkAccessManager(this);
        auto tr = buildTestRequest(g);

        auto* reply = nam->post(tr.request, tr.body);
        connect(reply, &QNetworkReply::finished, this,
            [this, reply, g, nam, results, counter, total, i]() {
                int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;

                TestResultItem item;
                item.name = g.name;
                item.success = ok;
                item.httpStatus = status;
                item.errorMessage = ok ? QString() : reply->errorString();
                results->append(item);
                updateTestStatus(i, ok, status);

                reply->deleteLater();
                nam->deleteLater();

                (*counter)++;
                if (*counter >= total) {
                    m_btnTestAll->setEnabled(true);
                    m_btnTest->setEnabled(true);
                    TestResultDialog dlg(*results, this);
                    dlg.exec();
                }
            });
    }
}

void ConfigGroupPanel::onExportConfig() {
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/proxy_config.json";
    QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出配置"), defaultPath,
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    if (!m_config) {
        return;
    }

    QJsonArray arr;
    for (const auto& g : m_config->groups()) {
        QJsonObject obj;
        obj["name"] = g.name;
        obj["provider"] = g.provider;
        obj["outbound_adapter"] = g.outboundAdapter;
        obj["base_url"] = g.baseUrl;
        obj["model_id"] = g.modelId;
        obj["api_key"] = m_config->encodeApiKeyForExternal(g.apiKey);
        obj["middle_route"] = g.middleRoute;
        obj["max_retry_attempts"] = g.maxRetryAttempts;
        obj["hijack_domain_override"] = g.hijackDomainOverride;
        // custom_headers
        QJsonObject headersObj;
        for (auto it = g.customHeaders.cbegin(); it != g.customHeaders.cend(); ++it)
            headersObj[it.key()] = it.value();
        obj["custom_headers"] = headersObj;
        // base_url_candidates
        QJsonArray candidatesArr;
        for (const auto& u : g.baseUrlCandidates)
            candidatesArr.append(u);
        obj["base_url_candidates"] = candidatesArr;
        arr.append(obj);
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        emit logMessage(QStringLiteral("配置已导出到: %1").arg(path), 1);
    }
}

void ConfigGroupPanel::onImportConfig() {
    QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("导入配置"), QString(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无效的配置文件格式"));
        return;
    }

    int imported = 0;
    QJsonArray arr = doc.array();
    for (const auto& item : arr) {
        QJsonObject obj = item.toObject();
        QString modelId = obj["model_id"].toString();
        QString apiKey = obj["api_key"].toString();
        if (m_config) {
            apiKey = m_config->decodeApiKeyFromExternal(apiKey);
        }
        if (modelId.isEmpty() || apiKey.isEmpty()) continue;

        QVariantMap map;
        map["name"] = obj["name"].toString(modelId);
        map["provider"] = provider_routing::migrateProviderField(obj["provider"].toString("openai"));
        map["outbound_adapter"] = obj["outbound_adapter"].toString();
        map["base_url"] = obj["base_url"].toString("https://api.openai.com");
        map["model_id"] = modelId;
        map["api_key"] = apiKey;
        map["middle_route"] = obj["middle_route"].toString("/v1");
        map["max_retry_attempts"] = obj["max_retry_attempts"].toInt(3);
        map["hijack_domain_override"] = obj["hijack_domain_override"].toString();
        m_config->addGroup(map);
        ++imported;
    }

    refreshTable();
    emit configChanged();
    emit logMessage(QStringLiteral("已导入 %1 个配置").arg(imported), 1);
}

void ConfigGroupPanel::onSelectionChanged() {
    updateButtonStates();
}

void ConfigGroupPanel::updateButtonStates() {
    bool hasSelection = currentRow() >= 0;
    m_btnEdit->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection);
    m_btnTest->setEnabled(hasSelection);
}

int ConfigGroupPanel::currentRow() const {
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return -1;
    return items.first()->row();
}

QString ConfigGroupPanel::maskApiKey(const QString& apiKey) const {
    if (apiKey.isEmpty()) return QStringLiteral("(无)");
    if (apiKey.length() <= 8) return QStringLiteral("***");
    return apiKey.left(4) + "***" + apiKey.right(4);
}

void ConfigGroupPanel::updateTestStatus(int groupIndex, bool success, int httpStatus) {
    QString status = success
        ? QStringLiteral("正常 (%1)").arg(httpStatus)
        : QStringLiteral("异常 (%1)").arg(httpStatus);
    m_statusCache[groupIndex] = status;

    // Find and update the row
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* item = m_table->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == groupIndex) {
            auto* statusItem = m_table->item(row, 6);
            if (statusItem) {
                statusItem->setText(status);
                statusItem->setForeground(success ? QColor(Theme::success) : QColor(Theme::error));
            }
            break;
        }
    }
}
