#include "writer/GeometryWriter.h"

namespace fez
{

void GeometryWriter::writeObj(const Geometry& geometry)
{
    m_SaveName = geometry.m_Name;
    const auto mesh_id = addGeometry(geometry);

    if(!mesh_id)
        return;

    m_Builder.addNode({}, Transform{}, *mesh_id);

    save();
}

} // namespace fez
