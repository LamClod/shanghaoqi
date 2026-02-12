#include <QTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "config/config_store.h"
#include "config/config_types.h"

class TestConfigStore : public QObject {
    Q_OBJECT

private slots:
    void testLoadEmptyCreatesDefault() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/config.json");

        ConfigStore store;
        bool ok = store.load(path);
        // Load returns false when file doesn't exist, but store is usable
        Q_UNUSED(ok);

        QCOMPARE(store.groups().size(), 0);
    }

    void testAddAndRetrieveGroup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/config.json");

        ConfigStore store;
        store.load(path);

        QVariantMap group;
        group[QStringLiteral("name")] = QStringLiteral("Test Group");
        group[QStringLiteral("provider")] = QStringLiteral("openai");
        group[QStringLiteral("baseUrl")] = QStringLiteral("https://api.openai.com/v1");
        group[QStringLiteral("modelId")] = QStringLiteral("gpt-4");
        group[QStringLiteral("apiKey")] = QStringLiteral("sk-test-key");

        store.addGroup(group);
        QCOMPARE(store.groups().size(), 1);
        QCOMPARE(store.groups()[0].name, QStringLiteral("Test Group"));
    }

    void testUpdateGroup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/config.json");

        ConfigStore store;
        store.load(path);

        QVariantMap group;
        group[QStringLiteral("name")] = QStringLiteral("Group 1");
        group[QStringLiteral("modelId")] = QStringLiteral("gpt-4");
        group[QStringLiteral("apiKey")] = QStringLiteral("sk-key");
        group[QStringLiteral("baseUrl")] = QStringLiteral("https://example.com");
        store.addGroup(group);

        QVariantMap updated;
        updated[QStringLiteral("name")] = QStringLiteral("Group 1 Updated");
        updated[QStringLiteral("modelId")] = QStringLiteral("gpt-4-turbo");
        updated[QStringLiteral("apiKey")] = QStringLiteral("sk-key-2");
        updated[QStringLiteral("baseUrl")] = QStringLiteral("https://example.com");
        store.updateGroup(0, updated);

        QCOMPARE(store.groups()[0].name, QStringLiteral("Group 1 Updated"));
        QCOMPARE(store.groups()[0].modelId, QStringLiteral("gpt-4-turbo"));
    }

    void testRemoveGroup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/config.json");

        ConfigStore store;
        store.load(path);

        QVariantMap g1;
        g1[QStringLiteral("name")] = QStringLiteral("G1");
        g1[QStringLiteral("modelId")] = QStringLiteral("m1");
        g1[QStringLiteral("apiKey")] = QStringLiteral("k1");
        g1[QStringLiteral("baseUrl")] = QStringLiteral("http://a.com");
        store.addGroup(g1);

        QVariantMap g2;
        g2[QStringLiteral("name")] = QStringLiteral("G2");
        g2[QStringLiteral("modelId")] = QStringLiteral("m2");
        g2[QStringLiteral("apiKey")] = QStringLiteral("k2");
        g2[QStringLiteral("baseUrl")] = QStringLiteral("http://b.com");
        store.addGroup(g2);

        QCOMPARE(store.groups().size(), 2);
        store.removeGroup(0);
        QCOMPARE(store.groups().size(), 1);
        QCOMPARE(store.groups()[0].name, QStringLiteral("G2"));
    }

    void testSaveAndReload() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/config.json");

        {
            ConfigStore store;
            store.load(path);

            QVariantMap group;
            group[QStringLiteral("name")] = QStringLiteral("Saved Group");
            group[QStringLiteral("provider")] = QStringLiteral("anthropic");
            group[QStringLiteral("baseUrl")] = QStringLiteral("https://api.anthropic.com/v1");
            group[QStringLiteral("modelId")] = QStringLiteral("claude-3-opus");
            group[QStringLiteral("apiKey")] = QStringLiteral("sk-ant-test");
            store.addGroup(group);

            store.setMappedModelId(QStringLiteral("gpt-5"));
            store.setAuthKey(QStringLiteral("my-auth-key"));

            bool saved = store.save();
            QVERIFY(saved);
        }

        {
            ConfigStore store2;
            bool loaded = store2.load(path);
            QVERIFY(loaded);
            QCOMPARE(store2.groups().size(), 1);
            QCOMPARE(store2.groups()[0].name, QStringLiteral("Saved Group"));
            QCOMPARE(store2.mappedModelId(), QStringLiteral("gpt-5"));
            QCOMPARE(store2.authKey(), QStringLiteral("my-auth-key"));
        }
    }

    void testRuntimeOptions() {
        ConfigStore store;

        QVariantMap opts;
        opts[QStringLiteral("debugMode")] = true;
        opts[QStringLiteral("proxyPort")] = 8443;
        opts[QStringLiteral("connectionPoolSize")] = 15;
        store.setRuntimeOptions(opts);

        auto config = store.runtimeConfig();
        QCOMPARE(config.debugMode, true);
        QCOMPARE(config.proxyPort, 8443);
        QCOMPARE(config.connectionPoolSize, 15);
    }

    void testCurrentGroupIndex() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.path() + QStringLiteral("/config.json");

        ConfigStore store;
        store.load(path);

        for (int i = 0; i < 3; ++i) {
            QVariantMap g;
            g[QStringLiteral("name")] = QStringLiteral("G%1").arg(i);
            g[QStringLiteral("modelId")] = QStringLiteral("m");
            g[QStringLiteral("apiKey")] = QStringLiteral("k");
            g[QStringLiteral("baseUrl")] = QStringLiteral("http://x.com");
            store.addGroup(g);
        }

        store.setCurrentGroupIndex(2);
        QCOMPARE(store.currentGroupIndex(), 2);

        auto config = store.proxyConfig();
        QCOMPARE(config.currentGroupIndex, 2);
    }
};

QTEST_MAIN(TestConfigStore)
#include "tst_config_store.moc"
