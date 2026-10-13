#include "parser/LevelParser.h"

#include "geometry/TexturedQuad.h"

#include "parser/ArtObjectParser.h"
#include "parser/TextureParser.h"
#include "parser/TrileSetParser.h"
#include "parser/level/LevelReader.h"

#include "xml/Xml.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMutexLocker>

#include <QtGui/QImage>

#include <qdebug.h>

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

QMutex LevelParser::sm_TrileSetCacheMutex = {};
LevelParser::TrileSetCache LevelParser::sm_TrileSetCache = {};

QMutex LevelParser::sm_ArtObjectCacheMutex = {};
LevelParser::ArtObjectCache LevelParser::sm_ArtObjectCache = {};

QMutex LevelParser::sm_TextureCacheMutex = {};
LevelParser::TextureCache LevelParser::sm_TextureCache = {};

LevelParser::LevelParser()
{
}

LevelParser::~LevelParser()
{
}

LevelParser::LevelResult LevelParser::parse(const QString& path) noexcept
{
    qDebug() << "parsing: " << path;

    QFileInfo info(path);

    if(!info.exists())
        return {};

    m_Path = info.absolutePath();

    // load
    const auto document = loadXml(path);

    if(!document)
    {
        if(document.error().kind == ErrorKind::XmlSyntax)
            qDebug() << "Error: \"" + QString::fromStdString(document.error().message) + "\" at " + QString::fromStdString(document.error().source);

        return {};
    }

    // header, Triles, ArtObjects
    auto description = readLevel(*document);

    if(!description)
        return {};

    // readLevel succeeded, so the document has a Level element
    const auto level_elem = document->firstChildElement("Level");

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
    auto background_planes = std::move(readBackgroundPlanes(level_elem));

    if(!background_planes)
        return {};

    background_planes = parseBackgroundPlanes(*background_planes);

    if(!background_planes)
        return {};

    // Groups
    // NonplayerCharacters
    auto characters = std::move(readCharacters(level_elem));

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
    // find trile set in cache
    const auto trile_set = [this, &trileSetName]() -> Level::TrileGeometries {
        QMutexLocker locker(&sm_TrileSetCacheMutex);

        const auto trile_set_find_iter = sm_TrileSetCache.find(trileSetName);

        if(trile_set_find_iter != sm_TrileSetCache.cend())
            return trile_set_find_iter->second;

        // load triles
        const auto trile_sets_dir = QDir(m_Path + "/../trile sets");

        if(!trile_sets_dir.exists())
            return {};

        const auto trile_set_path = trile_sets_dir.absolutePath() + "/" + trileSetName + ".xml";

        if(!QFile(trile_set_path).exists())
            return {};

        auto triles_set = parseTrileSet(trile_set_path);

        // a failed set and a set without triles both fail the level (not cached, as before)
        if(!triles_set || triles_set->m_Triles.empty())
            return {};

        sm_TrileSetCache.insert({trileSetName, triles_set->m_Triles});

        return std::move(triles_set->m_Triles);
    }();

    if(trile_set.empty())
        return {};

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

        // find art object set in cache
        auto art_object = [this](const QString& artObjectName) -> std::optional<Geometry> {
            QMutexLocker locker(&sm_ArtObjectCacheMutex);

            const auto art_object_find_iter = sm_ArtObjectCache.find(artObjectName);

            if(art_object_find_iter != sm_ArtObjectCache.cend())
                return art_object_find_iter->second;

            // load art object
            const auto art_objects_dir = QDir(m_Path + "/../art objects");

            if(!art_objects_dir.exists())
                return {};

            const auto art_object_path = art_objects_dir.absolutePath() + "/" + artObjectName + ".xml";

            if(!QFile(art_object_path).exists())
                return {};

            auto art_object_result = parseArtObject(art_object_path);

            if(!art_object_result)
                return {};

            sm_ArtObjectCache.insert({artObjectName, *art_object_result});

            return *art_object_result;
        }(artObject.m_Name);

        if(!art_object)
            return {};

        result.insert({artObject.m_Name, std::move(*art_object)});
    }

    return result;
}

LevelParser::BackgroundPlanesResult LevelParser::readBackgroundPlanes(const QDomElement& elem)
{
    // read background planes
    const auto background_planes_elem = elem.firstChildElement("BackgroundPlanes");

    if(background_planes_elem.isNull())
        return {};

    // count backgroun planes
    auto entry_elem = background_planes_elem.firstChildElement("Entry");
    auto entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        entry_count++;

        entry_elem = entry_elem.nextSiblingElement();
    }

    auto result = Level::BackgroundPlanes(entry_count);

    // entry
    entry_elem = background_planes_elem.firstChildElement("Entry");
    entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        auto background_plane = BackgroundPlane();

        // read BackgroundPlane
        const auto background_plane_elem = entry_elem.firstChildElement("BackgroundPlane");

        if(background_plane_elem.isNull())
            return {};

        if(!background_plane_elem.hasAttribute("textureName"))
            return {};

        background_plane.m_Name = QString(background_plane_elem.attribute("textureName")).replace('\\', '/');

        // read texture
        if(!background_plane_elem.hasAttribute("animated"))
            return {};

        const auto animated_str = background_plane_elem.attribute("animated");
        auto animated = false;

        if(animated_str.compare("true", Qt::CaseInsensitive) == 0)
            animated = true;
        else if(animated_str.compare("false", Qt::CaseInsensitive) == 0)
            animated = false;
        else
            return {};

        const auto texture_path = QDir(m_Path + "/../background planes").absolutePath();

        const auto texture = [this, &texture_path, &background_plane, &animated]() -> TextureResult {
            QMutexLocker locker(&sm_TextureCacheMutex);

            const auto cache_key = texture_path + "/" + background_plane.m_Name;
            const auto cache_texture = sm_TextureCache.find(cache_key);

            if(cache_texture != sm_TextureCache.cend())
                return cache_texture->second;

            const auto loaded_texture = loadTexture(texture_path, background_plane.m_Name, animated);

            if(!loaded_texture)
                return {};

            sm_TextureCache.insert({cache_key, *loaded_texture});

            return *loaded_texture;
        }();

        if(!texture)
            return {};

        background_plane.m_Geometry.m_Texture = std::move(*texture);

        // read opacity
        if(!background_plane_elem.hasAttribute("opacity"))
            return {};

        auto opacity_ok = false;
        background_plane.m_Geometry.m_Opacity = background_plane_elem.attribute("opacity").toFloat(&opacity_ok);

        if(!opacity_ok)
            return {};

        // read double sided
        if(!background_plane_elem.hasAttribute("doubleSided"))
            return {};

        const auto double_sided = background_plane_elem.attribute("doubleSided");

        if(double_sided.compare("true", Qt::CaseInsensitive) == 0)
            background_plane.m_Geometry.m_DoubleSided = true;
        else if(double_sided.compare("false", Qt::CaseInsensitive) == 0)
            background_plane.m_Geometry.m_DoubleSided = false;
        else
            return {};

        // read position
        const auto position_elem = background_plane_elem.firstChildElement("Position");

        if(position_elem.isNull())
            return {};

        const auto pos_vec3_elem = position_elem.firstChildElement("Vector3");

        if(pos_vec3_elem.isNull())
            return {};

        if(!pos_vec3_elem.hasAttribute("x") || !pos_vec3_elem.hasAttribute("y") || !pos_vec3_elem.hasAttribute("z"))
            return {};

        auto x_ok = false;
        auto y_ok = false;
        auto z_ok = false;

        background_plane.m_Position = {pos_vec3_elem.attribute("x").toFloat(&x_ok),  //
                                       pos_vec3_elem.attribute("y").toFloat(&y_ok),  //
                                       pos_vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        // read rotation
        const auto rotation_elem = background_plane_elem.firstChildElement("Rotation");

        if(rotation_elem.isNull())
            return {};

        const auto quaterion_elem = rotation_elem.firstChildElement("Quaternion");

        if(quaterion_elem.isNull())
            return {};

        if(!quaterion_elem.hasAttribute("x") || !quaterion_elem.hasAttribute("y") || !quaterion_elem.hasAttribute("z") || !quaterion_elem.hasAttribute("w"))
            return {};

        auto w_ok = false;

        background_plane.m_Rotation = {quaterion_elem.attribute("w").toFloat(&w_ok),  //
                                       quaterion_elem.attribute("x").toFloat(&x_ok),  //
                                       quaterion_elem.attribute("y").toFloat(&y_ok),  //
                                       quaterion_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok || !w_ok)
            return {};

        // read scale
        const auto scale_elem = background_plane_elem.firstChildElement("Scale");

        if(scale_elem.isNull())
            return {};

        const auto scale_vec3_elem = scale_elem.firstChildElement("Vector3");

        if(scale_vec3_elem.isNull())
            return {};

        if(!scale_vec3_elem.hasAttribute("x") || !scale_vec3_elem.hasAttribute("y") || !scale_vec3_elem.hasAttribute("z"))
            return {};

        background_plane.m_Scale = {scale_vec3_elem.attribute("x").toFloat(&x_ok),  //
                                    scale_vec3_elem.attribute("y").toFloat(&y_ok),  //
                                    scale_vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        result[entry_count++] = std::move(background_plane);
        entry_elem = entry_elem.nextSiblingElement();
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

LevelParser::CharactersResult LevelParser::readCharacters(const QDomElement& elem)
{
    // read background planes
    const auto non_player_characters_elem = elem.firstChildElement("NonplayerCharacters");

    if(non_player_characters_elem.isNull())
        return {};

    // count backgroun planes
    auto entry_elem = non_player_characters_elem.firstChildElement("Entry");
    auto entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        entry_count++;

        entry_elem = entry_elem.nextSiblingElement();
    }

    auto result = Level::Characters(entry_count);

    // entry
    entry_elem = non_player_characters_elem.firstChildElement("Entry");
    entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        auto character = Character();

        // read NpcInstance
        const auto npc_instance = entry_elem.firstChildElement("NpcInstance");

        if(npc_instance.isNull())
            return {};

        if(!npc_instance.hasAttribute("name"))
            return {};

        character.m_Name = QString(npc_instance.attribute("name"));

        // read actions
        const auto actions_elem = npc_instance.firstChildElement("Actions");

        if(actions_elem.isNull())
            return {};

        auto action_elem = actions_elem.firstChildElement("Action");
        auto action_count = size_t(0);

        while(!action_elem.isNull())
        {
            action_count++;

            action_elem = action_elem.nextSiblingElement();
        }

        using Actions = std::vector<std::pair<QString, QString>>;
        auto actions = Actions(action_count);

        action_elem = actions_elem.firstChildElement("Action");
        action_count = size_t(0);

        while(!action_elem.isNull())
        {
            if(!action_elem.hasAttribute("key"))
                return {};

            const auto action_key = action_elem.attribute("key");

            const auto npc_action_content = action_elem.firstChildElement("NpcActionContent");

            if(npc_action_content.isNull())
                return {};

            if(!npc_action_content.hasAttribute("animationName"))
                return {};

            const auto action_animation_name = npc_action_content.attribute("animationName");

            actions[action_count++] = {action_key, action_animation_name};
            action_elem = action_elem.nextSiblingElement();
        }

        if(actions.empty())
            return {};

        // read texture
        const auto texture_path = QDir(m_Path + "/../character animations").absolutePath();
        const auto action_name = actions[0].second;

        const auto texture = [this, &texture_path, &character, &action_name]() -> TextureResult {
            QMutexLocker locker(&sm_TextureCacheMutex);

            const auto cache_key = texture_path + "/" + character.m_Name + "/" + action_name;
            const auto cache_texture = sm_TextureCache.find(cache_key);

            if(cache_texture != sm_TextureCache.cend())
                return cache_texture->second;

            const auto loaded_texture = loadTexture(texture_path, character.m_Name + "/" + action_name, true);

            if(!loaded_texture)
                return {};

            sm_TextureCache.insert(std::make_pair(cache_key, *loaded_texture));

            return *loaded_texture;
        }();

        if(!texture)
            return {};

        character.m_Geometry.m_Texture = std::move(*texture);

        // read position
        const auto position_elem = npc_instance.firstChildElement("Position");

        if(position_elem.isNull())
            return {};

        const auto pos_vec3_elem = position_elem.firstChildElement("Vector3");

        if(pos_vec3_elem.isNull())
            return {};

        if(!pos_vec3_elem.hasAttribute("x") || !pos_vec3_elem.hasAttribute("y") || !pos_vec3_elem.hasAttribute("z"))
            return {};

        auto x_ok = false;
        auto y_ok = false;
        auto z_ok = false;

        character.m_Position = {pos_vec3_elem.attribute("x").toFloat(&x_ok),  //
                                pos_vec3_elem.attribute("y").toFloat(&y_ok),  //
                                pos_vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        result[entry_count++] = std::move(character);
        entry_elem = entry_elem.nextSiblingElement();
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
