#pragma once

#include "error/Error.h"
#include "export/Transforms.h"
#include "model/Geometry.h"

#include <QtCore/QString>

#include <expected>
#include <memory>
#include <vector>

struct aiScene;
struct aiMesh;
struct aiMaterial;
struct aiNode;

namespace fez
{

// True when the geometry has no vertices or no faces. Such geometry is normal FEZ data (e.g. empty triles) and gets
// no mesh; SceneBuilder::addMesh rejects it.
bool isEmptyGeometry(const Geometry& geometry);

// Collects meshes, materials and nodes for one OBJ export and hands them to a complete aiScene in build().
// Every addMesh call for non-empty geometry adds exactly one mesh and one material (no deduplication); the OBJ
// exporter writes every material to the .mtl file, so the number of calls decides the output.
class SceneBuilder
{
public:
    using MeshId = unsigned int;

    SceneBuilder();
    ~SceneBuilder();

    SceneBuilder(const SceneBuilder&) = delete;
    SceneBuilder& operator=(const SceneBuilder&) = delete;

    // Adds one mesh and one material for the geometry and returns the mesh id (= material index).
    // Errors (nothing is added in either case, so the next mesh gets the next free id):
    //  - BadValue when the geometry has no vertices or no faces (isEmptyGeometry).
    //  - FileNotFound when the geometry is a plane whose texture file does not exist, Io when it exists but cannot be
    //    read as an image (source: the texture file).
    std::expected<MeshId, Error> addMesh(const Geometry& geometry);

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
