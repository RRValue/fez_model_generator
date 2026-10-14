#include "parser/level/LevelSections.h"

#include "xml/Xml.h"

#include <format>
#include <utility>

namespace fez::level
{

namespace
{

std::expected<TrileEmplacement, Error> readTrileEntry(const QDomElement& entry_elem)
{
    TrileEmplacement result;

    // TrileEmplacement x, y, z
    const auto emplacement_elem = childElement(entry_elem, "TrileEmplacement");

    if(!emplacement_elem)
        return std::unexpected(emplacement_elem.error());

    const auto emplacement = readVector3(*emplacement_elem);

    if(!emplacement)
        return std::unexpected(emplacement.error());

    result.m_Emplacement = *emplacement;

    // TrileInstance trileId, orientation: integers; orientation is a quarter turn about Y, 0 to 3
    const auto instance_elem = childElement(entry_elem, "TrileInstance");

    if(!instance_elem)
        return std::unexpected(instance_elem.error());

    const auto trile_id = attribute<int>(*instance_elem, "trileId");

    if(!trile_id)
        return std::unexpected(trile_id.error());

    const auto orientation = attribute<int>(*instance_elem, "orientation");

    if(!orientation)
        return std::unexpected(orientation.error());

    if(*orientation < 0 || *orientation > 3)
        return std::unexpected(Error{ErrorKind::BadValue,
                                     std::format("attribute 'orientation' is not in 0 to 3: {}", *orientation),
                                     std::format("{}@orientation", elementPath(*instance_elem))});

    result.m_Id = *trile_id;
    result.m_Orientation = *orientation;

    // TrileInstance/Position/Vector3
    const auto position_elem = childElement(*instance_elem, "Position");

    if(!position_elem)
        return std::unexpected(position_elem.error());

    const auto vector3_elem = childElement(*position_elem, "Vector3");

    if(!vector3_elem)
        return std::unexpected(vector3_elem.error());

    const auto position = readVector3(*vector3_elem);

    if(!position)
        return std::unexpected(position.error());

    result.m_Position = *position;

    return result;
}

} // namespace

std::expected<LevelDescription::TrileEmplacements, Error> readTriles(const QDomElement& levelElem)
{
    const auto triles_elem = childElement(levelElem, "Triles");

    if(!triles_elem)
        return std::unexpected(triles_elem.error());

    LevelDescription::TrileEmplacements result;

    // From the first Entry on, every following sibling element is an entry (no name filter, as before).
    const auto first_entry = triles_elem->firstChildElement("Entry");

    for(auto iter = ChildElementRange::Iterator(first_entry, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        auto emplacement = readTrileEntry(*iter);

        if(!emplacement)
            return std::unexpected(emplacement.error());

        result.push_back(std::move(*emplacement));
    }

    return result;
}

} // namespace fez::level
