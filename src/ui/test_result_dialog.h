#pragma once
#include <QDialog>
#include <QList>
#include "runtime_options_panel.h"

class TestResultDialog : public QDialog {
    Q_OBJECT

public:
    // single result
    TestResultDialog(const TestResultItem& result, QWidget* parent = nullptr);
    // batch results
    TestResultDialog(const QList<TestResultItem>& results, QWidget* parent = nullptr);

private:
    void setupSingleResultUi(const TestResultItem& result);
    void setupBatchResultUi(const QList<TestResultItem>& results);
};
