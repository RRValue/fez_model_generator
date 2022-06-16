#pragma once

#include "model/Level.h"

#include "writer/Writer.h"

class LevelWriter : public Writer
{
public:
    using Writer::Writer;

    void writeLevel(const Level& level);
};
