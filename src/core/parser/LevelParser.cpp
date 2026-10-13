#include "parser/LevelParser.h"

#include "geometry/TexturedQuad.h"

#include "parser/level/LevelReader.h"
#include "repository/AssetRepository.h"

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
} // namespace

LevelParser::LevelParser(AssetRepository& repository)
    : m_Repository(repository)
{
}

LevelParser::LevelResult LevelParser::parse(const QString& path) noexcept
{
    // header, Triles, ArtObjects, BackgroundPlanes, NonplayerCharacters (logs "parsing:" and XML syntax errors)
    auto description = parseLevel(path);

    if(!description)
        return {};

    Level result;

    result.m_TrileSetName = description->m_TrileSetName;
    result.m_LevelName = description->m_Name;

    // Size
    // StartingPosition
    // Volumes
    // Scripts
    // Triles
    auto trile_geometries = parseTrileEmplacements(description->m_TrileEmplacements, result.m_TrileSetName);

    if(!trile_geometries)
        return {};

    // ArtObjects
    auto art_object_geometries = parseArtObjects(description->m_ArtObjects);

    if(!art_object_geometries)
        return {};

    // BackgroundPlanes
    auto background_planes = loadBackgroundPlaneTextures(description->m_BackgroundPlanes);

    if(!background_planes)
        return {};

    background_planes = parseBackgroundPlanes(*background_planes);

    if(!background_planes)
        return {};

    // Groups
    // NonplayerCharacters
    auto characters = loadCharacterTextures(description->m_Npcs);

    if(!characters)
        return {};

    characters = parseCharacters(*characters);

    if(!characters)
        return {};
    // Paths
    // MutedLoops
    // AmbienceTracks

    result.m_TrileEmplacements = std::move(description->m_TrileEmplacements);
    result.m_TrileGeometries = std::move(*trile_geometries);

    result.m_ArtObjects = std::move(description->m_ArtObjects);
    result.m_ArtObjectGeometries = std::move(*art_object_geometries);

    result.m_BackgroundPlanes = std::move(*background_planes);

    result.m_Characters = std::move(*characters);

    return result;
}

LevelParser::TrileGeometriesResult LevelParser::parseTrileEmplacements(const Level::TrileEmplacements& emplacements, const QString& trileSetName)
{
    // <input>/trile sets/<name>.xml; a failed set and a set without triles both fail the level
    const auto loaded_trile_set = m_Repository.trileSet(trileSetName);

    if(!loaded_trile_set || (*loaded_trile_set)->m_Triles.empty())
        return {};

    const auto& trile_set = (*loaded_trile_set)->m_Triles;

    Level::TrileGeometries result;

    for(const auto& emplacement : emplacements)
    {
        if(result.find(emplacement.m_Id) != result.cend())
            continue;

        // \todo what is -1
        if(emplacement.m_Id == -1)
            continue;

        const auto find_result = trile_set.find(emplacement.m_Id);

        if(find_result == trile_set.cend())
            return {};

        result.insert(*find_result);
    }

    return result;
}

LevelParser::ArtObjectGeometriesResult LevelParser::parseArtObjects(const Level::ArtObjects& artObjects)
{
    Level::ArtObjectGeometries result;

    for(const auto& artObject : artObjects)
    {
        if(result.find(artObject.m_Name) != result.cend())
            continue;

        // <input>/art objects/<name>.xml
        const auto art_object = m_Repository.artObject(artObject.m_Name);

        if(!art_object)
            return {};

        result.insert({artObject.m_Name, **art_object});
    }

    return result;
}

LevelParser::BackgroundPlanesResult LevelParser::loadBackgroundPlaneTextures(const LevelDescription::BackgroundPlanes& backgroundPlanes)
{
    auto result = Level::BackgroundPlanes();
    result.reserve(backgroundPlanes.size());

    for(const auto& description : backgroundPlanes)
    {
        auto background_plane = BackgroundPlane();

        background_plane.m_Name = description.m_TextureName;

        // <input>/background planes/<name>; the repository key ignores animated (as the old cache key did)
        const auto texture = m_Repository.texture("background planes", background_plane.m_Name, description.m_Animated);

        if(!texture)
            return {};

        background_plane.m_Geometry.m_Texture = **texture;

        background_plane.m_Geometry.m_Opacity = description.m_Opacity;
        background_plane.m_Geometry.m_DoubleSided = description.m_DoubleSided;

        background_plane.m_Position = description.m_Position;
        background_plane.m_Rotation = description.m_Rotation;
        background_plane.m_Scale = description.m_Scale;

        result.push_back(std::move(background_plane));
    }

    return result;
}

LevelParser::BackgroundPlanesResult LevelParser::parseBackgroundPlanes(const Level::BackgroundPlanes& backgroundPlanes)
{
    Level::BackgroundPlanes result;

    for(const auto& backgroundPlane : backgroundPlanes)
    {
        const auto& texture = backgroundPlane.m_Geometry.m_Texture;

        // Integer division kept on purpose (fixed in Task 28).
        const auto geom_w = float(texture.m_Width / 16);
        const auto geom_h = float(texture.m_Height / 16);

        auto back_ground_plane = backgroundPlane;

        Geometry quad = makeTexturedQuad(Vec2f{geom_w, geom_h}, firstFrameUv(texture), 0.05f);

        back_ground_plane.m_Geometry.m_IsPlane = quad.m_IsPlane;
        back_ground_plane.m_Geometry.m_Vertices = std::move(quad.m_Vertices);
        back_ground_plane.m_Geometry.m_Indices = std::move(quad.m_Indices);

        back_ground_plane.m_Geometry.m_Name = back_ground_plane.m_Geometry.m_Texture.m_TextureName;

        result.push_back(back_ground_plane);
    }

    return result;
}

LevelParser::CharactersResult LevelParser::loadCharacterTextures(const LevelDescription::Npcs& npcs)
{
    auto result = Level::Characters();
    result.reserve(npcs.size());

    for(const auto& npc : npcs)
    {
        auto character = Character();

        character.m_Name = npc.m_Name;

        // parseLevel guarantees at least one action; the first one names the texture
        if(npc.m_Actions.empty())
            return {};

        // <input>/character animations/<npc>/<action>
        const auto& action_name = npc.m_Actions[0].second;

        const auto texture = m_Repository.texture("character animations", character.m_Name + "/" + action_name, true);

        if(!texture)
            return {};

        character.m_Geometry.m_Texture = **texture;

        character.m_Position = npc.m_Position;

        result.push_back(std::move(character));
    }

    return result;
}

LevelParser::CharactersResult LevelParser::parseCharacters(const Level::Characters& characters)
{
    Level::Characters result;

    for(const auto& character : characters)
    {
        const auto& texture = character.m_Geometry.m_Texture;

        const auto geom_w = (float)texture.m_Width / float(16);
        const auto geom_h = (float)texture.m_Height / float(16);

        auto character_res = character;

        Geometry quad = makeTexturedQuad(Vec2f{geom_w, geom_h}, firstFrameUv(texture), 0.0f);

        character_res.m_Geometry.m_IsPlane = quad.m_IsPlane;
        character_res.m_Geometry.m_Vertices = std::move(quad.m_Vertices);
        character_res.m_Geometry.m_Indices = std::move(quad.m_Indices);

        character_res.m_Geometry.m_Name = character_res.m_Geometry.m_Texture.m_TextureName;

        result.push_back(character_res);
    }

    return result;
}

} // namespace fez
