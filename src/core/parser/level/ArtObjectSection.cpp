#include "parser/level/LevelSections.h"

#include "xml/Xml.h"

#include <utility>

namespace fez::level
{

namespace
{

// The first <childName> of the first <name> child of parent, e.g. Position/Vector3.
std::expected<QDomElement, Error> grandChildElement(const QDomElement& parent, const QString& name, const QString& childName)
{
    const auto child = childElement(parent, name);

    if(!child)
        return std::unexpected(child.error());

    return childElement(*child, childName);
}

std::expected<ArtObject, Error> readArtObjectEntry(const QDomElement& entry_elem)
{
    ArtObject result;

    const auto instance_elem = childElement(entry_elem, "ArtObjectInstance");

    if(!instance_elem)
        return std::unexpected(instance_elem.error());

    const auto name = attribute<QString>(*instance_elem, "name");

    if(!name)
        return std::unexpected(name.error());

    result.m_Name = *name;

    // Position/Vector3
    const auto position_elem = grandChildElement(*instance_elem, "Position", "Vector3");

    if(!position_elem)
        return std::unexpected(position_elem.error());

    const auto position = readVector3(*position_elem);

    if(!position)
        return std::unexpected(position.error());

    result.m_Position = *position;

    // Rotation/Quaternion (w, x, y, z)
    const auto rotation_elem = grandChildElement(*instance_elem, "Rotation", "Quaternion");

    if(!rotation_elem)
        return std::unexpected(rotation_elem.error());

    const auto rotation = readQuaternion(*rotation_elem);

    if(!rotation)
        return std::unexpected(rotation.error());

    result.m_Rotation = *rotation;

    // Scale/Vector3
    const auto scale_elem = grandChildElement(*instance_elem, "Scale", "Vector3");

    if(!scale_elem)
        return std::unexpected(scale_elem.error());

    const auto scale = readVector3(*scale_elem);

    if(!scale)
        return std::unexpected(scale.error());

    result.m_Scale = *scale;

    return result;
}

} // namespace

std::expected<LevelDescription::ArtObjects, Error> readArtObjects(const QDomElement& levelElem)
{
    const auto art_objects_elem = childElement(levelElem, "ArtObjects");

    if(!art_objects_elem)
        return std::unexpected(art_objects_elem.error());

    LevelDescription::ArtObjects result;

    // From the first Entry on, every following sibling element is an entry (no name filter, as before).
    const auto first_entry = art_objects_elem->firstChildElement("Entry");

    for(auto iter = ChildElementRange::Iterator(first_entry, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        auto art_object = readArtObjectEntry(*iter);

        if(!art_object)
            return std::unexpected(art_object.error());

        result.push_back(std::move(*art_object));
    }

    return result;
}

} // namespace fez::level
