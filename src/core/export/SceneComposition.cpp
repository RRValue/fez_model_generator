#include "export/SceneComposition.h"

#include "export/SceneBuilder.h"
#include "export/Transforms.h"

#include <assimp/scene.h>

namespace fez
{

namespace
{

// Collects the scene and the textures of the meshes that got an id.
class Composer
{
public:
    // One mesh, one material and one node per instance (no deduplication); no node and no texture when the mesh gets
    // no id.
    void addInstance(const Geometry& geometry, const QString& name, const Transform& transform)
    {
        const auto mesh_id = m_Builder.addMesh(geometry);

        if(!mesh_id)
            return;

        m_Textures.push_back({.m_Source = geometry.m_Texture.m_TextureOrgFile, .m_Name = geometry.m_Texture.m_TextureName});
        m_Builder.addNode(name, transform, *mesh_id);
    }

    // every node has exactly one texture entry
    bool hasNodes() const
    {
        return !m_Textures.empty();
    }

    ComposedScene finish()
    {
        return {.m_Scene = ScenePtr(m_Builder.build().release()), .m_Textures = std::move(m_Textures)};
    }

private:
    SceneBuilder m_Builder;
    std::vector<TextureFile> m_Textures;
};

} // namespace

void SceneDeleter::operator()(aiScene* scene) const noexcept
{
    delete scene;
}

std::optional<ComposedScene> buildGeometryScene(const Geometry& geometry)
{
    Composer composer;
    composer.addInstance(geometry, {}, Transform{});

    if(!composer.hasNodes())
        return std::nullopt;

    return composer.finish();
}

ComposedScene buildLevelScene(const Level& level)
{
    Composer composer;

    for(const auto& ao : level.m_ArtObjects)
    {
        const auto ao_geom_find_iter = level.m_ArtObjectGeometries.find(ao.m_Name);

        if(ao_geom_find_iter == level.m_ArtObjectGeometries.cend())
            continue;

        composer.addInstance(ao_geom_find_iter->second, ao.m_Name,
                             {.m_Position = withLevelObjectOffset(ao.m_Position), .m_Rotation = ao.m_Rotation, .m_Scale = ao.m_Scale});
    }

    for(const auto& te : level.m_TrileEmplacements)
    {
        const auto trile_geom_find_iter = level.m_TrileGeometries.find(te.m_Id);

        if(trile_geom_find_iter == level.m_TrileGeometries.cend())
            continue;

        composer.addInstance(trile_geom_find_iter->second, QString::number(te.m_Id),
                             {.m_Position = te.m_Position, .m_Rotation = trileOrientation(te.m_Orientation)});
    }

    for(const auto& bp : level.m_BackgroundPlanes)
        composer.addInstance(bp.m_Geometry, bp.m_Name,
                             {.m_Position = withLevelObjectOffset(bp.m_Position), .m_Rotation = bp.m_Rotation, .m_Scale = bp.m_Scale});

    for(const auto& car : level.m_Characters)
        composer.addInstance(car.m_Geometry, car.m_Name, {.m_Position = car.m_Position});

    return composer.finish();
}

} // namespace fez
