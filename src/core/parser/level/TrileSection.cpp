#include "parser/level/LevelSections.h"

#include "xml/Xml.h"

#include <format>
#include <utility>

namespace fez::level
{

namespace
{

// One TrileInstance element: trileId, orientation (integers; orientation is a quarter turn about Y, 0 to 3) and
// Position/Vector3, placed at the given emplacement.
std::expected<TrileEmplacement, Error> readTrileInstance(const QDomElement& instance_elem, const Vec3f& emplacement)
{
    TrileEmplacement result;
    result.m_Emplacement = emplacement;

    const auto trile_id = attribute<int>(instance_elem, "trileId");

    if(!trile_id)
        return std::unexpected(trile_id.error());

    const auto orientation = attribute<int>(instance_elem, "orientation");

    if(!orientation)
        return std::unexpected(orientation.error());

    if(*orientation < 0 || *orientation > 3)
        return std::unexpected(Error{ErrorKind::BadValue,
                                     std::format("attribute 'orientation' is not in 0 to 3: {}", *orientation),
                                     std::format("{}@orientation", elementPath(instance_elem))});

    result.m_Id = *trile_id;
    result.m_Orientation = *orientation;

    // TrileInstance/Position/Vector3
    const auto position_elem = childElement(instance_elem, "Position");

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

// One Triles entry: its TrileInstance, then every instance in TrileInstance/OverlappedTriles (if present), in XML
// order, all at the entry's emplacement. Overlapped instances are further triles in the same cell, each with its own
// id, orientation and position (FEZ data: 1135 of them in 52 levels, fez_mode-777.38). Every child element of
// OverlappedTriles is read as a TrileInstance; nested OverlappedTriles are not read (the data has none).
std::expected<void, Error> readTrileEntry(const QDomElement& entry_elem, LevelDescription::TrileEmplacements& result)
{
    // TrileEmplacement x, y, z
    const auto emplacement_elem = childElement(entry_elem, "TrileEmplacement");

    if(!emplacement_elem)
        return std::unexpected(emplacement_elem.error());

    const auto emplacement = readVector3(*emplacement_elem);

    if(!emplacement)
        return std::unexpected(emplacement.error());

    const auto instance_elem = childElement(entry_elem, "TrileInstance");

    if(!instance_elem)
        return std::unexpected(instance_elem.error());

    auto instance = readTrileInstance(*instance_elem, *emplacement);

    if(!instance)
        return std::unexpected(instance.error());

    result.push_back(std::move(*instance));

    const auto overlapped_elem = instance_elem->firstChildElement("OverlappedTriles");

    if(overlapped_elem.isNull())
        return {};

    for(const auto& overlapped_instance_elem : childElements(overlapped_elem))
    {
        auto overlapped = readTrileInstance(overlapped_instance_elem, *emplacement);

        if(!overlapped)
            return std::unexpected(overlapped.error());

        result.push_back(std::move(*overlapped));
    }

    return {};
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
        const auto read = readTrileEntry(*iter, result);

        if(!read)
            return std::unexpected(read.error());
    }

    return result;
}

} // namespace fez::level
