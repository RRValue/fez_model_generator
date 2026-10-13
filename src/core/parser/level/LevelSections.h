#pragma once

// Internal to parser/level: one reader per section of the Level element.

#include "error/Error.h"
#include "model/LevelDescription.h"

#include <QtXml/QDomElement>

#include <expected>

namespace fez::level
{

// Level/Triles (TrileSection.cpp).
std::expected<LevelDescription::TrileEmplacements, Error> readTriles(const QDomElement& levelElem);

// Level/ArtObjects (ArtObjectSection.cpp).
std::expected<LevelDescription::ArtObjects, Error> readArtObjects(const QDomElement& levelElem);

// Level/BackgroundPlanes (BackgroundPlaneSection.cpp).
std::expected<LevelDescription::BackgroundPlanes, Error> readBackgroundPlanes(const QDomElement& levelElem);

// Level/NonplayerCharacters (NpcSection.cpp).
std::expected<LevelDescription::Npcs, Error> readNpcs(const QDomElement& levelElem);

} // namespace fez::level
