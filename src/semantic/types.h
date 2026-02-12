#pragma once
#include <QtGlobal>

enum class TaskKind : quint8 {
    Conversation, Embedding, Ranking, ImageGeneration
};

enum class FrameType : quint8 {
    Started, Delta, ActionDelta, UsageDelta, Finished, Failed
};

enum class SegmentKind : quint8 {
    Text, Media, Structured, Redacted
};

enum class ErrorKind : quint8 {
    InvalidInput,    // 400
    Unauthorized,    // 401
    Forbidden,       // 403
    RateLimited,     // 429  (可重试)
    Unavailable,     // 503  (可重试)
    Timeout,         // 504  (可重试)
    NotSupported,    // 501
    Internal         // 500
};

enum class StopCause : quint8 {
    Completed, Length, ContentFilter, ToolCall
};

enum class StreamMode : quint8 {
    FollowClient = 0, ForceOn = 1, ForceOff = 2
};
