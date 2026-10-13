#pragma once

#include "model/Level.h"

#include "writer/Writer.h"

namespace fez
{

class LevelWriter : public Writer
{
public:
    using Writer::Writer;

    void writeLevel(const Level& level);
};

} // namespace fez
