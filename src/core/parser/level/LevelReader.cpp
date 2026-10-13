#include "parser/level/LevelReader.h"

#include "parser/level/LevelSections.h"
#include "xml/Xml.h"

#include <qdebug.h>

#include <utility>

namespace fez
{

std::expected<LevelDescription, Error> parseLevel(const QString& path)
{
    qDebug() << "parsing: " << path;

    const auto document = loadXml(path);

    if(!document)
    {
        if(document.error().kind == ErrorKind::XmlSyntax)
            qDebug() << "Error: \"" + QString::fromStdString(document.error().message) + "\" at " + QString::fromStdString(document.error().source);

        return std::unexpected(document.error());
    }

    return readLevel(*document);
}

std::expected<LevelDescription, Error> readLevel(const QDomDocument& document)
{
    // QDomDocument::firstChildElement: the document element, if it is named Level
    const auto level_elem = document.firstChildElement("Level");

    if(level_elem.isNull())
        return std::unexpected(Error{ErrorKind::MissingElement, "missing element 'Level'", "Level"});

    LevelDescription result;

    // header
    const auto trile_set_name = attribute<QString>(level_elem, "trileSetName");

    if(!trile_set_name)
        return std::unexpected(trile_set_name.error());

    result.m_TrileSetName = *trile_set_name;

    const auto name = attribute<QString>(level_elem, "name");

    if(!name)
        return std::unexpected(name.error());

    result.m_Name = *name;

    // Triles
    auto triles = level::readTriles(level_elem);

    if(!triles)
        return std::unexpected(triles.error());

    result.m_TrileEmplacements = std::move(*triles);

    // ArtObjects
    auto art_objects = level::readArtObjects(level_elem);

    if(!art_objects)
        return std::unexpected(art_objects.error());

    result.m_ArtObjects = std::move(*art_objects);

    // BackgroundPlanes
    auto background_planes = level::readBackgroundPlanes(level_elem);

    if(!background_planes)
        return std::unexpected(background_planes.error());

    result.m_BackgroundPlanes = std::move(*background_planes);

    // NonplayerCharacters
    auto npcs = level::readNpcs(level_elem);

    if(!npcs)
        return std::unexpected(npcs.error());

    result.m_Npcs = std::move(*npcs);

    return result;
}

} // namespace fez
