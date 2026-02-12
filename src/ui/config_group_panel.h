#pragma once
#include <QWidget>
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QSpinBox>
#include <QTabWidget>
#include <QListWidget>

class ConfigStore;
class Bootstrap;
struct ConfigGroup;

// ============================================================================
// ProxyConfigDialog
// ============================================================================

class ProxyConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProxyConfigDialog(QWidget* parent = nullptr);
    ProxyConfigDialog(const ConfigGroup& config, QWidget* parent = nullptr);
    ~ProxyConfigDialog() override;

    ConfigGroup getConfig() const;

private slots:
    void onAccept();
    void onMappedUrlChanged();
    void onApiKeyChanged();

private:
    void setupUi();
    void populateFields(const ConfigGroup& config);
    void stopModelFetchTimer();
    void fetchModelsFromApi();
    bool canFetchModels() const;

    QLineEdit*  m_nameEdit;
    QComboBox*  m_localUrlCombo;
    QComboBox*  m_outboundAdapterCombo;
    QLineEdit*  m_mappedUrlEdit;
    QComboBox*  m_mappedModelCombo;
    QLineEdit*  m_middleRouteEdit;
    QLineEdit*  m_apiKeyEdit;
    QLabel*     m_statusLabel;

    QTabWidget*   m_tabWidget;
    QSpinBox*     m_maxRetrySpin;
    QLineEdit*    m_hijackDomainEdit;
    QTableWidget* m_customHeadersTable;
    QPushButton*  m_btnAddHeader;
    QPushButton*  m_btnRemoveHeader;
    QListWidget*  m_candidatesList;
    QLineEdit*    m_candidateUrlEdit;
    QPushButton*  m_btnAddCandidate;
    QPushButton*  m_btnRemoveCandidate;

    QTimer*     m_fetchTimer;
    QNetworkAccessManager* m_nam;
    QString m_lastFetchedUrl;
    QString m_lastFetchedKey;
};

// ============================================================================
// ConfigGroupPanel
// ============================================================================

class ConfigGroupPanel : public QWidget {
    Q_OBJECT

public:
    explicit ConfigGroupPanel(ConfigStore* config, Bootstrap* bootstrap,
                              QWidget* parent = nullptr);
    ~ConfigGroupPanel() override = default;

    void refreshTable();

signals:
    void configChanged();
    void logMessage(const QString& message, int level);

private slots:
    void onAddConfig();
    void onEditConfig();
    void onDeleteConfig();
    void onRefresh();
    void onTestConfig();
    void onTestAllConfigs();
    void onExportConfig();
    void onImportConfig();
    void updateTestStatus(int groupIndex, bool success, int httpStatus);
    void onSelectionChanged();

private:
    void setupUi();
    void updateButtonStates();
    int currentRow() const;
    QString maskApiKey(const QString& apiKey) const;

    QTableWidget* m_table;

    QPushButton* m_btnAdd;
    QPushButton* m_btnEdit;
    QPushButton* m_btnDelete;
    QPushButton* m_btnImport;
    QPushButton* m_btnExport;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnTest;
    QPushButton* m_btnTestAll;

    ConfigStore* m_config;
    Bootstrap* m_bootstrap;
    QMap<int, QString> m_statusCache;
};
