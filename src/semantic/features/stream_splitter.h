#pragma once
#include "semantic/response.h"
#include "semantic/frame.h"
#include <QList>

class StreamSplitter {
public:
    explicit StreamSplitter(int chunkSize = 20);
    QList<StreamFrame> split(const SemanticResponse& response);

private:
    int m_chunkSize;
};
