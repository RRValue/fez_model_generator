#pragma once

#include "model/Level.h"
#include "model/LevelDescription.h"
#include "model/Texture.h"

#include <QtCore/QString>

#include <optional>

namespace fez
{

class AssetRepository;

// Builds a Level from a level XML file; trile sets, art objects and textures come from the repository (shared by
// all levels of a run, thread-safe).
class LevelParser
{
    using LevelResult = std::optional<Level>;
    
    using TrileGeometriesResult = std::optional<Level::TrileGeometries>;

    using ArtObjectGeometriesResult = std::optional<Level::ArtObjectGeometries>;

    using BackgroundPlanesResult = std::optional<Level::BackgroundPlanes>;
    using CharactersResult = std::optional<Level::Characters>;

public:
    explicit LevelParser(AssetRepository& repository);

    LevelResult parse(const QString& path) noexcept;

private:
    BackgroundPlanesResult loadBackgroundPlaneTextures(const LevelDescription::BackgroundPlanes& backgroundPlanes);
    CharactersResult loadCharacterTextures(const LevelDescription::Npcs& npcs);

    TrileGeometriesResult parseTrileEmplacements(const Level::TrileEmplacements& emplacements, const QString& trileSetName);
    ArtObjectGeometriesResult parseArtObjects(const Level::ArtObjects& artObjects);
    BackgroundPlanesResult parseBackgroundPlanes(const Level::BackgroundPlanes& backgroundPlanes);
    CharactersResult parseCharacters(const Level::Characters& characters);

private:
    AssetRepository& m_Repository;
};

} // namespace fez
