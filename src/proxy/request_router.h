#pragma once
#include <QString>
#include <QList>
#include <optional>

struct Route {
    QString pathPattern;
    QString inboundProtocol;
    QString provider;
};

class RequestRouter {
public:
    void registerDefaults();
    void addRoute(const Route& route);
    std::optional<Route> match(const QString& method, const QString& path) const;

private:
    struct InternalRoute {
        QString method = QStringLiteral("POST"); // HTTP method ("POST", "GET", or "*" for any)
        QString pathPrefix;  // URL path prefix for matching
        bool wildcard = false; // true if pathPattern ends with "*"
        Route route;
    };
    QList<InternalRoute> m_routes;
};
