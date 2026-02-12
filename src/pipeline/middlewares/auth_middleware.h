#pragma once
#include "pipeline/middleware.h"

class AuthMiddleware : public IPipelineMiddleware {
public:
    explicit AuthMiddleware(const QString& authKey = {}) : m_authKey(authKey) {}
    QString name() const override { return "auth"; }
    Result<SemanticRequest> onRequest(SemanticRequest request) override;

private:
    QString m_authKey;
};
