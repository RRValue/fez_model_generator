#pragma once

#include "error/Error.h"
#include "model/Geometry.h"
#include "model/Level.h"

#include <QtCore/QString>

#include <expected>
#include <memory>
#include <optional>
#include <vector>

struct aiScene;

namespace fez
{

// Deletes an aiScene where assimp's types are complete (callers outside export/ never include assimp headers).
struct SceneDeleter
{
    void operator()(aiScene* scene) const noexcept;
};

using ScenePtr = std::unique_ptr<aiScene, SceneDeleter>;

// A texture to copy next to an exported OBJ: m_Source -> <export dir>/<m_Name>.
struct TextureFile
{
    QString m_Source;
    QString m_Name;
};

// A scene ready for exportObj plus the textures its materials reference: one entry per mesh that got an id, in mesh
// order (duplicates included; TextureCopier copies each destination once).
struct ComposedScene
{
    ScenePtr m_Scene;
    std::vector<TextureFile> m_Textures;
};

// One mesh with one unnamed node at the origin (art objects, triles). No scene (nothing to export, not an error) for
// empty geometry (isEmptyGeometry). Error: the SceneBuilder::addMesh error otherwise (a plane whose texture cannot be
// loaded).
std::expected<std::optional<ComposedScene>, Error> buildGeometryScene(const Geometry& geometry);

// One mesh, material and node per instance, in this order: art objects (skipped without geometry; position with
// levelObjectOffset), trile emplacements (skipped without geometry, e.g. id -1; node named after the id, rotation
// trileOrientation), background planes (levelObjectOffset), NPCs (position only). An instance with empty geometry
// (isEmptyGeometry; normal FEZ data, e.g. empty triles) is skipped silently: no mesh, material, node or texture entry.
// Gives a scene also for an empty level. Error: the first SceneBuilder::addMesh error (a plane or NPC whose texture
// cannot be loaded, FileNotFound or Io with the texture file as source); the level then gets no scene at all, as for
// any other level error.
std::expected<ComposedScene, Error> buildLevelScene(const Level& level);

} // namespace fez
