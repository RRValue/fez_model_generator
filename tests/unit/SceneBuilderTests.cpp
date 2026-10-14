#include "export/SceneBuilder.h"
#include "export/Transforms.h"

#include "model/Geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QtCore/QString>
#include <QtCore/QTemporaryDir>

#include <QtGui/QColor>
#include <QtGui/QImage>

#include <assimp/material.h>
#include <assimp/scene.h>

#include <numbers>
#include <string>

// SceneBuilder collects meshes, materials and nodes and hands them to one aiScene. It must produce exactly the scene
// the old Writer::addGeometry built: one mesh and one material per added geometry (no deduplication), faces with the
// winding (0, 2, 1), and an opacity texture only for planes whose texture has translucent pixels.

namespace
{

fez::Geometry triangle(const QString& name = "tri", const QString& texture = "tex.png")
{
    fez::Geometry geometry;
    geometry.m_Name = name;
    geometry.m_Vertices = {
        {.m_Position = {0.0f, 0.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {0.0f, 0.0f}},
        {.m_Position = {1.0f, 0.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {1.0f, 0.0f}},
        {.m_Position = {0.0f, 2.0f, 0.5f}, .m_Normal = {0.0f, 1.0f, 0.0f}, .m_TextureCoordinate = {0.25f, 0.75f}},
    };
    geometry.m_Indices = {0, 1, 2};
    geometry.m_Texture.m_TextureName = texture;
    geometry.m_Opacity = 0.5f;
    geometry.m_DoubleSided = false;
    return geometry;
}

std::string materialName(const aiMaterial& material)
{
    aiString name;

    if(material.Get(AI_MATKEY_NAME, name) != aiReturn_SUCCESS)
        return "<none>";

    return name.C_Str();
}

std::string textureOf(const aiMaterial& material, aiTextureType type)
{
    aiString path;

    if(material.Get(AI_MATKEY_TEXTURE(type, 0), path) != aiReturn_SUCCESS)
        return "<none>";

    return path.C_Str();
}

bool sameMatrix(const aiMatrix4x4& a, const aiMatrix4x4& b)
{
    for(unsigned int row = 0; row < 4; ++row)
        for(unsigned int column = 0; column < 4; ++column)
            if(a[row][column] != b[row][column])
                return false;

    return true;
}

QString writeImage(const QTemporaryDir& dir, const QString& name, bool translucent)
{
    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(QColor(10, 20, 30, 255));

    if(translucent)
        image.setPixelColor(3, 2, QColor(10, 20, 30, 128));

    const auto path = dir.filePath(name);
    REQUIRE(image.save(path, "PNG"));
    return path;
}

fez::Geometry plane(const QString& textureFile)
{
    auto geometry = triangle("plane", "plane.png");
    geometry.m_IsPlane = true;
    geometry.m_Texture.m_TextureOrgFile = textureFile;
    return geometry;
}

} // namespace

TEST_CASE("SceneBuilder: an empty builder gives an empty scene", "[export]")
{
    fez::SceneBuilder builder;
    const auto scene = builder.build();

    REQUIRE(scene != nullptr);
    CHECK(scene->mNumMeshes == 0);
    CHECK(scene->mMeshes == nullptr);
    CHECK(scene->mNumMaterials == 0);
    CHECK(scene->mMaterials == nullptr);
    REQUIRE(scene->mRootNode != nullptr);
    CHECK(scene->mRootNode->mNumChildren == 0);
    CHECK(scene->mRootNode->mChildren == nullptr);
    CHECK(scene->mRootNode->mNumMeshes == 0);
    CHECK(scene->mRootNode->mTransformation.IsIdentity());
}

TEST_CASE("SceneBuilder: addMesh copies vertices, normals and texture coordinates", "[export]")
{
    fez::SceneBuilder builder;
    const auto id = builder.addMesh(triangle());

    REQUIRE(id == 0u);

    const auto scene = builder.build();
    REQUIRE(scene->mNumMeshes == 1);

    const auto& mesh = *scene->mMeshes[0];
    CHECK(std::string(mesh.mName.C_Str()) == "tri");
    CHECK(mesh.mPrimitiveTypes == aiPrimitiveType_TRIANGLE);
    REQUIRE(mesh.mNumVertices == 3);
    CHECK(mesh.mVertices[2] == aiVector3D(0.0f, 2.0f, 0.5f));
    CHECK(mesh.mNormals[2] == aiVector3D(0.0f, 1.0f, 0.0f));
    CHECK(mesh.mTextureCoords[0][2] == aiVector3D(0.25f, 0.75f, 0.0f));
    CHECK(mesh.mMaterialIndex == 0);
}

TEST_CASE("SceneBuilder: faces get the winding (0, 2, 1)", "[export]")
{
    auto geometry = triangle();
    geometry.m_Vertices.push_back(geometry.m_Vertices[0]);
    geometry.m_Indices = {0, 1, 2, 2, 1, 3};

    fez::SceneBuilder builder;
    REQUIRE(builder.addMesh(geometry) == 0u);

    const auto scene = builder.build();
    const auto& mesh = *scene->mMeshes[0];

    REQUIRE(mesh.mNumFaces == 2);
    REQUIRE(mesh.mFaces[0].mNumIndices == 3);
    CHECK(mesh.mFaces[0].mIndices[0] == 0);
    CHECK(mesh.mFaces[0].mIndices[1] == 2);
    CHECK(mesh.mFaces[0].mIndices[2] == 1);
    REQUIRE(mesh.mFaces[1].mNumIndices == 3);
    CHECK(mesh.mFaces[1].mIndices[0] == 2);
    CHECK(mesh.mFaces[1].mIndices[1] == 3);
    CHECK(mesh.mFaces[1].mIndices[2] == 1);
}

TEST_CASE("SceneBuilder: the material carries name, diffuse texture, opacity and sidedness", "[export]")
{
    fez::SceneBuilder builder;
    REQUIRE(builder.addMesh(triangle("tri", "set.png")) == 0u);

    const auto scene = builder.build();
    REQUIRE(scene->mNumMaterials == 1);

    const auto& material = *scene->mMaterials[0];
    CHECK(materialName(material) == "set.png");
    CHECK(textureOf(material, aiTextureType_DIFFUSE) == "set.png");
    CHECK(textureOf(material, aiTextureType_OPACITY) == "<none>");

    float opacity = 0.0f;
    REQUIRE(material.Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS);
    CHECK(opacity == 0.5f);

    // m_DoubleSided is stored as a one-byte bool buffer, as before.
    const aiMaterialProperty* two_sided = nullptr;
    REQUIRE(aiGetMaterialProperty(&material, AI_MATKEY_TWOSIDED, &two_sided) == aiReturn_SUCCESS);
    REQUIRE(two_sided->mDataLength == 1);
    CHECK(two_sided->mData[0] == 0);
}

TEST_CASE("SceneBuilder: one mesh and one material per instance, without deduplication", "[export]")
{
    const auto geometry = triangle("trile", "set.png");

    fez::SceneBuilder builder;
    const auto first = builder.addMesh(geometry);
    const auto second = builder.addMesh(geometry);
    const auto third = builder.addMesh(triangle("other", "other.png"));

    REQUIRE(first == 0u);
    REQUIRE(second == 1u);
    REQUIRE(third == 2u);

    builder.addNode("a", {}, *first);
    builder.addNode("b", {}, *second);
    builder.addNode("c", {}, *third);

    const auto scene = builder.build();

    REQUIRE(scene->mNumMeshes == 3);
    REQUIRE(scene->mNumMaterials == 3);
    CHECK(scene->mRootNode->mNumChildren == 3);

    for(unsigned int i = 0; i < 3; ++i)
        CHECK(scene->mMeshes[i]->mMaterialIndex == i);

    CHECK(scene->mMeshes[0] != scene->mMeshes[1]);
    CHECK(scene->mMaterials[0] != scene->mMaterials[1]);
    CHECK(materialName(*scene->mMaterials[0]) == "set.png");
    CHECK(materialName(*scene->mMaterials[1]) == "set.png");
    CHECK(materialName(*scene->mMaterials[2]) == "other.png");
}

TEST_CASE("SceneBuilder: an empty geometry gives an error and adds no mesh and no material", "[export]")
{
    fez::SceneBuilder builder;
    REQUIRE(builder.addMesh(triangle("first")) == 0u);

    const auto no_vertices = builder.addMesh(fez::Geometry{});
    REQUIRE_FALSE(no_vertices.has_value());
    CHECK(no_vertices.error().kind == fez::ErrorKind::BadValue);

    auto vertices_only = triangle("vertices only");
    vertices_only.m_Indices.clear();
    const auto no_faces = builder.addMesh(vertices_only);
    REQUIRE_FALSE(no_faces.has_value());
    CHECK(no_faces.error().kind == fez::ErrorKind::BadValue);
    CHECK(no_faces.error().source == "vertices only");

    // the next mesh gets the next free id: nothing was allocated for the empty geometries
    REQUIRE(builder.addMesh(triangle("last")) == 1u);

    const auto scene = builder.build();
    REQUIRE(scene->mNumMeshes == 2);
    REQUIRE(scene->mNumMaterials == 2);

    CHECK(scene->mMeshes[0]->mName == aiString("first"));
    CHECK(scene->mMeshes[1]->mName == aiString("last"));
    CHECK(scene->mMeshes[1]->mMaterialIndex == 1);
    CHECK(materialName(*scene->mMaterials[1]) == "tex.png");
}

TEST_CASE("SceneBuilder: only empty geometry leaves the scene without meshes and materials", "[export]")
{
    fez::SceneBuilder builder;
    CHECK_FALSE(builder.addMesh(fez::Geometry{}).has_value());

    const auto scene = builder.build();
    CHECK(scene->mNumMeshes == 0);
    CHECK(scene->mNumMaterials == 0);
    CHECK(scene->mMeshes == nullptr);
    CHECK(scene->mMaterials == nullptr);
}

TEST_CASE("SceneBuilder: an opacity texture only for planes with translucent pixels", "[export]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto translucent = writeImage(dir, "translucent.png", true);
    const auto opaque = writeImage(dir, "opaque.png", false);

    auto not_a_plane = triangle("solid", "plane.png");
    not_a_plane.m_Texture.m_TextureOrgFile = translucent;

    fez::SceneBuilder builder;
    REQUIRE(builder.addMesh(plane(translucent)) == 0u);
    REQUIRE(builder.addMesh(plane(opaque)) == 1u);
    REQUIRE(builder.addMesh(not_a_plane) == 2u);

    const auto scene = builder.build();
    REQUIRE(scene->mNumMaterials == 3);

    CHECK(textureOf(*scene->mMaterials[0], aiTextureType_OPACITY) == "plane.png");
    CHECK(textureOf(*scene->mMaterials[1], aiTextureType_OPACITY) == "<none>");
    CHECK(textureOf(*scene->mMaterials[2], aiTextureType_OPACITY) == "<none>");
}

TEST_CASE("SceneBuilder: a plane whose texture cannot be loaded gives no id", "[export]")
{
    fez::SceneBuilder builder;
    const auto id = builder.addMesh(plane("Z:/does/not/exist/plane.png"));
    REQUIRE_FALSE(id.has_value());
    CHECK(id.error().kind == fez::ErrorKind::FileNotFound);

    const auto scene = builder.build();
    REQUIRE(scene->mNumMeshes == 1);
    REQUIRE(scene->mNumMaterials == 1);
    CHECK(materialName(*scene->mMaterials[0]) == "plane.png");
}

TEST_CASE("SceneBuilder: nodes keep their order, name, mesh and transform", "[export]")
{
    fez::SceneBuilder builder;
    const auto first = builder.addMesh(triangle("a"));
    const auto second = builder.addMesh(triangle("b"));
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    const fez::Transform transform{
        .m_Position = {1.0f, -2.0f, 3.5f},
        .m_Rotation = fez::QuaternionF(0.5f, 0.5f, -0.5f, 0.5f),
        .m_Scale = {2.0f, 3.0f, 4.0f},
    };

    builder.addNode("second", transform, *second);
    builder.addNode("first", {}, *first);

    const auto scene = builder.build();
    const auto& root = *scene->mRootNode;
    REQUIRE(root.mNumChildren == 2);

    const auto& node0 = *root.mChildren[0];
    CHECK(std::string(node0.mName.C_Str()) == "second");
    CHECK(node0.mParent == &root);
    REQUIRE(node0.mNumMeshes == 1);
    CHECK(node0.mMeshes[0] == 1);
    CHECK(sameMatrix(node0.mTransformation, aiMatrix4x4(aiVector3D(2.0f, 3.0f, 4.0f),
                                                        aiQuaternion(0.5f, 0.5f, -0.5f, 0.5f),
                                                        aiVector3D(1.0f, -2.0f, 3.5f))));

    const auto& node1 = *root.mChildren[1];
    CHECK(std::string(node1.mName.C_Str()) == "first");
    REQUIRE(node1.mNumMeshes == 1);
    CHECK(node1.mMeshes[0] == 0);
    CHECK(node1.mTransformation.IsIdentity());
    CHECK(sameMatrix(node1.mTransformation,
                     aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), aiQuaternion(), aiVector3D(0.0f, 0.0f, 0.0f))));
}

TEST_CASE("SceneBuilder: build hands everything over and leaves the builder empty", "[export]")
{
    fez::SceneBuilder builder;
    const auto id = builder.addMesh(triangle());
    REQUIRE(id.has_value());
    builder.addNode("n", {}, *id);

    const auto scene = builder.build();
    CHECK(scene->mNumMeshes == 1);

    const auto empty = builder.build();
    CHECK(empty->mNumMeshes == 0);
    CHECK(empty->mNumMaterials == 0);
    CHECK(empty->mRootNode->mNumChildren == 0);
}

TEST_CASE("trileOrientation: orientations 0 to 3 rotate about Y like the old Euler quaternions", "[export]")
{
    constexpr auto pi = std::numbers::pi_v<float>;
    const aiQuaternion expected[] = {
        aiQuaternion(pi, 0.0f, 0.0f),
        aiQuaternion(pi * -0.5f, 0.0f, 0.0f),
        aiQuaternion(0.0f, 0.0f, 0.0f),
        aiQuaternion(pi * 0.5f, 0.0f, 0.0f),
    };

    for(int orientation = 0; orientation < 4; ++orientation)
    {
        INFO("orientation " << orientation);
        const fez::QuaternionF q = fez::trileOrientation(orientation);
        const auto& e = expected[orientation];

        // Bit-exact: the node matrix is built from these values.
        CHECK(q.w() == e.w);
        CHECK(q.x() == e.x);
        CHECK(q.y() == e.y);
        CHECK(q.z() == e.z);
    }

    const auto half = std::numbers::sqrt2_v<float> * 0.5f;

    CHECK(fez::trileOrientation(0).w() == Catch::Approx(0.0f).margin(1e-6));
    CHECK(fez::trileOrientation(0).y() == Catch::Approx(1.0f));
    CHECK(fez::trileOrientation(1).w() == Catch::Approx(half));
    CHECK(fez::trileOrientation(1).y() == Catch::Approx(-half));
    CHECK(fez::trileOrientation(2).w() == 1.0f);
    CHECK(fez::trileOrientation(2).y() == 0.0f);
    CHECK(fez::trileOrientation(3).w() == Catch::Approx(half));
    CHECK(fez::trileOrientation(3).y() == Catch::Approx(half));

    for(const auto q : {fez::trileOrientation(0), fez::trileOrientation(1), fez::trileOrientation(3)})
    {
        CHECK(q.x() == 0.0f);
        CHECK(q.z() == 0.0f);
    }
}

TEST_CASE("trileOrientation: other values give the identity", "[export]")
{
    for(const int orientation : {-1, 4, 17})
    {
        INFO("orientation " << orientation);
        const fez::QuaternionF q = fez::trileOrientation(orientation);
        CHECK(q.w() == 1.0f);
        CHECK(q.x() == 0.0f);
        CHECK(q.y() == 0.0f);
        CHECK(q.z() == 0.0f);
    }
}

TEST_CASE("levelObjectOffset: art objects and planes are shifted by -0.5 on every axis", "[export]")
{
    CHECK(fez::levelObjectOffset == -0.5f);

    const fez::Vec3f position(1.0f, 2.25f, -3.0f);
    const fez::Vec3f shifted = fez::withLevelObjectOffset(position);

    CHECK(shifted.x() == 0.5f);
    CHECK(shifted.y() == 1.75f);
    CHECK(shifted.z() == -3.5f);

    // Bit-exact with the old "p - 0.5f" for values that need rounding.
    const fez::Vec3f odd(16777215.0f, 0.1f, -1.0e-8f);
    const fez::Vec3f odd_shifted = fez::withLevelObjectOffset(odd);

    CHECK(odd_shifted.x() == odd.x() - 0.5f);
    CHECK(odd_shifted.y() == odd.y() - 0.5f);
    CHECK(odd_shifted.z() == odd.z() - 0.5f);
}
