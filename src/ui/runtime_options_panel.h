#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

class ConfigStore;

struct TestResultItem {
    QString name;
    bool success = false;
    int httpStatus = 0;
    QString errorMessage;
};

class RuntimeOptionsPanel : public QWidget {
    Q_OBJECT

public:
    explicit RuntimeOptionsPanel(ConfigStore* config, QWidget* parent = nullptr);
    ~RuntimeOptionsPanel() override = default;

    void loadFromConfig();
    void saveToConfig();

signals:
    void optionsChanged();

private slots:
    void onOptionChanged();

private:
    void setupUi();

    QCheckBox* m_chkDebugMode;
    QCheckBox* m_chkDisableSslStrict;
    QCheckBox* m_chkHttp2;
    QCheckBox* m_chkConnPool;
    QSpinBox*  m_spinPoolSize;
    QComboBox* m_comboUpstream;
    QComboBox* m_comboDownstream;
    QSpinBox*  m_spinProxyPort;
    QSpinBox*  m_spinRequestTimeout;
    QSpinBox*  m_spinConnectionTimeout;

    ConfigStore* m_config;
};
