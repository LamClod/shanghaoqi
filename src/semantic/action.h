#pragma once
#include <QString>
#include <QJsonObject>

struct ActionSpec {
    QString name;
    QString description;
    QJsonObject parameters;
};

struct ActionCall {
    QString callId;
    QString name;
    QString args;
};

struct ActionDelta {
    QString callId;
    QString name;
    QString argsPatch;
};

struct ActionResult {
    QString callId;
    QString output;
};
