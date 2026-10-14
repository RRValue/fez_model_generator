#include "level/ResolveLevel.h"

#include "geometry/TexturedQuad.h"
#include "repository/AssetRepository.h"

#include <QtCore/QString>

#include <string>
#include <utility>

namespace fez
{

namespace
{
    // UV rectangle of a plane/NPC quad: the first frame of an animated texture, otherwise the whole texture.
    UvRect firstFrameUv(const Texture& texture)
    {
        if(texture.m_IsAnimated)
            return {texture.m_TextureAnimationOffsets[0].m_Offset, texture.m_TextureAnimationOffsets[0].m_Size};

        return {Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}};
    }

    // Puts a quad for the texture of the geometry into it and names the geometry after that texture.
    void attachQuad(Geometry& geometry, Vec2f size, float depthOffset)
    {
        Geometry quad = makeTexturedQuad(size, firstFrameUv(geometry.m_Texture), depthOffset);

        geometry.m_IsPlane = quad.m_IsPlane;
        geometry.m_Vertices = std::move(quad.m_Vertices);
        geometry.m_Indices = std::move(quad.m_Indices);

        geometry.m_Name = geometry.m_Texture.m_TextureName;
    }

    std::expected<Level::TrileGeometries, Error> resolveTriles(const LevelDescription& description, AssetRepository& repository)
    {
        // <input>/trile sets/<name>.xml; a failed set and a set without triles both fail the level
        const auto loaded_trile_set = repository.trileSet(description.m_TrileSetName);

        if(!loaded_trile_set)
            return std::unexpected(loaded_trile_set.error());

        const auto& trile_set = (*loaded_trile_set)->m_Triles;

        if(trile_set.empty())
            return std::unexpected(Error{ErrorKind::BadValue, "trile set has no triles",
                                         (repository.inputRoot() + "/trile sets/" + description.m_TrileSetName + ".xml").toStdString()});

        Level::TrileGeometries result;

        for(const auto& emplacement : description.m_TrileEmplacements)
        {
            if(result.find(emplacement.m_Id) != result.cend())
                continue;

            // \todo what is -1
            if(emplacement.m_Id == -1)
                continue;

            const auto find_result = trile_set.find(emplacement.m_Id);

            if(find_result == trile_set.cend())
                return std::unexpected(Error{ErrorKind::BadValue,
                                             "trile id " + std::to_string(emplacement.m_Id) + " is not in trile set " + description.m_TrileSetName.toStdString(),
                                             "Level/Triles"});

            result.insert(*find_result);
        }

        return result;
    }

    std::expected<Level::ArtObjectGeometries, Error> resolveArtObjects(const LevelDescription& description, AssetRepository& repository)
    {
        Level::ArtObjectGeometries result;

        for(const auto& instance : description.m_ArtObjects)
        {
            if(result.find(instance.m_Name) != result.cend())
                continue;

            // <input>/art objects/<name>.xml
            const auto art_object = repository.artObject(instance.m_Name);

            if(!art_object)
                return std::unexpected(art_object.error());

            result.insert({instance.m_Name, **art_object});
        }

        return result;
    }

    std::expected<Level::BackgroundPlanes, Error> resolveBackgroundPlanes(const LevelDescription& description, AssetRepository& repository)
    {
        auto result = Level::BackgroundPlanes();
        result.reserve(description.m_BackgroundPlanes.size());

        // all textures first, then the quads (as before)
        for(const auto& plane : description.m_BackgroundPlanes)
        {
            auto background_plane = BackgroundPlane();

            background_plane.m_Name = plane.m_TextureName;

            // <input>/background planes/<name>; the repository key ignores animated (as the old cache key did)
            const auto texture = repository.texture("background planes", background_plane.m_Name, plane.m_Animated);

            if(!texture)
                return std::unexpected(texture.error());

            background_plane.m_Geometry.m_Texture = **texture;

            background_plane.m_Geometry.m_Opacity = plane.m_Opacity;
            background_plane.m_Geometry.m_DoubleSided = plane.m_DoubleSided;

            background_plane.m_Position = plane.m_Position;
            background_plane.m_Rotation = plane.m_Rotation;
            background_plane.m_Scale = plane.m_Scale;

            result.push_back(std::move(background_plane));
        }

        for(auto& background_plane : result)
        {
            const auto& texture = background_plane.m_Geometry.m_Texture;

            // Integer division kept on purpose (fixed in Task 28).
            const auto geom_w = float(texture.m_Width / 16);
            const auto geom_h = float(texture.m_Height / 16);

            attachQuad(background_plane.m_Geometry, Vec2f{geom_w, geom_h}, 0.05f);
        }

        return result;
    }

    std::expected<Level::Characters, Error> resolveCharacters(const LevelDescription& description, AssetRepository& repository)
    {
        auto result = Level::Characters();
        result.reserve(description.m_Npcs.size());

        // all textures first, then the quads (as before)
        for(const auto& npc : description.m_Npcs)
        {
            auto character = Character();

            character.m_Name = npc.m_Name;

            // parseLevel guarantees at least one action; the first one names the texture
            if(npc.m_Actions.empty())
                return std::unexpected(Error{ErrorKind::MissingElement, "NPC " + npc.m_Name.toStdString() + " has no actions",
                                             "Level/NonplayerCharacters/Entry/NpcInstance/Actions/Action"});

            // <input>/character animations/<npc>/<action>
            const auto& action_name = npc.m_Actions[0].second;

            const auto texture = repository.texture("character animations", character.m_Name + "/" + action_name, true);

            if(!texture)
                return std::unexpected(texture.error());

            character.m_Geometry.m_Texture = **texture;

            character.m_Position = npc.m_Position;

            result.push_back(std::move(character));
        }

        for(auto& character : result)
        {
            const auto& texture = character.m_Geometry.m_Texture;

            const auto geom_w = (float)texture.m_Width / float(16);
            const auto geom_h = (float)texture.m_Height / float(16);

            attachQuad(character.m_Geometry, Vec2f{geom_w, geom_h}, 0.0f);
        }

        return result;
    }
} // namespace

std::expected<Level, Error> resolveLevel(const LevelDescription& description, AssetRepository& repository)
{
    // Size
    // StartingPosition
    // Volumes
    // Scripts
    // Triles
    auto trile_geometries = resolveTriles(description, repository);

    if(!trile_geometries)
        return std::unexpected(std::move(trile_geometries.error()));

    // ArtObjects
    auto art_object_geometries = resolveArtObjects(description, repository);

    if(!art_object_geometries)
        return std::unexpected(std::move(art_object_geometries.error()));

    // BackgroundPlanes
    auto background_planes = resolveBackgroundPlanes(description, repository);

    if(!background_planes)
        return std::unexpected(std::move(background_planes.error()));

    // Groups
    // NonplayerCharacters
    auto characters = resolveCharacters(description, repository);

    if(!characters)
        return std::unexpected(std::move(characters.error()));

    // Paths
    // MutedLoops
    // AmbienceTracks

    Level result;

    result.m_TrileSetName = description.m_TrileSetName;
    result.m_LevelName = description.m_Name;

    result.m_TrileEmplacements = description.m_TrileEmplacements;
    result.m_TrileGeometries = std::move(*trile_geometries);

    result.m_ArtObjects = description.m_ArtObjects;
    result.m_ArtObjectGeometries = std::move(*art_object_geometries);

    result.m_BackgroundPlanes = std::move(*background_planes);

    result.m_Characters = std::move(*characters);

    return result;
}

} // namespace fez
