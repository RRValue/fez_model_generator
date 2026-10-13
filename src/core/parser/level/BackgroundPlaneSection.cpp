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

std::expected<BackgroundPlaneDescription, Error> readBackgroundPlaneEntry(const QDomElement& entry_elem)
{
    BackgroundPlaneDescription result;

    const auto plane_elem = childElement(entry_elem, "BackgroundPlane");

    if(!plane_elem)
        return std::unexpected(plane_elem.error());

    // textureName, with backslashes turned into slashes
    const auto texture_name = attribute<QString>(*plane_elem, "textureName");

    if(!texture_name)
        return std::unexpected(texture_name.error());

    result.m_TextureName = QString(*texture_name).replace('\\', '/');

    // animated
    const auto animated = readBool(*plane_elem, "animated");

    if(!animated)
        return std::unexpected(animated.error());

    result.m_Animated = *animated;

    // opacity
    const auto opacity = attribute<float>(*plane_elem, "opacity");

    if(!opacity)
        return std::unexpected(opacity.error());

    result.m_Opacity = *opacity;

    // doubleSided
    const auto double_sided = readBool(*plane_elem, "doubleSided");

    if(!double_sided)
        return std::unexpected(double_sided.error());

    result.m_DoubleSided = *double_sided;

    // Position/Vector3
    const auto position_elem = grandChildElement(*plane_elem, "Position", "Vector3");

    if(!position_elem)
        return std::unexpected(position_elem.error());

    const auto position = readVector3(*position_elem);

    if(!position)
        return std::unexpected(position.error());

    result.m_Position = *position;

    // Rotation/Quaternion (w, x, y, z)
    const auto rotation_elem = grandChildElement(*plane_elem, "Rotation", "Quaternion");

    if(!rotation_elem)
        return std::unexpected(rotation_elem.error());

    const auto rotation = readQuaternion(*rotation_elem);

    if(!rotation)
        return std::unexpected(rotation.error());

    result.m_Rotation = *rotation;

    // Scale/Vector3
    const auto scale_elem = grandChildElement(*plane_elem, "Scale", "Vector3");

    if(!scale_elem)
        return std::unexpected(scale_elem.error());

    const auto scale = readVector3(*scale_elem);

    if(!scale)
        return std::unexpected(scale.error());

    result.m_Scale = *scale;

    return result;
}

} // namespace

std::expected<LevelDescription::BackgroundPlanes, Error> readBackgroundPlanes(const QDomElement& levelElem)
{
    const auto planes_elem = childElement(levelElem, "BackgroundPlanes");

    if(!planes_elem)
        return std::unexpected(planes_elem.error());

    LevelDescription::BackgroundPlanes result;

    // From the first Entry on, every following sibling element is an entry (no name filter, as before).
    const auto first_entry = planes_elem->firstChildElement("Entry");

    for(auto iter = ChildElementRange::Iterator(first_entry, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        auto plane = readBackgroundPlaneEntry(*iter);

        if(!plane)
            return std::unexpected(plane.error());

        result.push_back(std::move(*plane));
    }

    return result;
}

} // namespace fez::level
