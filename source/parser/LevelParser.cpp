#include "parser/LevelParser.h"

#include "parser/ArtObjectParser.h"
#include "parser/TrileSetParser.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <qdebug.h>

LevelParser::TrileSetCache LevelParser::sm_TrileSetCache = {};
LevelParser::ArtObjectCache LevelParser::sm_ArtObjectCache = {};

LevelParser::LevelParser() : m_Document{}
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

    const auto name = info.baseName();
    const auto org_path = info.absolutePath();
    m_Document.clear();

    // load
    auto xml_file = QFile(path);

    if(!xml_file.exists())
        return {};

    if(!xml_file.open(QIODevice::OpenModeFlag::ReadOnly))
        return {};

    QString error_msg;
    int error_line;
    int error_column;

    if(!m_Document.setContent(&xml_file, &error_msg, &error_line, &error_column))
    {
        qDebug() << "Error: \"" + error_msg + "\" at line: " + QString::number(error_line) + " Columns: " + QString::number(error_column);

        return {};
    }

    // parse
    const auto level_elem = m_Document.firstChildElement("Level");

    if(level_elem.isNull())
        return {};

    Level result;

    if(!level_elem.hasAttribute("trileSetName"))
        return {};

    result.m_TrileSetName = level_elem.attribute("trileSetName");

    if(!level_elem.hasAttribute("name"))
        return {};

    result.m_LevelName = level_elem.attribute("name");

    // Size
    // StartingPosition
    // Volumes
    // Scripts
    // Triles
    auto trile_emplacements = std::move(readTrileEmplacements(level_elem));

    if(!trile_emplacements)
        return {};

    auto trile_geometries = parseTrileEmplacements(*trile_emplacements, result.m_TrileSetName);

    if(!trile_geometries)
        return {};

    // ArtObjects
    auto art_objects = std::move(readArtObjects(level_elem));

    if(!art_objects)
        return {};

    auto art_object_geometries = parseArtObjects(*art_objects);

    if(!art_object_geometries)
        return {};
    // Groups
    // NonplayerCharacters
    // Paths
    // MutedLoops
    // AmbienceTracks

    result.m_TrileEmplacements = std::move(*trile_emplacements);
    result.m_TrileGeometries = std::move(*trile_geometries);

    result.m_ArtObjects = std::move(*art_objects);
    result.m_ArtObjectGeometries = std::move(*art_object_geometries);

    return result;
}

LevelParser::TrileEmplacementsResult LevelParser::readTrileEmplacements(const QDomElement& elem)
{
    // read TrileEmplacement
    const auto triles_elem = elem.firstChildElement("Triles");

    if(triles_elem.isNull())
        return {};

    // count emplacements
    auto entry_elem = triles_elem.firstChildElement("Entry");
    auto entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        entry_count++;

        entry_elem = entry_elem.nextSiblingElement();
    }

    auto result = Level::TrileEmplacements(entry_count);

    // set emplacements
    entry_elem = triles_elem.firstChildElement("Entry");
    entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        auto trile_emplacement = TrileEmplacement();

        // read TrileEmplacement
        const auto emplacemant_elem = entry_elem.firstChildElement("TrileEmplacement");

        if(emplacemant_elem.isNull())
            return {};

        if(!emplacemant_elem.hasAttribute("x") || !emplacemant_elem.hasAttribute("y") || !emplacemant_elem.hasAttribute("z"))
            return {};

        auto x_ok = false;
        auto y_ok = false;
        auto z_ok = false;

        trile_emplacement.m_Emplacement = {emplacemant_elem.attribute("x").toFloat(&x_ok),  //
                                           emplacemant_elem.attribute("y").toFloat(&y_ok),  //
                                           emplacemant_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        // read TrileInstance
        const auto trile_instance_elem = entry_elem.firstChildElement("TrileInstance");

        if(trile_instance_elem.isNull())
            return {};

        if(!trile_instance_elem.hasAttribute("trileId") || !trile_instance_elem.hasAttribute("orientation"))
            return {};

        auto trile_id_ok = false;
        auto orientation_ok = false;

        trile_emplacement.m_Id = trile_instance_elem.attribute("trileId").toFloat(&trile_id_ok);
        trile_emplacement.m_Orintation = trile_instance_elem.attribute("orientation").toFloat(&orientation_ok);

        if(!trile_id_ok || !orientation_ok)
            return {};

        // read position
        const auto position_elem = trile_instance_elem.firstChildElement("Position");

        if(position_elem.isNull())
            return {};

        const auto vec3_elem = position_elem.firstChildElement("Vector3");

        if(vec3_elem.isNull())
            return {};

        if(!vec3_elem.hasAttribute("x") || !vec3_elem.hasAttribute("y") || !vec3_elem.hasAttribute("z"))
            return {};

        trile_emplacement.m_Position = {vec3_elem.attribute("x").toFloat(&x_ok),  //
                                        vec3_elem.attribute("y").toFloat(&y_ok),  //
                                        vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        result[entry_count++] = std::move(trile_emplacement);
        entry_elem = entry_elem.nextSiblingElement();
    }

    return result;
}

LevelParser::ArtObjectsResult LevelParser::readArtObjects(const QDomElement& elem)
{
    // read TrileEmplacement
    const auto art_objects_elem = elem.firstChildElement("ArtObjects");

    if(art_objects_elem.isNull())
        return {};

    // count emplacements
    auto entry_elem = art_objects_elem.firstChildElement("Entry");
    auto entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        entry_count++;

        entry_elem = entry_elem.nextSiblingElement();
    }

    auto result = Level::ArtObjects(entry_count);

    // set emplacements
    entry_elem = art_objects_elem.firstChildElement("Entry");
    entry_count = size_t(0);

    while(!entry_elem.isNull())
    {
        auto art_object = ArtObject();

        // read ArtObjectInstance
        const auto art_object_instance_elem = entry_elem.firstChildElement("ArtObjectInstance");

        if(art_object_instance_elem.isNull())
            return {};

        if(!art_object_instance_elem.hasAttribute("name"))
            return {};

        art_object.m_Name = art_object_instance_elem.attribute("name");

        // read position
        const auto position_elem = art_object_instance_elem.firstChildElement("Position");

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

        art_object.m_Position = {pos_vec3_elem.attribute("x").toFloat(&x_ok),  //
                                 pos_vec3_elem.attribute("y").toFloat(&y_ok),  //
                                 pos_vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        // read rotation
        const auto rotation_elem = art_object_instance_elem.firstChildElement("Rotation");

        if(rotation_elem.isNull())
            return {};

        const auto quaterion_elem = rotation_elem.firstChildElement("Quaternion");

        if(quaterion_elem.isNull())
            return {};

        if(!quaterion_elem.hasAttribute("x") || !quaterion_elem.hasAttribute("y") || !quaterion_elem.hasAttribute("z") || !quaterion_elem.hasAttribute("w"))
            return {};

        auto w_ok = false;

        art_object.m_Rotation = {quaterion_elem.attribute("x").toFloat(&x_ok),  //
                                 quaterion_elem.attribute("y").toFloat(&y_ok),  //
                                 quaterion_elem.attribute("z").toFloat(&z_ok),  //
                                 quaterion_elem.attribute("w").toFloat(&w_ok)};

        if(!x_ok || !y_ok || !z_ok || !w_ok)
            return {};

        // read scale
        const auto scale_elem = art_object_instance_elem.firstChildElement("Scale");

        if(scale_elem.isNull())
            return {};

        const auto scale_vec3_elem = scale_elem.firstChildElement("Vector3");

        if(scale_vec3_elem.isNull())
            return {};

        if(!scale_vec3_elem.hasAttribute("x") || !scale_vec3_elem.hasAttribute("y") || !scale_vec3_elem.hasAttribute("z"))
            return {};

        art_object.m_Scale = {scale_vec3_elem.attribute("x").toFloat(&x_ok),  //
                              scale_vec3_elem.attribute("y").toFloat(&y_ok),  //
                              scale_vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return {};

        result[entry_count++] = std::move(art_object);
        entry_elem = entry_elem.nextSiblingElement();
    }

    return result;
}

LevelParser::TrileGeometriesResult LevelParser::parseTrileEmplacements(const Level::TrileEmplacements& emplacements, const QString& trileSetName)
{
    // find trile set in cache
    const auto trile_set = [this, &trileSetName]() -> Level::TrileGeometries {
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

        TrileSetParser parser;
        const auto triles_set = parser.parse(trile_set_path);

        if(triles_set.empty())
            return {};

        sm_TrileSetCache.insert({trileSetName, triles_set});

        return triles_set;
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

            ArtObjectParser parser;
            auto art_object_result = parser.parse(art_object_path);

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