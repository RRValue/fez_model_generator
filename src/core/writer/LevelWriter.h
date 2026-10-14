#pragma once

#include "model/Level.h"

#include "writer/Writer.h"

#include <QtCore/QString>

namespace fez
{

class LevelWriter : public Writer
{
public:
    using Writer::Writer;

    void writeLevel(const Level& level);

private:
    // One mesh, one material and one node per instance (no deduplication); nothing when the mesh gets no id.
    void addInstance(const Geometry& geometry, const QString& name, const Transform& transform);
};

} // namespace fez
