#include "request_router.h"
#include "core/log_manager.h"

void RequestRouter::registerDefaults()
{
    m_routes.clear();

    // POST /v1/chat/completions -> openai inbound
    addRoute({QStringLiteral("/v1/chat/completions"),
              QStringLiteral("openai"), QString()});

    // POST /v1/messages -> anthropic inbound
    addRoute({QStringLiteral("/v1/messages"),
              QStringLiteral("anthropic"), QString()});

    // POST /v1/responses -> openai.responses inbound
    addRoute({QStringLiteral("/v1/responses"),
              QStringLiteral("openai.responses"), QString()});

    // POST /gemini/v1beta/models/* -> gemini inbound (wildcard)
    addRoute({QStringLiteral("/gemini/v1beta/models/*"),
              QStringLiteral("gemini"), QString()});

    // GET /v1/models -> openai inbound
    addRoute({QStringLiteral("/v1/models"),
              QStringLiteral("openai"), QString()});

    // POST /v1/embeddings -> openai inbound
    addRoute({QStringLiteral("/v1/embeddings"),
              QStringLiteral("openai"), QString()});

    // POST /v1/rerank -> openai inbound
    addRoute({QStringLiteral("/v1/rerank"),
              QStringLiteral("openai"), QString()});

    // POST /v1/audio -> openai inbound
    addRoute({QStringLiteral("/v1/audio"),
              QStringLiteral("openai"), QString()});

    LOG_INFO(QStringLiteral("RequestRouter: registered %1 default routes")
                 .arg(m_routes.size()));
}

void RequestRouter::addRoute(const Route& route)
{
    InternalRoute entry;
    entry.route = route;

    // Determine the HTTP method from well-known path patterns
    // GET is only used for model listing; everything else is POST
    if (route.pathPattern == QStringLiteral("/v1/models")) {
        entry.method = QStringLiteral("GET");
    } else {
        entry.method = QStringLiteral("POST");
    }

    // Handle wildcard paths: "/some/prefix/*"
    if (route.pathPattern.endsWith(QLatin1Char('*'))) {
        entry.wildcard = true;
        entry.pathPrefix = route.pathPattern.left(route.pathPattern.size() - 1);
    } else {
        entry.wildcard = false;
        entry.pathPrefix = route.pathPattern;
    }

    m_routes.append(entry);
}

std::optional<Route> RequestRouter::match(const QString& method, const QString& path) const
{
    const QString normalizedMethod = method.trimmed().toUpper();

    for (const InternalRoute& entry : m_routes) {
        // Method check: entry.method must match, or "*" matches any method
        if (entry.method != QStringLiteral("*") && entry.method != normalizedMethod) {
            continue;
        }

        // Path check: wildcard routes use startsWith; exact routes require equality
        if (entry.wildcard) {
            if (path.startsWith(entry.pathPrefix)) {
                return entry.route;
            }
        } else {
            if (path == entry.pathPrefix) {
                return entry.route;
            }
        }
    }

    return std::nullopt;
}
