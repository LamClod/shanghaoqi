#pragma once
#include "types.h"
#include <QString>
#include <QJsonObject>

struct DomainFailure {
    ErrorKind   kind = ErrorKind::Internal;
    QString     code;
    QString     message;
    bool        retryable = false;
    bool        temporary = false;

    int httpStatus() const;
    QJsonObject toJson() const;

    static DomainFailure invalidInput(const QString& code, const QString& msg);
    static DomainFailure unauthorized(const QString& msg);
    static DomainFailure notSupported(const QString& code, const QString& msg);
    static DomainFailure unavailable(const QString& msg);
    static DomainFailure timeout(const QString& msg);
    static DomainFailure rateLimited(const QString& msg);
    static DomainFailure internal(const QString& msg);
};
