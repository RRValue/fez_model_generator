#pragma once

#include "export/Transforms.h"
#include "model/Geometry.h"

#include <QtCore/QString>

#include <memory>
#include <optional>
#include <vector>

struct aiScene;
struct aiMesh;
struct aiMaterial;
struct aiNode;

namespace fez
{

// Collects meshes, materials and nodes for one OBJ export and hands them to a complete aiScene in build().
// Every addMesh call adds exactly one mesh and one material (no deduplication); the OBJ exporter writes every
// material to the .mtl file, so the number of calls decides the output.
class SceneBuilder
{
public:
    using MeshId = unsigned int;

    SceneBuilder();
    ~SceneBuilder();

    SceneBuilder(const SceneBuilder&) = delete;
    SceneBuilder& operator=(const SceneBuilder&) = delete;

    // Adds one mesh and one material for the geometry and returns the mesh id (= material index). Returns no id, but
    // still keeps the (incomplete) mesh and material, when the geometry has no vertices or no faces, or when it is a
    // plane whose texture file cannot be loaded (kept behavior; Task 29 drops empty meshes).
    std::optional<MeshId> addMesh(const Geometry& geometry);

    // Adds a child of the root node showing one mesh.
    void addNode(const QString& name, const Transform& transform, MeshId meshId);

    // Releases everything added so far into a new aiScene; the builder is empty afterwards.
    std::unique_ptr<aiScene> build();

private:
    std::vector<std::unique_ptr<aiMesh>> m_Meshes;
    std::vector<std::unique_ptr<aiMaterial>> m_Materials;
    std::vector<std::unique_ptr<aiNode>> m_Nodes;
};

} // namespace fez
