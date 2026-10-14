#include "export/SceneComposition.h"
#include "export/Transforms.h"

#include "model/Geometry.h"
#include "model/Level.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QString>

#include <assimp/scene.h>

#include <string>

// buildGeometryScene / buildLevelScene compose the scenes the old GeometryWriter / LevelWriter exported, and list the
// textures to copy next to the OBJ: one entry per mesh that got an id, in mesh order.

namespace
{

fez::Geometry triangle(const QString& name, const QString& texture)
{
    fez::Geometry geometry;
    geometry.m_Name = name;
    geometry.m_Vertices = {
        {.m_Position = {0.0f, 0.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {0.0f, 0.0f}},
        {.m_Position = {1.0f, 0.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {1.0f, 0.0f}},
        {.m_Position = {0.0f, 1.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {0.0f, 1.0f}},
    };
    geometry.m_Indices = {0, 1, 2};
    geometry.m_Texture.m_TextureName = texture;
    geometry.m_Texture.m_TextureOrgFile = "in/" + texture;
    return geometry;
}

std::string nodeName(const aiScene& scene, unsigned int child)
{
    return scene.mRootNode->mChildren[child]->mName.C_Str();
}

unsigned int nodeMesh(const aiScene& scene, unsigned int child)
{
    const auto& node = *scene.mRootNode->mChildren[child];
    REQUIRE(node.mNumMeshes == 1);
    return node.mMeshes[0];
}

aiMatrix4x4 expectedMatrix(const fez::Vec3f& p, const fez::QuaternionF& r, const fez::Vec3f& s)
{
    return aiMatrix4x4(aiVector3D(s.x(), s.y(), s.z()), aiQuaternion(r.w(), r.x(), r.y(), r.z()),
                       aiVector3D(p.x(), p.y(), p.z()));
}

bool sameMatrix(const aiMatrix4x4& a, const aiMatrix4x4& b)
{
    for(unsigned int row = 0; row < 4; ++row)
        for(unsigned int column = 0; column < 4; ++column)
            if(a[row][column] != b[row][column])
                return false;

    return true;
}

} // namespace

TEST_CASE("buildGeometryScene: one mesh and one unnamed node at the origin", "[export][scene]")
{
    const auto composed = fez::buildGeometryScene(triangle("ao", "ao.png"));

    REQUIRE(composed.has_value());
    REQUIRE(composed->m_Scene != nullptr);

    const auto& scene = *composed->m_Scene;
    CHECK(scene.mNumMeshes == 1);
    CHECK(scene.mNumMaterials == 1);
    REQUIRE(scene.mRootNode->mNumChildren == 1);
    CHECK(nodeName(scene, 0).empty());
    CHECK(nodeMesh(scene, 0) == 0);
    CHECK(scene.mRootNode->mChildren[0]->mTransformation.IsIdentity());

    REQUIRE(composed->m_Textures.size() == 1);
    CHECK(composed->m_Textures[0].m_Source == "in/ao.png");
    CHECK(composed->m_Textures[0].m_Name == "ao.png");
}

TEST_CASE("buildGeometryScene: nothing to export for a geometry without faces", "[export][scene]")
{
    auto geometry = triangle("empty", "empty.png");
    geometry.m_Indices.clear();

    CHECK_FALSE(fez::buildGeometryScene(geometry).has_value());
}

TEST_CASE("buildLevelScene: art objects, triles, planes and NPCs in this order", "[export][scene]")
{
    fez::Level level;
    level.m_LevelName = "LEVEL";

    level.m_ArtObjectGeometries.emplace("ao_a", triangle("ao_a", "ao_a.png"));
    level.m_ArtObjects.push_back({.m_Name = "ao_a", .m_Position = {1.0f, 2.0f, 3.0f},
                                  .m_Rotation = fez::QuaternionF(0.0f, 1.0f, 0.0f, 0.0f), .m_Scale = {2.0f, 2.0f, 2.0f}});
    level.m_ArtObjects.push_back({.m_Name = "ao_missing"}); // no geometry: skipped

    level.m_TrileGeometries.emplace(7, triangle("7", "set.png"));
    level.m_TrileEmplacements.push_back({.m_Id = 7, .m_Position = {4.0f, 5.0f, 6.0f}, .m_Orientation = 1});
    level.m_TrileEmplacements.push_back({.m_Id = -1, .m_Position = {0.0f, 0.0f, 0.0f}}); // no geometry: skipped
    level.m_TrileEmplacements.push_back({.m_Id = 7, .m_Position = {7.0f, 8.0f, 9.0f}, .m_Orientation = 2});

    fez::BackgroundPlane plane;
    plane.m_Name = "plane";
    plane.m_Position = {0.5f, 1.5f, 2.5f};
    plane.m_Scale = {1.0f, 1.0f, 1.0f};
    plane.m_Geometry = triangle("plane", "plane.png");
    level.m_BackgroundPlanes.push_back(plane);

    fez::Character npc;
    npc.m_Name = "npc";
    npc.m_Position = {3.0f, 3.0f, 3.0f};
    npc.m_Geometry = triangle("npc/idle.ani.png", "npc/idle.ani.png");
    level.m_Characters.push_back(npc);

    const auto composed = fez::buildLevelScene(level);
    REQUIRE(composed.m_Scene != nullptr);
    const auto& scene = *composed.m_Scene;

    // one mesh and one material per instance, no deduplication of the two trile 7 instances
    CHECK(scene.mNumMeshes == 5);
    CHECK(scene.mNumMaterials == 5);
    REQUIRE(scene.mRootNode->mNumChildren == 5);

    CHECK(nodeName(scene, 0) == "ao_a");
    CHECK(nodeName(scene, 1) == "7");
    CHECK(nodeName(scene, 2) == "7");
    CHECK(nodeName(scene, 3) == "plane");
    CHECK(nodeName(scene, 4) == "npc");

    for(unsigned int i = 0; i < 5; ++i)
        CHECK(nodeMesh(scene, i) == i);

    const auto& ao = level.m_ArtObjects[0];
    CHECK(sameMatrix(scene.mRootNode->mChildren[0]->mTransformation,
                     expectedMatrix(fez::withLevelObjectOffset(ao.m_Position), ao.m_Rotation, ao.m_Scale)));
    CHECK(sameMatrix(scene.mRootNode->mChildren[1]->mTransformation,
                     expectedMatrix({4.0f, 5.0f, 6.0f}, fez::trileOrientation(1), fez::Vec3f::Ones())));
    CHECK(sameMatrix(scene.mRootNode->mChildren[2]->mTransformation,
                     expectedMatrix({7.0f, 8.0f, 9.0f}, fez::trileOrientation(2), fez::Vec3f::Ones())));
    CHECK(sameMatrix(scene.mRootNode->mChildren[3]->mTransformation,
                     expectedMatrix(fez::withLevelObjectOffset(plane.m_Position), plane.m_Rotation, plane.m_Scale)));
    CHECK(sameMatrix(scene.mRootNode->mChildren[4]->mTransformation,
                     expectedMatrix(npc.m_Position, fez::QuaternionF::Identity(), fez::Vec3f::Ones())));

    // one texture entry per mesh, duplicates included (the copier copies each destination once)
    REQUIRE(composed.m_Textures.size() == 5);
    CHECK(composed.m_Textures[0].m_Name == "ao_a.png");
    CHECK(composed.m_Textures[1].m_Name == "set.png");
    CHECK(composed.m_Textures[2].m_Name == "set.png");
    CHECK(composed.m_Textures[3].m_Name == "plane.png");
    CHECK(composed.m_Textures[4].m_Name == "npc/idle.ani.png");
    CHECK(composed.m_Textures[4].m_Source == "in/npc/idle.ani.png");
}

TEST_CASE("buildLevelScene: an instance with empty geometry adds no mesh, no node and no texture", "[export][scene]")
{
    fez::Level level;
    level.m_LevelName = "LEVEL";

    auto empty = triangle("empty", "empty.png");
    empty.m_Indices.clear();

    fez::Character npc;
    npc.m_Name = "npc";
    npc.m_Geometry = triangle("npc.png", "npc.png");

    fez::BackgroundPlane plane;
    plane.m_Name = "empty_plane";
    plane.m_Geometry = empty;

    level.m_BackgroundPlanes.push_back(plane);
    level.m_Characters.push_back(npc);

    const auto composed = fez::buildLevelScene(level);
    const auto& scene = *composed.m_Scene;

    // the empty plane adds no mesh and no material (Task 29)
    CHECK(scene.mNumMeshes == 1);
    CHECK(scene.mNumMaterials == 1);
    REQUIRE(scene.mRootNode->mNumChildren == 1);
    CHECK(nodeName(scene, 0) == "npc");
    CHECK(nodeMesh(scene, 0) == 0);

    REQUIRE(composed.m_Textures.size() == 1);
    CHECK(composed.m_Textures[0].m_Name == "npc.png");
}

TEST_CASE("buildLevelScene: an empty level gives a scene without meshes", "[export][scene]")
{
    fez::Level level;
    level.m_LevelName = "EMPTY";

    const auto composed = fez::buildLevelScene(level);

    REQUIRE(composed.m_Scene != nullptr);
    CHECK(composed.m_Scene->mNumMeshes == 0);
    CHECK(composed.m_Scene->mRootNode->mNumChildren == 0);
    CHECK(composed.m_Textures.empty());
}
