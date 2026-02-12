#include "bootstrap.h"
#include "log_manager.h"
#include "config/config_store.h"
#include "config/model_list_utils.h"
#include "config/model_list_request_builder.h"
#include "config/provider_routing.h"
#include "proxy/proxy_server.h"
#include "platform/interfaces/i_cert_manager.h"
#include "platform/interfaces/i_hosts_manager.h"
#include "platform/interfaces/i_privilege_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QSet>
#include <memory>

namespace {

using provider_routing::ModelListProvider;

}

Bootstrap::Bootstrap(QObject* parent)
    : QObject(parent)
{
}

bool Bootstrap::isProxyRunning() const {
    return m_proxy && m_proxy->isRunning();
}

Bootstrap::CertAction Bootstrap::decideCertAction() {
    if (!m_certManager || !m_config)
        return CertAction::Generate;

    QString certPath = m_config->proxyConfig().certPath;
    if (certPath.isEmpty())
        return CertAction::Generate;

    if (!m_certManager->isCaCertInstalled(certPath))
        return CertAction::Reinstall;

    QDateTime expiry = m_certManager->getCertExpiry(certPath);
    if (expiry.isValid() && expiry < QDateTime::currentDateTime().addDays(30))
        return CertAction::CleanAndRegen;

    return CertAction::None;
}

void Bootstrap::startAll() {
    LOG_INFO(QStringLiteral("========== 开始启动服务 =========="));

    if (!m_config || !m_proxy || !m_certManager || !m_hostsManager) {
        emit stepProgress("init", false, "组件未初始化");
        emit proxyStatusChanged(false);
        return;
    }

    auto config = m_config->proxyConfig();
    if (config.groups.isEmpty()) {
        emit stepProgress("validate", false, "没有配置组");
        emit proxyStatusChanged(false);
        return;
    }

    // Auto-derive hijack domains from config groups' provider fields
    QStringList hijackDomains;
    for (const auto& g : config.groups) {
        QString domain = provider_routing::canonicalHijackDomain(g);
        if (!domain.isEmpty() && !hijackDomains.contains(domain, Qt::CaseInsensitive))
            hijackDomains.append(domain);
    }
    if (hijackDomains.isEmpty()) {
        emit stepProgress("validate", false, "没有可劫持的域名（请在配置组中设置入站适配器）");
        emit proxyStatusChanged(false);
        return;
    }
    LOG_INFO(QStringLiteral("劫持域名: %1").arg(hijackDomains.join(", ")));

    // cert
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    QString caCertPath = appData + "/ca.crt";
    QString caKeyPath = appData + "/ca.key";
    QString serverCertPath = appData + "/server.crt";
    QString serverKeyPath = appData + "/server.key";

    auto certAction = decideCertAction();
    if (certAction != CertAction::None) {
        emit stepProgress("cert_generate", true, "正在生成证书...");
        LOG_INFO(QStringLiteral("[1/4] 生成证书..."));

        if (!m_certManager->generateCaCert(caCertPath, caKeyPath)) {
            emit stepProgress("cert_generate", false, "CA 证书生成失败");
            emit proxyStatusChanged(false);
            return;
        }

        if (!m_certManager->generateServerCertForDomains(caCertPath,
                                                          caKeyPath,
                                                          hijackDomains,
                                                          serverCertPath,
                                                          serverKeyPath)) {
            emit stepProgress("cert_generate", false, QStringLiteral("服务器证书生成失败"));
            emit proxyStatusChanged(false);
            return;
        }
        emit stepProgress("cert_generate", true, "证书生成完成");
    }

    // install CA
    emit stepProgress("cert_install", true, "正在安装 CA 证书...");
    LOG_INFO(QStringLiteral("[2/4] 安装 CA 证书..."));
    if (!m_certManager->isCaCertInstalled(caCertPath)) {
        if (!m_certManager->installCaCert(caCertPath)) {
            emit stepProgress("cert_install", false, "CA 证书安装失败");
            emit proxyStatusChanged(false);
            return;
        }
    }
    emit stepProgress("cert_install", true, "CA 证书已安装");

    // hosts
    emit stepProgress("hosts_modify", true, "正在修改 hosts...");
    LOG_INFO(QStringLiteral("[3/4] 修改 hosts 文件..."));
    for (const auto& domain : hijackDomains) {
        if (!m_hostsManager->hasEntry(domain)) {
            if (!m_hostsManager->addEntry("127.0.0.1", domain)) {
                emit stepProgress("hosts_modify", false, QString("hosts 修改失败: %1").arg(domain));
                emit proxyStatusChanged(false);
                return;
            }
        }
    }
    m_hostsManager->flush();
    m_hijackDomains = hijackDomains;
    m_hostsModified = true;
    emit stepProgress("hosts_modify", true, "hosts 修改完成");

    // Set cert paths on config before starting proxy
    config.certPath = serverCertPath;
    config.keyPath = serverKeyPath;

    // proxy
    emit stepProgress("proxy_start", true, "正在启动代理...");
    LOG_INFO(QStringLiteral("[4/4] 启动代理服务器..."));
    if (!m_proxy->start(config)) {
        emit stepProgress("proxy_start", false, "代理启动失败");
        restoreHosts();
        emit proxyStatusChanged(false);
        return;
    }
    emit stepProgress("proxy_start", true, "代理已启动");
    emit proxyStatusChanged(true);

    LOG_INFO(QStringLiteral("========== 所有服务启动成功 =========="));
    LOG_INFO(QStringLiteral("代理监听: 0.0.0.0:%1").arg(config.runtime.proxyPort));
}

void Bootstrap::stopAll() {
    LOG_INFO(QStringLiteral("========== 停止服务 =========="));
    stopProxy();
    restoreHosts();
    LOG_INFO(QStringLiteral("========== 服务已停止 =========="));
}

void Bootstrap::generateCerts() {
    // covered by startAll
}

void Bootstrap::installCaCert() {
    // covered by startAll
}

void Bootstrap::modifyHosts() {
    if (!m_config || !m_hostsManager) return;
    for (const auto& domain : m_hijackDomains) {
        m_hostsManager->addEntry("127.0.0.1", domain);
    }
    m_hostsManager->flush();
    m_hostsModified = true;
}

void Bootstrap::restoreHosts() {
    if (!m_hostsModified || !m_hostsManager) return;
    for (const auto& domain : m_hijackDomains) {
        m_hostsManager->removeEntry(domain);
    }
    m_hostsManager->flush();
    m_hostsModified = false;
    m_hijackDomains.clear();
    LOG_INFO(QStringLiteral("hosts 文件已恢复"));
}

void Bootstrap::startProxy() {
    if (!m_proxy || !m_config) return;
    m_proxy->start(m_config->proxyConfig());
    emit proxyStatusChanged(true);
}

void Bootstrap::stopProxy() {
    if (!m_proxy) return;
    if (m_proxy->isRunning()) {
        m_proxy->stop();
        emit proxyStatusChanged(false);
        LOG_INFO(QStringLiteral("代理服务器已停止"));
    }
}

void Bootstrap::fetchModelList(int groupIndex) {
    if (!m_config) return;
    auto group = m_config->groupAt(groupIndex);
    if (group.apiKey.isEmpty() || group.baseUrl.isEmpty()) {
        emit modelListReady(groupIndex, {});
        return;
    }

    auto* nam = new QNetworkAccessManager(this);

    const auto requestContext = model_list_request_builder::buildContext(group);
    if (!requestContext.isValid()) {
        emit modelListReady(groupIndex, {});
        nam->deleteLater();
        return;
    }

    struct ModelListFetchState : public std::enable_shared_from_this<ModelListFetchState> {
        Bootstrap* owner = nullptr;
        QNetworkAccessManager* nam = nullptr;
        ConfigGroup group;
        int groupIndex = -1;
        model_list_request_builder::Context requestContext;
        int modeIndex = 0;
        QStringList fetchedModels;

        void run() {
            if (!owner || !nam) {
                return;
            }
            if (modeIndex >= requestContext.authModes.size()) {
                emit owner->modelListReady(groupIndex, fetchedModels);
                nam->deleteLater();
                return;
            }

            const QString authMode = requestContext.authModes.at(modeIndex);
            QNetworkRequest req = model_list_request_builder::makeNetworkRequest(
                requestContext, authMode, 15000);
            auto* reply = nam->get(req);

            QObject::connect(reply, &QNetworkReply::finished, owner,
                             [state = shared_from_this(), reply, authMode]() {
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = reply->readAll();
                const bool ok = reply->error() == QNetworkReply::NoError
                                && status >= 200 && status < 300;

                if (ok) {
                    state->fetchedModels = model_list_utils::parseModelIds(body);
                    state->fetchedModels.sort();
                    reply->deleteLater();
                    emit state->owner->modelListReady(state->groupIndex,
                                                      state->fetchedModels);
                    state->nam->deleteLater();
                    return;
                }

                LOG_WARNING(QStringLiteral("Bootstrap: fetchModelList group=%1 auth=%2 status=%3 err=%4")
                                .arg(state->group.name, authMode)
                                .arg(status)
                                .arg(reply->errorString()));

                const bool authFailure = (status == 401 || status == 403);
                const bool canRetry =
                    (state->modeIndex + 1) < state->requestContext.authModes.size();
                reply->deleteLater();
                if (authFailure && canRetry) {
                    ++state->modeIndex;
                    state->run();
                    return;
                }

                emit state->owner->modelListReady(state->groupIndex,
                                                  state->fetchedModels);
                state->nam->deleteLater();
            });
        }
    };

    auto state = std::make_shared<ModelListFetchState>();
    state->owner = this;
    state->nam = nam;
    state->group = group;
    state->groupIndex = groupIndex;
    state->requestContext = requestContext;
    state->run();
}

void Bootstrap::testConfig(int groupIndex) {
    if (!m_config) return;
    auto group = m_config->groupAt(groupIndex);
    if (group.apiKey.trimmed().isEmpty() || group.baseUrl.trimmed().isEmpty()) {
        emit testResult(groupIndex, false, 400,
                        QStringLiteral("missing api key or base url"));
        return;
    }

    auto* nam = new QNetworkAccessManager(this);

    const ModelListProvider provider = provider_routing::detectModelListProvider(group);
    QString baseUrl = group.baseUrl.trimmed();
    QString middleRoute = provider_routing::effectiveMiddleRoute(group, provider);
    if (!middleRoute.isEmpty() && baseUrl.endsWith(middleRoute)) {
        middleRoute.clear();
    }

    QNetworkRequest req;
    QJsonObject body;

    if (provider == ModelListProvider::Anthropic) {
        req = QNetworkRequest(QUrl(baseUrl + middleRoute + QStringLiteral("/messages")));
        req.setRawHeader("x-api-key", group.apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
        body["model"] = group.modelId;
        body["max_tokens"] = 8;

        QJsonArray messages;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "hi";
        messages.append(msg);
        body["messages"] = messages;
    } else if (provider == ModelListProvider::Gemini) {
        QString route = middleRoute.trimmed();
        if (route.isEmpty()) {
            route = QStringLiteral("/v1beta");
        }
        if (!route.startsWith(QLatin1Char('/'))) {
            route.prepend(QLatin1Char('/'));
        }

        QString modelPath = group.modelId.trimmed();
        if (!modelPath.startsWith(QStringLiteral("models/"), Qt::CaseInsensitive)) {
            modelPath.prepend(QStringLiteral("models/"));
        }

        QUrl url(baseUrl + route + QLatin1Char('/') + modelPath + QStringLiteral(":generateContent"));
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("key"), group.apiKey);
        url.setQuery(query);
        req = QNetworkRequest(url);

        QJsonObject part;
        part["text"] = "hi";
        QJsonArray parts;
        parts.append(part);
        QJsonObject content;
        content["parts"] = parts;
        QJsonArray contents;
        contents.append(content);
        body["contents"] = contents;
    } else {
        req = QNetworkRequest(QUrl(baseUrl + middleRoute + QStringLiteral("/chat/completions")));
        req.setRawHeader("Authorization", ("Bearer " + group.apiKey).toUtf8());
        body["model"] = group.modelId;

        QJsonArray messages;
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "hi";
        messages.append(msg);
        body["messages"] = messages;
        body["max_tokens"] = 5;
    }

    for (auto it = group.customHeaders.cbegin(); it != group.customHeaders.cend(); ++it) {
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(15000);

    auto* reply = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, groupIndex, nam]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;

        QString err;
        if (!ok) {
            err = reply->errorString();
            const QString bodyPreview = QString::fromUtf8(body.left(256));
            if (!bodyPreview.trimmed().isEmpty()) {
                err += QStringLiteral(" | ") + bodyPreview;
            }
        }

        emit testResult(groupIndex, ok, status, err);
        reply->deleteLater();
        nam->deleteLater();
    });
}

void Bootstrap::testAllConfigs() {
    if (!m_config) return;
    int total = m_config->groups().size();
    if (total == 0) {
        emit testAllDone({});
        return;
    }

    auto results = std::make_shared<QVariantList>();
    auto pending = std::make_shared<QSet<int>>();
    for (int i = 0; i < total; ++i) {
        pending->insert(i);
    }

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(this, &Bootstrap::testResult, this,
                    [this, total, results, pending, conn](int idx,
                                                          bool success,
                                                          int httpStatus,
                                                          const QString& error) {
        if (idx < 0 || idx >= total || !pending->contains(idx)) {
            return;
        }

        pending->remove(idx);

        QVariantMap r;
        r["index"] = idx;
        r["success"] = success;
        r["http_status"] = httpStatus;
        r["error"] = error;
        results->append(r);

        if (pending->isEmpty()) {
            disconnect(*conn);
            emit testAllDone(*results);
        }
    });

    for (int i = 0; i < total; ++i) {
        testConfig(i);
    }
}
