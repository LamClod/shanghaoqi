#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QSslConfiguration>

#include "platform/platform_factory.h"
#include "adapters/inbound/multi_router.h"
#include "adapters/inbound/openai_chat.h"
#include "adapters/inbound/openai_responses.h"
#include "adapters/inbound/anthropic.h"
#include "adapters/inbound/gemini.h"
#include "adapters/inbound/aisdk.h"
#include "adapters/inbound/jina.h"
#include "adapters/inbound/codex.h"
#include "adapters/inbound/claudecode.h"
#include "adapters/inbound/antigravity.h"
#include "adapters/outbound/multi_router.h"
#include "adapters/outbound/openai.h"
#include "adapters/outbound/anthropic.h"
#include "adapters/outbound/gemini.h"
#include "adapters/outbound/zai.h"
#include "adapters/outbound/bailian.h"
#include "adapters/outbound/modelscope.h"
#include "adapters/outbound/codex.h"
#include "adapters/outbound/claudecode.h"
#include "adapters/outbound/antigravity.h"
#include "adapters/outbound/openai_compat.h"
#include "adapters/executor/qt_executor.h"
#include "adapters/capability/static_resolver.h"
#include "pipeline/pipeline.h"
#include "pipeline/middlewares/auth_middleware.h"
#include "pipeline/middlewares/model_mapping_middleware.h"
#include "pipeline/middlewares/stream_mode_middleware.h"
#include "pipeline/middlewares/debug_middleware.h"
#include "proxy/proxy_server.h"
#include "config/config_store.h"
#include "core/log_manager.h"
#include "core/bootstrap.h"
#include "ui/main_widget.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("shanghaoqi"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("ShangHaoQi"));

    // --- 1. Platform ---
    auto factory = PlatformFactory::create();
    auto certMgr = factory->createCertManager();
    auto hostsMgr = factory->createHostsManager();
    auto privMgr = factory->createPrivilegeManager();

    if (!privMgr->isRunningAsAdmin()) {
        privMgr->restartAsAdmin(app.applicationFilePath());
        return 0;
    }

    // --- 2. Data directory ---
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString configPath = dataDir + QStringLiteral("/config.json");
    QString logDir = dataDir + QStringLiteral("/logs");
    QDir().mkpath(logDir);

    // --- 3. Config + Log ---
    LogManager::instance().initialize(logDir);
    LOG_INFO(QStringLiteral("涓婂彿鍣?v1.0.0 鍚姩"));

    ConfigStore configStore;
    if (!configStore.load(configPath)) {
        configStore.save();
    }

    auto proxyConf = configStore.proxyConfig();

    // --- 4. Connection pool + Executor ---
    auto& proxyServer = *new ProxyServer(&app);
    ConnectionPool& connPool = proxyServer.connectionPool();

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(proxyConf.runtime.disableSslStrict
                                    ? QSslSocket::VerifyNone
                                    : QSslSocket::AutoVerifyPeer);
    sslConfig.setProtocol(proxyConf.runtime.enableHttp2
                              ? QSsl::TlsV1_2OrLater
                              : QSsl::TlsV1_2);
    QtExecutor executor(connPool, sslConfig);
    executor.setRequestTimeout(proxyConf.runtime.requestTimeout);
    executor.setConnectionTimeout(proxyConf.runtime.connectionTimeout);

    // --- 5. Inbound adapters ---
    auto inOAI  = std::make_unique<OpenAIChatAdapter>();
    auto inResp = std::make_unique<OpenAIResponsesAdapter>();
    auto inAnth = std::make_unique<AnthropicAdapter>();
    auto inGem  = std::make_unique<GeminiAdapter>();
    auto inSdk  = std::make_unique<AiSdkAdapter>();
    auto inJina = std::make_unique<JinaAdapter>(inOAI.get());

    auto* rawOAI  = inOAI.get();
    auto* rawResp = inResp.get();
    auto* rawAnth = inAnth.get();

    auto inCodex = std::make_unique<CodexAdapter>(rawOAI, rawResp);
    auto inCC    = std::make_unique<ClaudeCodeAdapter>(rawAnth);
    auto inAG    = std::make_unique<AntigravityAdapter>(rawOAI, rawResp);

    auto inRouter = std::make_unique<InboundMultiRouter>();
    inRouter->registerAdapter(std::move(inOAI));
    inRouter->registerAdapter(std::move(inResp));
    inRouter->registerAdapter(std::move(inAnth));
    inRouter->registerAdapter(std::move(inGem));
    inRouter->registerAdapter(std::move(inSdk));
    inRouter->registerAdapter(std::move(inJina));
    inRouter->registerAdapter(std::move(inCodex));
    inRouter->registerAdapter(std::move(inCC));
    inRouter->registerAdapter(std::move(inAG));

    // --- 6. Outbound adapters ---
    auto outOAI  = std::make_unique<OpenAIOutbound>();
    auto outAnth = std::make_unique<AnthropicOutbound>();
    auto outGem  = std::make_unique<GeminiOutbound>();
    auto outZai  = std::make_unique<ZaiOutbound>();
    auto outBL   = std::make_unique<BailianOutbound>();
    auto outMS   = std::make_unique<ModelScopeOutbound>();
    auto outCodex = std::make_unique<CodexOutbound>();

    auto* rawOutAnth = outAnth.get();
    auto outCC   = std::make_unique<ClaudeCodeOutbound>(rawOutAnth);
    auto outAG   = std::make_unique<AntigravityOutbound>();

    auto outOR   = std::make_unique<OpenAICompatOutbound>(
        QStringLiteral("openrouter"), QStringLiteral("https://openrouter.ai/api"), QStringLiteral("/v1"));
    auto outXAI  = std::make_unique<OpenAICompatOutbound>(
        QStringLiteral("xai"), QStringLiteral("https://api.x.ai"), QStringLiteral("/v1"));
    auto outDS   = std::make_unique<OpenAICompatOutbound>(
        QStringLiteral("deepseek"), QStringLiteral("https://api.deepseek.com"), QStringLiteral("/v1"));
    auto outDB   = std::make_unique<OpenAICompatOutbound>(
        QStringLiteral("doubao"), QStringLiteral("https://ark.cn-beijing.volces.com/api"), QStringLiteral("/v3"));
    auto outMoon = std::make_unique<OpenAICompatOutbound>(
        QStringLiteral("moonshot"), QStringLiteral("https://api.moonshot.cn"), QStringLiteral("/v1"));

    auto outRouter = std::make_unique<OutboundMultiRouter>();
    outRouter->registerAdapter(std::move(outOAI));
    outRouter->registerAdapter(std::move(outAnth));
    outRouter->registerAdapter(std::move(outGem));
    outRouter->registerAdapter(std::move(outZai));
    outRouter->registerAdapter(std::move(outBL));
    outRouter->registerAdapter(std::move(outMS));
    outRouter->registerAdapter(std::move(outCodex));
    outRouter->registerAdapter(std::move(outCC));
    outRouter->registerAdapter(std::move(outAG));
    outRouter->registerAdapter(std::move(outOR));
    outRouter->registerAdapter(std::move(outXAI));
    outRouter->registerAdapter(std::move(outDS));
    outRouter->registerAdapter(std::move(outDB));
    outRouter->registerAdapter(std::move(outMoon));

    // --- 7. Capability resolver ---
    auto capResolver = std::make_unique<StaticCapabilityResolver>();

    // --- 8. Pipeline ---
    auto* rawInRouter  = inRouter.get();
    auto* rawOutRouter = outRouter.get();
    auto* rawCap       = capResolver.get();

    Pipeline pipeline(rawInRouter, rawOutRouter, &executor, rawCap, &app);

    Policy runtimePolicy;
    runtimePolicy.setDefaultMaxAttempts(qMax(1, proxyConf.currentGroup().maxRetryAttempts));
    pipeline.setPolicy(&runtimePolicy);

    pipeline.addMiddleware(std::make_unique<AuthMiddleware>(
        proxyConf.global.authKey));

    pipeline.addMiddleware(std::make_unique<ModelMappingMiddleware>(
        proxyConf.currentGroup().name,
        proxyConf.currentGroup().modelId));

    pipeline.addMiddleware(std::make_unique<StreamModeMiddleware>(
        proxyConf.runtime.upstreamStreamMode,
        proxyConf.runtime.downstreamStreamMode));

    pipeline.addMiddleware(std::make_unique<DebugMiddleware>(
        proxyConf.runtime.debugMode));

    // --- 9. Proxy server ---
    proxyServer.setPipeline(&pipeline);

    // --- 10. Bootstrap ---
    Bootstrap bootstrap(&app);
    bootstrap.setConfig(&configStore);
    bootstrap.setProxy(&proxyServer);
    bootstrap.setCertManager(certMgr.get());
    bootstrap.setHostsManager(hostsMgr.get());
    bootstrap.setPrivilegeManager(privMgr.get());

    // --- 11. UI ---
    MainWidget mainWidget(&bootstrap, &configStore, &LogManager::instance());
    mainWidget.show();

    LOG_INFO(QStringLiteral("应用程序初始化完成"));

    return app.exec();
}

