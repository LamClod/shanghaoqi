#include "failure.h"
#include <QJsonDocument>

int DomainFailure::httpStatus() const {
    switch (kind) {
    case ErrorKind::InvalidInput:  return 400;
    case ErrorKind::Unauthorized:  return 401;
    case ErrorKind::Forbidden:     return 403;
    case ErrorKind::RateLimited:   return 429;
    case ErrorKind::NotSupported:  return 501;
    case ErrorKind::Unavailable:   return 503;
    case ErrorKind::Timeout:       return 504;
    case ErrorKind::Internal:
    default:                       return 500;
    }
}

QJsonObject DomainFailure::toJson() const {
    QJsonObject err;
    err["code"] = code;
    err["message"] = message;
    err["type"] = static_cast<int>(kind);
    QJsonObject root;
    root["error"] = err;
    return root;
}

DomainFailure DomainFailure::invalidInput(const QString& code, const QString& msg) {
    return {ErrorKind::InvalidInput, code, msg, false, false};
}

DomainFailure DomainFailure::unauthorized(const QString& msg) {
    return {ErrorKind::Unauthorized, "unauthorized", msg, false, false};
}

DomainFailure DomainFailure::notSupported(const QString& code, const QString& msg) {
    return {ErrorKind::NotSupported, code, msg, false, false};
}

DomainFailure DomainFailure::unavailable(const QString& msg) {
    return {ErrorKind::Unavailable, "unavailable", msg, true, true};
}

DomainFailure DomainFailure::timeout(const QString& msg) {
    return {ErrorKind::Timeout, "timeout", msg, true, true};
}

DomainFailure DomainFailure::rateLimited(const QString& msg) {
    return {ErrorKind::RateLimited, "rate_limited", msg, true, true};
}

DomainFailure DomainFailure::internal(const QString& msg) {
    return {ErrorKind::Internal, "internal", msg, false, false};
}
