#include "writer/LevelWriter.h"

#include "export/Transforms.h"

#include <QtCore/QString>

namespace fez
{

void LevelWriter::addInstance(const Geometry& geometry, const QString& name, const Transform& transform)
{
    const auto mesh_id = addGeometry(geometry);

    if(!mesh_id)
        return;

    m_Builder.addNode(name, transform, *mesh_id);
}

void LevelWriter::writeLevel(const Level& level)
{
    m_SaveName = level.m_LevelName;

    for(const auto& ao : level.m_ArtObjects)
    {
        const auto ao_geom_find_iter = level.m_ArtObjectGeometries.find(ao.m_Name);

        if(ao_geom_find_iter == level.m_ArtObjectGeometries.cend())
            continue;

        addInstance(ao_geom_find_iter->second, ao.m_Name,
                    {.m_Position = withLevelObjectOffset(ao.m_Position), .m_Rotation = ao.m_Rotation, .m_Scale = ao.m_Scale});
    }

    for(const auto& te : level.m_TrileEmplacements)
    {
        const auto trile_geom_find_iter = level.m_TrileGeometries.find(te.m_Id);

        if(trile_geom_find_iter == level.m_TrileGeometries.cend())
            continue;

        addInstance(trile_geom_find_iter->second, QString::number(te.m_Id),
                    {.m_Position = te.m_Position, .m_Rotation = trileOrientation(static_cast<int>(te.m_Orientation))});
    }

    for(const auto& bp : level.m_BackgroundPlanes)
        addInstance(bp.m_Geometry, bp.m_Name,
                    {.m_Position = withLevelObjectOffset(bp.m_Position), .m_Rotation = bp.m_Rotation, .m_Scale = bp.m_Scale});

    for(const auto& car : level.m_Characters)
        addInstance(car.m_Geometry, car.m_Name, {.m_Position = car.m_Position});

    save();
}

} // namespace fez
