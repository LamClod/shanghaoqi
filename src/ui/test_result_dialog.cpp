#include "test_result_dialog.h"
#include "theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFont>

TestResultDialog::TestResultDialog(const TestResultItem& result, QWidget* parent)
    : QDialog(parent)
{
    setupSingleResultUi(result);
}

TestResultDialog::TestResultDialog(const QList<TestResultItem>& results, QWidget* parent)
    : QDialog(parent)
{
    setupBatchResultUi(results);
}

void TestResultDialog::setupSingleResultUi(const TestResultItem& result) {
    setWindowTitle(QStringLiteral("测活结果"));
    setFixedSize(260, 120);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* row = new QHBoxLayout();

    // icon
    auto* icon = new QLabel(this);
    QFont iconFont;
    iconFont.setPointSize(24);
    icon->setFont(iconFont);
    if (result.success) {
        icon->setText(QStringLiteral("\u2713"));
        icon->setStyleSheet(QString("QLabel { color: %1; }").arg(Theme::success));
    } else {
        icon->setText(QStringLiteral("\u2717"));
        icon->setStyleSheet(QString("QLabel { color: %1; }").arg(Theme::error));
    }
    row->addWidget(icon);

    auto* info = new QVBoxLayout();
    auto* nameLabel = new QLabel(result.name, this);
    QFont nameFont;
    nameFont.setPointSize(11);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    info->addWidget(nameLabel);

    QString statusText;
    if (result.success) {
        statusText = QStringLiteral("测活成功 (HTTP %1)").arg(result.httpStatus);
    } else {
        statusText = QStringLiteral("测活失败 (HTTP %1)").arg(result.httpStatus);
        if (!result.errorMessage.isEmpty())
            statusText += " - " + result.errorMessage;
    }
    info->addWidget(new QLabel(statusText, this));
    row->addLayout(info);
    row->addStretch();
    layout->addLayout(row);

    layout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* okBtn = new QPushButton(QStringLiteral("确定"), this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);
}

void TestResultDialog::setupBatchResultUi(const QList<TestResultItem>& results) {
    setWindowTitle(QStringLiteral("一键测活结果"));
    setMinimumSize(320, 200);
    resize(340, 260);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // stats
    int successCount = 0;
    for (const auto& r : results)
        if (r.success) ++successCount;
    int failCount = results.size() - successCount;

    auto* statsLabel = new QLabel(
        QStringLiteral("成功: %1  |  失败: %2  |  总计: %3")
            .arg(successCount).arg(failCount).arg(results.size()), this);
    QFont statsFont;
    statsFont.setPointSize(10);
    statsFont.setBold(true);
    statsLabel->setFont(statsFont);
    layout->addWidget(statsLabel);

    // scroll area
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto* container = new QWidget(scrollArea);
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setSpacing(4);
    containerLayout->setContentsMargins(4, 4, 4, 4);

    for (const auto& r : results) {
        auto* row = new QHBoxLayout();

        auto* icon = new QLabel(this);
        if (r.success) {
            icon->setText(QStringLiteral("\u2713"));
            icon->setStyleSheet(QString("QLabel { color: %1; }").arg(Theme::success));
        } else {
            icon->setText(QStringLiteral("\u2717"));
            icon->setStyleSheet(QString("QLabel { color: %1; }").arg(Theme::error));
        }
        row->addWidget(icon);

        auto* nameLabel = new QLabel(r.name, this);
        row->addWidget(nameLabel, 1);

        auto* statusLabel = new QLabel(QStringLiteral("HTTP %1").arg(r.httpStatus), this);
        row->addWidget(statusLabel);

        containerLayout->addLayout(row);
    }
    containerLayout->addStretch();

    scrollArea->setWidget(container);
    layout->addWidget(scrollArea, 1);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* okBtn = new QPushButton(QStringLiteral("确定"), this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);
}
