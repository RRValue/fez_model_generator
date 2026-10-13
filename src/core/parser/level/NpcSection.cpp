#include "parser/level/LevelSections.h"

#include "xml/Xml.h"

#include <utility>

namespace fez::level
{

namespace
{

// Actions: from the first Action child on, every following sibling element is an action (no name filter, as before).
// No Action child fails the entry (MissingElement ".../Actions/Action").
std::expected<NpcDescription::Actions, Error> readActions(const QDomElement& actions_elem)
{
    const auto first_action = childElement(actions_elem, "Action");

    if(!first_action)
        return std::unexpected(first_action.error());

    NpcDescription::Actions result;

    for(auto iter = ChildElementRange::Iterator(*first_action, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        const auto key = attribute<QString>(*iter, "key");

        if(!key)
            return std::unexpected(key.error());

        const auto content_elem = childElement(*iter, "NpcActionContent");

        if(!content_elem)
            return std::unexpected(content_elem.error());

        const auto animation_name = attribute<QString>(*content_elem, "animationName");

        if(!animation_name)
            return std::unexpected(animation_name.error());

        result.emplace_back(*key, *animation_name);
    }

    return result;
}

std::expected<NpcDescription, Error> readNpcEntry(const QDomElement& entry_elem)
{
    NpcDescription result;

    const auto instance_elem = childElement(entry_elem, "NpcInstance");

    if(!instance_elem)
        return std::unexpected(instance_elem.error());

    const auto name = attribute<QString>(*instance_elem, "name");

    if(!name)
        return std::unexpected(name.error());

    result.m_Name = *name;

    // Actions
    const auto actions_elem = childElement(*instance_elem, "Actions");

    if(!actions_elem)
        return std::unexpected(actions_elem.error());

    auto actions = readActions(*actions_elem);

    if(!actions)
        return std::unexpected(actions.error());

    result.m_Actions = std::move(*actions);

    // Position/Vector3
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

std::expected<LevelDescription::Npcs, Error> readNpcs(const QDomElement& levelElem)
{
    const auto npcs_elem = childElement(levelElem, "NonplayerCharacters");

    if(!npcs_elem)
        return std::unexpected(npcs_elem.error());

    LevelDescription::Npcs result;

    // From the first Entry on, every following sibling element is an entry (no name filter, as before).
    const auto first_entry = npcs_elem->firstChildElement("Entry");

    for(auto iter = ChildElementRange::Iterator(first_entry, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        auto npc = readNpcEntry(*iter);

        if(!npc)
            return std::unexpected(npc.error());

        result.push_back(std::move(*npc));
    }

    return result;
}

} // namespace fez::level
