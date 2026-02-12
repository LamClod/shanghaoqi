#pragma once
#include "openai.h"

class OpenAICompatOutbound : public OpenAIOutbound {
public:
    OpenAICompatOutbound(const QString& id, const QString& defaultBaseUrl,
                         const QString& defaultMiddleRoute = QStringLiteral("/v1"));
    ~OpenAICompatOutbound() override = default;

    QString adapterId() const override;
    Result<ProviderRequest> buildRequest(const SemanticRequest& request) override;

private:
    QString m_id;
    QString m_defaultBaseUrl;
    QString m_defaultMiddleRoute;
};
