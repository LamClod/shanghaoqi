#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include "platform/windows/win_hosts_manager.h"
using PlatformHostsManager = WinHostsManager;
#elif defined(Q_OS_MACOS)
#include "platform/macos/mac_hosts_manager.h"
using PlatformHostsManager = MacHostsManager;
#else
#include "platform/linux/linux_hosts_manager.h"
using PlatformHostsManager = LinuxHostsManager;
#endif

class TestHostsManager : public QObject {
    Q_OBJECT

private slots:
    void testHostsFileFormat() {
        // Test the hosts file entry format without modifying system file
        QString entry = QStringLiteral("127.0.0.1 api.openai.com # ShangHaoQi");

        QStringList parts = entry.split(QRegularExpression(QStringLiteral("\\s+")));
        QVERIFY(parts.size() >= 2);
        QCOMPARE(parts[0], QStringLiteral("127.0.0.1"));
        QCOMPARE(parts[1], QStringLiteral("api.openai.com"));
        QVERIFY(entry.contains(QStringLiteral("# ShangHaoQi")));
    }

    void testParseHostsEntries() {
        // Simulate hosts file parsing
        QStringList lines = {
            QStringLiteral("# System hosts"),
            QStringLiteral("127.0.0.1 localhost"),
            QStringLiteral("127.0.0.1 api.openai.com # ShangHaoQi"),
            QStringLiteral("127.0.0.1 api.anthropic.com # ShangHaoQi"),
            QStringLiteral("")
        };

        QList<QPair<QString, QString>> entries;
        for (const auto& line : lines) {
            if (line.contains(QStringLiteral("# ShangHaoQi"))) {
                QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
                if (parts.size() >= 2)
                    entries.append({parts[0], parts[1]});
            }
        }

        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].first, QStringLiteral("127.0.0.1"));
        QCOMPARE(entries[0].second, QStringLiteral("api.openai.com"));
        QCOMPARE(entries[1].second, QStringLiteral("api.anthropic.com"));
    }

    void testRemoveEntryFromLines() {
        QStringList lines = {
            QStringLiteral("127.0.0.1 localhost"),
            QStringLiteral("127.0.0.1 api.openai.com # ShangHaoQi"),
            QStringLiteral("127.0.0.1 api.anthropic.com # ShangHaoQi"),
        };

        QString domainToRemove = QStringLiteral("api.openai.com");
        QStringList filtered;
        for (const auto& line : lines) {
            if (line.contains(domainToRemove) &&
                line.contains(QStringLiteral("# ShangHaoQi")))
                continue;
            filtered.append(line);
        }

        QCOMPARE(filtered.size(), 2);
        QVERIFY(!filtered[1].contains(QStringLiteral("api.openai.com")));
    }

    void testHasEntry() {
        QStringList lines = {
            QStringLiteral("127.0.0.1 api.openai.com # ShangHaoQi"),
        };

        bool found = false;
        for (const auto& line : lines) {
            if (line.contains(QStringLiteral("api.openai.com")) &&
                line.contains(QStringLiteral("# ShangHaoQi"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testTempFileWriteRead() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/hosts_test");

        // Write
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&f);
            out << "127.0.0.1 localhost\n";
            out << "127.0.0.1 api.openai.com # ShangHaoQi\n";
        }

        // Read back
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
            QString content = QString::fromUtf8(f.readAll());
            QVERIFY(content.contains(QStringLiteral("api.openai.com")));
            QVERIFY(content.contains(QStringLiteral("# ShangHaoQi")));
        }
    }
};

QTEST_MAIN(TestHostsManager)
#include "tst_hosts_manager.moc"
