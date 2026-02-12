#pragma once
#include "types.h"
#include <QString>
#include <QByteArray>
#include <QJsonObject>

struct MediaRef {
    QString mimeType;
    QString uri;
    QByteArray inlineData;
};

struct Segment {
    SegmentKind kind = SegmentKind::Text;
    QString text;
    MediaRef media;
    QJsonObject structured;
    QString intentTag;

    static Segment fromText(const QString& text) {
        return Segment{SegmentKind::Text, text, {}, {}, {}};
    }
    static Segment fromMedia(const MediaRef& ref) {
        return Segment{SegmentKind::Media, {}, ref, {}, {}};
    }
};
